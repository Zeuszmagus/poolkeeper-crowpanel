// Nézetverem, bemenetkezelés (gyűrű + érintés gesztusok), oldalirányú átmenet, felugró üzenet.

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../app_config.h"
#include "ui.h"
#include "ui_internal.h"

namespace {

constexpr int STACK_MAX = 8;
constexpr int ANIM_MS = 180;
constexpr int SWIPE_MIN = 45;     // px: vízszintes lapozás küszöbe
constexpr int TAP_MAX = 14;       // px: koppintásnak számító mozgás
constexpr float RING_MIN_R = 100; // px: ennyinél kijjebb kezdett húzás számít körbe húzásnak
// Nézetváltás (belépés/kilépés) után ennyi ideig nem fogadunk gombnyomást, hogy a megerősítő
// nyomás pergése vagy egy túl hosszan tartott nyomás ne nyissa meg azonnal újra a nézetet.
constexpr uint32_t PRESS_LOCKOUT_MS = 1000;

struct Nav {
  View *stack[STACK_MAX];
  int depth;
  lv_obj_t *root;       // aktuális nézet tárolója
  lv_obj_t *old_root;   // kicsúszó nézet (animáció alatt)
  lv_obj_t *toast;
  uint32_t toast_until;
  uint32_t last_input;
  uint32_t last_rev;
  uint32_t last_tick;
  // érintés
  bool pressing;
  lv_point_t p0, plast;
  bool dragging_ring;
  uint32_t press_block_until;
  uint32_t touch_t0;
  // Visszatartott érintési gesztus: csak akkor hajtjuk végre, ha közben nem volt gombnyomás/forgatás.
  enum class G : uint8_t { None, Tap, Swipe } pend;
  int pend_x, pend_y, pend_dir;
  uint32_t pend_t_release, pend_t0;
} nav = {};

// A kijelző maga a nyomógomb: minden gombnyomás érintéssel is jár. Az érintést ezért kb. 0,2 s-ig
// visszatartjuk, és elvetjük, ha az érintés körül a gomb be volt nyomva vagy a gyűrű elfordult.
constexpr uint32_t TOUCH_DEFER_MS = 200;
constexpr uint32_t KNOB_GUARD_MS = 150;  // ennyivel az érintés kezdete előtti gombaktivitás is számít

bool knob_active_since(uint32_t t) {
  const uint32_t k = board_last_knob_activity_ms();
  return k != 0 && (int32_t)(k - t) >= 0;
}

void block_presses() { nav.press_block_until = millis() + PRESS_LOCKOUT_MS; }

void finish_anim() {
  if (nav.old_root) {
    lv_obj_delete(nav.old_root);  // a hozzá tartozó animációk is törlődnek
    nav.old_root = nullptr;
  }
  if (nav.root) {
    lv_anim_delete(nav.root, nullptr);
    lv_obj_set_x(nav.root, 0);
  }
}

void anim_x(lv_obj_t *o, int from, int to, bool del_on_done) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, o);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_duration(&a, ANIM_MS);
  lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) { lv_obj_set_x((lv_obj_t *)obj, v); });
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  if (del_on_done) {
    lv_anim_set_completed_cb(&a, [](lv_anim_t *an) {
      lv_obj_t *obj = (lv_obj_t *)an->var;
      if (obj == nav.old_root) nav.old_root = nullptr;
      lv_obj_delete_async(obj);
    });
  }
  lv_anim_start(&a);
}

lv_obj_t *new_root() {
  lv_obj_t *r = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(r);
  lv_obj_set_size(r, PK_SCREEN_W, PK_SCREEN_H);
  lv_obj_remove_flag(r, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return r;
}

// Új nézet felépítése és becsúsztatása
void show_top(int dir) {
  finish_anim();
  View *v = nav.stack[nav.depth - 1];
  lv_obj_t *old = nav.root;
  nav.root = new_root();
  v->build(nav.root);
  v->refresh();
  if (old) {
    if (dir == 0) {
      lv_obj_delete(old);
    } else {
      nav.old_root = old;
      anim_x(old, 0, -dir * PK_SCREEN_W, true);
      anim_x(nav.root, dir * PK_SCREEN_W, 0, false);
    }
  }
  if (nav.toast) lv_obj_move_foreground(nav.toast);
  Serial.printf("[ui] nézet: %s\n", v->name());
}

// ---- érintés: saját gesztusfelismerés a képernyő objektumon ----
void screen_touch_cb(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  lv_indev_t *indev = lv_indev_active();
  if (!indev) return;
  lv_point_t p;
  lv_indev_get_point(indev, &p);
  View *v = ui_current();
  if (!v) return;

  if (code == LV_EVENT_PRESSED) {
    nav.pressing = true;
    nav.p0 = nav.plast = p;
    nav.dragging_ring = false;
    nav.last_input = millis();
    nav.touch_t0 = millis();
  } else if (code == LV_EVENT_PRESSING && nav.pressing) {
    nav.plast = p;
    const int dx = p.x - nav.p0.x, dy = p.y - nav.p0.y;
    if (knob_active_since(nav.touch_t0 - KNOB_GUARD_MS)) return;  // gombnyomás közbeni ujjmozgás
    // Körbe húzás a számlap peremén: a jelölő (és az érték) követi az ujjat
    if (v->wants_ring_drag()) {
      const float sx = nav.p0.x - 180.0f, sy = nav.p0.y - 180.0f;
      const bool started_on_rim = sqrtf(sx * sx + sy * sy) >= RING_MIN_R;
      if (nav.dragging_ring || (started_on_rim && (abs(dx) > TAP_MAX || abs(dy) > TAP_MAX))) {
        nav.dragging_ring = true;
        float deg = atan2f((float)(p.x - 180), (float)(180 - p.y)) * 180.0f / (float)M_PI;  // 0° = 12 óra
        if (deg < 0) deg += 360.0f;
        finish_anim();
        v->on_ring_angle(deg);
        nav.last_input = millis();
      }
    }
  } else if (code == LV_EVENT_RELEASED && nav.pressing) {
    nav.pressing = false;
    nav.last_input = millis();
    const int dx = nav.plast.x - nav.p0.x, dy = nav.plast.y - nav.p0.y;
    if (nav.dragging_ring) return;
    nav.pend = Nav::G::None;
    if (abs(dx) >= SWIPE_MIN && abs(dx) > abs(dy)) {
      // A húzás csak lapoz; a felengedés nem nyit meg semmit.
      nav.pend = Nav::G::Swipe;
      nav.pend_dir = dx < 0 ? +1 : -1;
    } else if (abs(dx) <= TAP_MAX && abs(dy) <= TAP_MAX) {
      nav.pend = Nav::G::Tap;
      nav.pend_x = nav.p0.x;
      nav.pend_y = nav.p0.y;
    }
    nav.pend_t_release = millis();
    nav.pend_t0 = nav.touch_t0;
  }
}

// A visszatartott gesztus végrehajtása vagy elvetése (ui_loop hívja)
void flush_pending_touch() {
  if (nav.pend == Nav::G::None || millis() - nav.pend_t_release < TOUCH_DEFER_MS) return;
  const Nav::G g = nav.pend;
  nav.pend = Nav::G::None;
  if (knob_active_since(nav.pend_t0 - KNOB_GUARD_MS)) {
    Serial.println("[ui] érintés elvetve: gombnyomás/forgatás közben történt");
    return;
  }
  View *v = ui_current();
  if (!v) return;
  finish_anim();
  if (g == Nav::G::Swipe) v->on_swipe_h(nav.pend_dir);
  else v->on_tap(nav.pend_x, nav.pend_y);
}

}  // namespace

// ---- navigáció ----

View *ui_current() { return nav.depth ? nav.stack[nav.depth - 1] : nullptr; }

void ui_push(View *v, int dir) {
  if (nav.depth >= STACK_MAX) return;
  if (nav.depth) nav.stack[nav.depth - 1]->on_leave();
  nav.stack[nav.depth++] = v;
  show_top(dir);
  block_presses();
}

void ui_pop(int dir) {
  if (nav.depth <= 1) return;
  nav.stack[nav.depth - 1]->on_leave();
  nav.depth--;
  show_top(dir);
  block_presses();
}

void ui_home() {
  if (nav.depth == 1) return;
  for (int i = nav.depth - 1; i >= 1; --i) nav.stack[i]->on_leave();
  nav.depth = 1;
  show_top(-1);
  block_presses();
}

void ui_rebuild_current() { show_top(0); }
void ui_slide(int dir) { show_top(dir); }

const char *ui_view_name() { return ui_current() ? ui_current()->name() : "-"; }

uint32_t View::idle_timeout() const { return PK_IDLE_TO_CLOCK_MS; }

void ui_toast(const char *msg, uint32_t color, uint32_t ms) {
  lv_label_set_text(nav.toast, msg);
  lv_obj_set_style_text_color(nav.toast, C(color), 0);
  lv_obj_set_style_border_color(nav.toast, C(color), 0);
  lv_obj_remove_flag(nav.toast, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(nav.toast);
  nav.toast_until = millis() + ms;
}

uint32_t mode_color(Mode m) {
  switch (m) {
    case Mode::Auto: return col::AUTO;
    case Mode::Off: return col::OFF;
    case Mode::Manual: return col::ON;
  }
  return col::TEXT;
}

// ---- rajzolás ----

lv_obj_t *make_canvas(lv_obj_t *parent, lv_event_cb_t draw_cb, void *user) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, PK_SCREEN_W, PK_SCREEN_H);
  lv_obj_remove_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_event_cb(o, draw_cb, LV_EVENT_DRAW_MAIN, user);
  return o;
}

// A rajzolás abszolút képernyő-koordinátákban történik; a vászon (csúszás közben is) aktuális
// bal felső sarka az origó, így a nézetek 0..359 helyi koordinátákkal rajzolnak.
static int g_ox = 0, g_oy = 0;
lv_layer_t *draw_begin(lv_event_t *e) {
  lv_obj_t *o = (lv_obj_t *)lv_event_get_current_target(e);
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  g_ox = a.x1;
  g_oy = a.y1;
  return lv_event_get_layer(e);
}

void polar(int cx, int cy, float r, float deg, int *x, int *y) {
  const float a = deg * (float)M_PI / 180.0f;
  *x = cx + (int)lroundf(r * cosf(a));
  *y = cy + (int)lroundf(r * sinf(a));
}

void draw_arc(lv_layer_t *l, int cx, int cy, int r, int w, int a0, int a1, uint32_t color, lv_opa_t opa, bool rounded) {
  lv_draw_arc_dsc_t d;
  lv_draw_arc_dsc_init(&d);
  d.center.x = cx + g_ox;
  d.center.y = cy + g_oy;
  d.radius = r;
  d.width = w;
  d.start_angle = ((a0 % 360) + 360) % 360;
  d.end_angle = ((a1 % 360) + 360) % 360;
  if (a1 - a0 >= 360) {
    d.start_angle = 0;
    d.end_angle = 360;
  }
  d.color = C(color);
  d.opa = opa;
  d.rounded = rounded;
  lv_draw_arc(l, &d);
}

void draw_line(lv_layer_t *l, int x0, int y0, int x1, int y1, int w, uint32_t color, bool round) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1.x = x0 + g_ox;
  d.p1.y = y0 + g_oy;
  d.p2.x = x1 + g_ox;
  d.p2.y = y1 + g_oy;
  d.width = w;
  d.color = C(color);
  d.round_start = round;
  d.round_end = round;
  lv_draw_line(l, &d);
}

void draw_circle(lv_layer_t *l, int cx, int cy, int r, uint32_t color, lv_opa_t opa) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = LV_RADIUS_CIRCLE;
  d.bg_color = C(color);
  d.bg_opa = opa;
  lv_area_t a = {cx - r + g_ox, cy - r + g_oy, cx + r + g_ox, cy + r + g_oy};
  lv_draw_rect(l, &d, &a);
}

void draw_text(lv_layer_t *l, const lv_font_t *f, uint32_t color, const char *txt, int cx, int cy, int w) {
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.font = f;
  d.color = C(color);
  d.text = txt;
  d.text_local = 1;  // a szöveg a hívó veremben van: az LVGL lemásolja
  d.align = LV_TEXT_ALIGN_CENTER;
  const int h = lv_font_get_line_height(f);
  lv_area_t a = {cx - w / 2 + g_ox, cy - h / 2 + g_oy, cx + w / 2 + g_ox, cy + h / 2 + g_oy};
  lv_draw_label(l, &d, &a);
}

// ---- widget segédek ----

lv_obj_t *mk_label(lv_obj_t *parent, const lv_font_t *f, uint32_t color, int cx, int cy, const char *txt, int width) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, C(color), 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  if (width > 0) {
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, width);
  }
  lv_label_set_text(l, txt);
  set_label_center(l, cx, cy);
  return l;
}

void set_label(lv_obj_t *l, const char *txt) {
  if (!l) return;
  const char *cur = lv_label_get_text(l);
  if (!cur || strcmp(cur, txt) != 0) lv_label_set_text(l, txt);
}

void set_label_color(lv_obj_t *l, uint32_t color) { lv_obj_set_style_text_color(l, C(color), 0); }

void set_label_center(lv_obj_t *l, int cx, int cy) {
  lv_obj_align(l, LV_ALIGN_CENTER, cx - PK_SCREEN_W / 2, cy - PK_SCREEN_H / 2);
}

const lv_font_t *fit_font(const char *txt, int max_w, const lv_font_t *const *fonts, int n) {
  for (int i = 0; i < n; ++i) {
    lv_point_t sz;
    lv_text_get_size(&sz, txt, fonts[i], 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (sz.x <= max_w) return fonts[i];
  }
  return fonts[n - 1];
}

const lv_font_t *title_font(const char *txt) {
  // A két oldalsó nyíl (x≈15–37 és 323–345) között ~268 px szabad hely
  static const lv_font_t *const fonts[] = {&pk_font_36, &pk_font_30, &pk_font_28, &pk_font_20};
  return fit_font(txt, 268, fonts, 4);
}

lv_obj_t *mk_header(lv_obj_t *parent, const char *txt, uint32_t color) {
  char b[64];
  snprintf(b, sizeof b, LV_SYMBOL_LEFT "  %s", txt);
  return mk_label(parent, &pk_font_20, color, 180, 42, b);
}

bool tap_in_header(int x, int y) { return y < 66 && x > 80 && x < 280; }

void fmt_duration(char *buf, int n, uint32_t s) {
  const uint32_t h = s / 3600, m = (s % 3600) / 60;
  if (h) snprintf(buf, n, "%lu ó %lu p", (unsigned long)h, (unsigned long)m);
  else snprintf(buf, n, "%lu perc", (unsigned long)m);
}

// ---- publikus API ----

void ui_begin() {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, C(col::BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(scr, screen_touch_cb, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(scr, screen_touch_cb, LV_EVENT_PRESSING, nullptr);
  lv_obj_add_event_cb(scr, screen_touch_cb, LV_EVENT_RELEASED, nullptr);

  nav.toast = lv_label_create(lv_layer_top());
  lv_obj_set_style_text_font(nav.toast, &pk_font_20, 0);
  lv_obj_set_style_text_align(nav.toast, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_bg_color(nav.toast, C(0x0B2148), 0);
  lv_obj_set_style_bg_opa(nav.toast, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(nav.toast, 16, 0);
  lv_obj_set_style_pad_ver(nav.toast, 10, 0);
  lv_obj_set_style_radius(nav.toast, 22, 0);
  lv_obj_set_style_border_width(nav.toast, 2, 0);
  lv_label_set_long_mode(nav.toast, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_max_width(nav.toast, 270, 0);
  lv_obj_set_width(nav.toast, LV_SIZE_CONTENT);
  lv_obj_align(nav.toast, LV_ALIGN_BOTTOM_MID, 0, -40);
  lv_obj_add_flag(nav.toast, LV_OBJ_FLAG_HIDDEN);

  nav.last_input = millis();
  ui_push(view_clock(), 0);
}

void ui_handle_knob(KnobEvent ev) {
  nav.last_input = millis();
  if (nav.pend != Nav::G::None) {
    nav.pend = Nav::G::None;  // a gyűrű/gomb az erősebb: a vele együtt érzékelt érintés elvetve
    Serial.println("[ui] érintés elvetve: gombesemény érkezett");
  }
  const bool is_press = ev == KnobEvent::Click || ev == KnobEvent::DoubleClick || ev == KnobEvent::LongPress;
  if (is_press && (int32_t)(millis() - nav.press_block_until) < 0) {
    Serial.printf("[ui] %s figyelmen kívül (kivárási idő)\n", knob_event_name(ev));
    return;
  }
  finish_anim();  // az animáció közben is azonnal reagálunk
  if (View *v = ui_current()) v->on_knob(ev);
}

void ui_loop() {
  const uint32_t now = millis();
  flush_pending_touch();
  View *v = ui_current();
  if (!v) return;
  if (g_state.rev != nav.last_rev || now - nav.last_tick >= 500) {
    nav.last_rev = g_state.rev;
    nav.last_tick = now;
    v->refresh();
  }
  if (nav.toast_until && (int32_t)(now - nav.toast_until) >= 0) {
    nav.toast_until = 0;
    lv_obj_add_flag(nav.toast, LV_OBJ_FLAG_HIDDEN);
  }
  const uint32_t idle = v->idle_timeout();
  if (idle && nav.depth > 1 && !nav.pressing && now - nav.last_input > idle) {
    nav.last_input = now;
    ui_home();
  }
}

void ui_send_screenshot() {
  finish_anim();
  lv_refr_now(nullptr);
  lv_draw_buf_t *snap = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565);
  if (!snap) {
    Serial.println("PKSHOT ERR");
    return;
  }
  // A felső réteg (felugró üzenet) a képernyőobjektumon kívül van: külön rárajzoljuk.
  const uint32_t w = snap->header.w, h = snap->header.h, stride = snap->header.stride;
  if (!lv_obj_has_flag(nav.toast, LV_OBJ_FLAG_HIDDEN)) {
    lv_draw_buf_t *t = lv_snapshot_take(nav.toast, LV_COLOR_FORMAT_RGB565);
    if (t) {
      lv_area_t a;
      lv_obj_get_coords(nav.toast, &a);
      for (uint32_t y = 0; y < t->header.h; ++y) {
        const int dy = a.y1 + (int)y;
        if (dy < 0 || dy >= (int)h) continue;
        for (uint32_t x = 0; x < t->header.w; ++x) {
          const int dx = a.x1 + (int)x;
          if (dx < 0 || dx >= (int)w) continue;
          memcpy(snap->data + dy * stride + dx * 2, t->data + y * t->header.stride + x * 2, 2);
        }
      }
      lv_draw_buf_destroy(t);
    }
  }
  Serial.printf("\nPKSHOT %lu %lu %lu\n", (unsigned long)w, (unsigned long)h, (unsigned long)(w * h * 2));
  Serial.flush();
  for (uint32_t y = 0; y < h; ++y) Serial.write(snap->data + y * stride, w * 2);
  Serial.flush();
  Serial.println("\nPKEND");
  lv_draw_buf_destroy(snap);
}
