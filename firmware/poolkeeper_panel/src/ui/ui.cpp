#include "ui.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../app_config.h"
#include "../backend/backend.h"
#include "../model/model.h"
#include "theme.h"

namespace {

enum class PageId : uint8_t { Pool, Water, Equipment, Schedule, System };
static const char *page_title(PageId p) {
  switch (p) {
    case PageId::Pool: return "Medence";
    case PageId::Water: return "Vízértékek";
    case PageId::Equipment: return "Gépészet";
    case PageId::Schedule: return "Időzítés";
    case PageId::System: return "Rendszer";
  }
  return "?";
}

constexpr int MAX_PAGES = 5;

struct PoolPage {
  lv_obj_t *arc, *caption, *big, *sub1, *sub2, *alert;
};
struct WaterRow {
  lv_obj_t *name, *value, *unit, *age, *dot;
};
struct WaterPage {
  WaterRow rows[3];
};
struct EquipPage {
  lv_obj_t *name, *pager, *state, *cmd, *mode, *feedback, *hint;
  int sel;
};
struct SchedPage {
  lv_obj_t *out_btn, *out_lbl, *days[7], *start, *end, *save, *save_lbl, *auto_btn, *auto_lbl, *mode_lbl;
  lv_group_t *group;
  int out_idx;  // kijelölt kimenet indexe (g_state.outputs)
};
struct SysPage {
  lv_obj_t *info, *profile_lbl, *link_lbl, *fault_lbl, *fault_btn, *clock_lbl, *first;
  lv_group_t *group;
};

struct Ui {
  lv_obj_t *scr, *tv;
  lv_obj_t *tiles[MAX_PAGES];
  PageId pages[MAX_PAGES];
  int page_count;
  int cur;
  bool interact;
  // Fejléc / lábléc (a tileview fölött)
  lv_obj_t *top, *dots[MAX_PAGES], *hint, *toast;
  uint32_t toast_until;
  lv_indev_t *enc;
  int16_t enc_diff;
  uint8_t enc_press_phase;
  PoolPage pool;
  WaterPage water;
  EquipPage eq;
  SchedPage sch;
  SysPage sys;
  uint32_t last_rev;
  uint32_t last_tick;
  bool rebuild;
} u = {};

// ---------- segédfüggvények ----------

void set_text(lv_obj_t *l, const char *t) {
  if (!l) return;
  const char *cur = lv_label_get_text(l);
  if (!cur || strcmp(cur, t) != 0) lv_label_set_text(l, t);
}
void set_color(lv_obj_t *o, uint32_t c) { lv_obj_set_style_text_color(o, C(c), 0); }
void show(lv_obj_t *o, bool v) {
  if (!o) return;
  if (v) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *label(lv_obj_t *parent, const lv_font_t *f, uint32_t color, lv_align_t al, int x, int y,
                const char *txt = "") {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, C(color), 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(l, txt);
  lv_obj_align(l, al, x, y);
  return l;
}

lv_obj_t *button(lv_obj_t *parent, int w, int h, const char *txt, lv_obj_t **lbl_out, const lv_font_t *f = &pk_font_20) {
  lv_obj_t *b = lv_button_create(parent);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_radius(b, h / 2, 0);
  lv_obj_set_style_bg_color(b, C(col::SURFACE), 0);
  lv_obj_set_style_bg_color(b, C(col::TEAL_DIM), LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(b, C(col::TEAL), LV_STATE_PRESSED);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, C(col::TRACK), 0);
  lv_obj_set_style_border_color(b, C(col::TEAL), LV_STATE_CHECKED);
  lv_obj_set_style_shadow_width(b, 0, 0);
  // Fókusz (gyűrűs kijelölés) jól látható keret
  lv_obj_set_style_outline_width(b, 3, LV_STATE_FOCUS_KEY);
  lv_obj_set_style_outline_color(b, C(col::TEAL), LV_STATE_FOCUS_KEY);
  lv_obj_set_style_outline_pad(b, 2, LV_STATE_FOCUS_KEY);
  lv_obj_t *l = lv_label_create(b);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, C(col::TEXT), 0);
  lv_label_set_text(l, txt);
  lv_obj_center(l);
  if (lbl_out) *lbl_out = l;
  return b;
}

lv_obj_t *page_title_label(lv_obj_t *tile, PageId p) {
  return label(tile, &pk_font_20, col::MUTED, LV_ALIGN_TOP_MID, 0, 52, page_title(p));
}

void fmt_hhmm(char *buf, size_t n, time_t t) {
  struct tm lt;
  localtime_r(&t, &lt);
  snprintf(buf, n, "%02d:%02d", lt.tm_hour, lt.tm_min);
}

void fmt_age(char *buf, size_t n, uint32_t age_ms) {
  const uint32_t s = age_ms / 1000;
  if (s < 5) snprintf(buf, n, "most");
  else if (s < 60) snprintf(buf, n, "%lu mp-e", (unsigned long)s);
  else if (s < 3600) snprintf(buf, n, "%lu perce", (unsigned long)(s / 60));
  else snprintf(buf, n, "%lu órája", (unsigned long)(s / 3600));
}

// Magyar tizedesvessző
void fmt_dec(char *buf, size_t n, float v, int decimals) {
  snprintf(buf, n, "%.*f", decimals, v);
  for (char *p = buf; *p; ++p)
    if (*p == '.') *p = ',';
}

bool reading_stale(const Reading &r) { return !r.present || r.updated_ms == 0 || millis() - r.updated_ms > PK_STALE_AFTER_MS; }

int find_page(PageId p) {
  for (int i = 0; i < u.page_count; ++i)
    if (u.pages[i] == p) return i;
  return -1;
}

void toast(const char *msg, uint32_t color = col::TEXT) {
  set_text(u.toast, msg);
  lv_obj_set_style_text_color(u.toast, C(color), 0);
  show(u.toast, true);
  lv_obj_move_foreground(u.toast);
  u.toast_until = millis() + 2200;
}

// Az időzíthető kimenetek közötti léptetés
int next_sched_output(int from, int dir) {
  const int n = g_state.output_count;
  for (int k = 1; k <= n; ++k) {
    int i = ((from + dir * k) % n + n) % n;
    if (g_state.outputs[i].has_schedule) return i;
  }
  return from;
}

// ---------- LVGL encoder indev (Időzítés / Rendszer oldal fókuszcsoportjához) ----------

void enc_read(lv_indev_t *, lv_indev_data_t *d) {
  d->enc_diff = u.enc_diff;
  u.enc_diff = 0;
  if (u.enc_press_phase == 1) {
    d->state = LV_INDEV_STATE_PRESSED;
    u.enc_press_phase = 2;
  } else {
    d->state = LV_INDEV_STATE_RELEASED;
    u.enc_press_phase = 0;
  }
}

lv_group_t *page_group(PageId p) {
  if (p == PageId::Schedule) return u.sch.group;
  if (p == PageId::System) return u.sys.group;
  return nullptr;
}

bool page_interactive(PageId p) { return p == PageId::Equipment || p == PageId::Schedule || p == PageId::System; }

void refresh_all();
void sch_load();

void set_interact(bool on) {
  const PageId p = u.pages[u.cur];
  if (on && !page_interactive(p)) return;
  u.interact = on;
  lv_group_t *g = page_group(p);
  if (g) {
    if (on) {
      lv_indev_set_group(u.enc, g);
      lv_group_set_editing(g, false);
      // Belépéskor mindig az első elem kap fókuszt (kiszámítható kezelés)
      lv_obj_t *first = p == PageId::Schedule ? u.sch.out_btn : u.sys.first;
      if (first) lv_group_focus_obj(first);
      lv_obj_t *f = lv_group_get_focused(g);
      if (f) lv_obj_add_state(f, LV_STATE_FOCUS_KEY);
    } else {
      lv_group_set_editing(g, false);
      lv_obj_t *f = lv_group_get_focused(g);
      if (f) lv_obj_remove_state(f, LV_STATE_FOCUS_KEY | LV_STATE_FOCUSED);
      lv_indev_set_group(u.enc, nullptr);
      // Mentés nélküli kilépés: a kijelző a ténylegesen érvényes programot mutassa
      if (p == PageId::Schedule) sch_load();
    }
  }
  refresh_all();
}

void goto_page(int idx, bool anim) {
  if (idx < 0 || idx >= u.page_count) return;
  if (u.interact) set_interact(false);
  u.cur = idx;
  lv_tileview_set_tile_by_index(u.tv, idx, 0, anim ? LV_ANIM_ON : LV_ANIM_OFF);
  refresh_all();
}

void tv_changed_cb(lv_event_t *) {
  lv_obj_t *t = lv_tileview_get_tile_active(u.tv);
  for (int i = 0; i < u.page_count; ++i) {
    if (u.tiles[i] == t && i != u.cur) {
      if (u.interact) set_interact(false);
      u.cur = i;
      refresh_all();
    }
  }
}

// ---------- Medence ----------

void build_pool(lv_obj_t *t) {
  PoolPage &p = u.pool;
  p.arc = lv_arc_create(t);
  lv_obj_set_size(p.arc, 350, 350);
  lv_obj_center(p.arc);
  lv_arc_set_rotation(p.arc, 135);
  lv_arc_set_bg_angles(p.arc, 0, 270);
  lv_arc_set_range(p.arc, 0, 1000);
  lv_obj_remove_style(p.arc, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(p.arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(p.arc, 12, LV_PART_MAIN);
  lv_obj_set_style_arc_width(p.arc, 12, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(p.arc, C(col::TRACK), LV_PART_MAIN);
  lv_obj_set_style_arc_color(p.arc, C(col::TEAL), LV_PART_INDICATOR);

  p.caption = label(t, &pk_font_20, col::MUTED, LV_ALIGN_TOP_MID, 0, 88);
  p.big = label(t, &pk_font_72, col::TEXT, LV_ALIGN_TOP_MID, 0, 116);
  p.sub1 = label(t, &pk_font_20, col::TEXT, LV_ALIGN_TOP_MID, 0, 202);
  p.sub2 = label(t, &pk_font_16, col::MUTED, LV_ALIGN_TOP_MID, 0, 232);
  p.alert = label(t, &pk_font_16, col::WARN, LV_ALIGN_TOP_MID, 0, 260);
  lv_label_set_long_mode(p.alert, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(p.alert, 250);
}

void refresh_pool() {
  PoolPage &p = u.pool;
  const AppState &s = g_state;
  char b[64], b2[32];
  const time_t now = time(nullptr);
  uint32_t arc_color = col::TEAL;

  if (s.profile == Profile::Full) {
    const Reading &r = s.water_temp;
    set_text(p.caption, "Vízhőmérséklet");
    if (!r.present) {
      set_text(p.big, "–");
      set_color(p.big, col::STALE);
    } else {
      fmt_dec(b2, sizeof b2, r.value, 1);
      snprintf(b, sizeof b, "%s°", b2);
      set_text(p.big, b);
      set_color(p.big, reading_stale(r) ? col::STALE : col::TEXT);
    }
    int v = r.present ? (int)((r.value - 10.0f) / 30.0f * 1000.0f) : 0;
    lv_arc_set_value(p.arc, v < 0 ? 0 : v > 1000 ? 1000 : v);

    char ph[16], orp[16];
    if (s.ph.present) fmt_dec(ph, sizeof ph, s.ph.value, 2); else strcpy(ph, "–");
    if (s.orp.present) snprintf(orp, sizeof orp, "%d mV", (int)s.orp.value); else strcpy(orp, "–");
    snprintf(b, sizeof b, "pH %s  ·  ORP %s", ph, orp);
    set_text(p.sub1, b);

    if (!r.present) {
      set_text(p.sub2, "NINCS ADAT");
      set_color(p.sub2, col::ERR);
      arc_color = col::STALE;
    } else if (reading_stale(r)) {
      fmt_age(b2, sizeof b2, millis() - r.updated_ms);
      snprintf(b, sizeof b, "ELAVULT · %s", b2);
      set_text(p.sub2, b);
      set_color(p.sub2, col::WARN);
      arc_color = col::STALE;
    } else {
      fmt_age(b2, sizeof b2, millis() - r.updated_ms);
      snprintf(b, sizeof b, "Frissítve: %s", b2);
      set_text(p.sub2, b);
      set_color(p.sub2, col::MUTED);
    }
    if (!s.link_ok) {
      set_text(p.alert, LV_SYMBOL_WARNING " Nincs kapcsolat a vezérlővel");
      set_color(p.alert, col::WARN);
      arc_color = col::WARN;
    } else {
      set_text(p.alert, "");
    }
  } else {
    // Basic: szűrés állapota és következő kapcsolás
    const Output &o = s.outputs[0];
    set_text(p.caption, "Szűrés");
    set_text(p.big, o.on ? "BE" : "KI");
    set_color(p.big, o.on ? col::TEAL : col::MUTED);

    const Schedule &sc = s.schedules[0];
    int arc_v = 0;
    if (!s.clock_valid) {
      set_text(p.sub1, "Következő kapcsolás: –");
    } else if (o.mode == OutputMode::Manual && o.override_until) {
      fmt_hhmm(b2, sizeof b2, o.override_until);
      snprintf(b, sizeof b, "Kézi %s · %s-ig", o.on ? "BE" : "KI", b2);
      set_text(p.sub1, b);
      const int32_t total = PK_OVERRIDE_DEFAULT_MIN * 60;
      int32_t left = (int32_t)(o.override_until - now);
      arc_v = left > 0 ? (int)(1000L * left / total) : 0;
    } else {
      time_t when;
      bool to_on;
      if (pk_schedule_next_change(sc, now, &when, &to_on)) {
        fmt_hhmm(b2, sizeof b2, when);
        struct tm wt, nt;
        localtime_r(&when, &wt);
        localtime_r(&now, &nt);
        const int dd = (int)((when - now + (nt.tm_hour * 3600 + nt.tm_min * 60 + nt.tm_sec)) / 86400);
        static const char *long_days[7] = {"hétfő", "kedd", "szerda", "csütörtök", "péntek", "szombat", "vasárnap"};
        const char *day = wt.tm_yday == nt.tm_yday ? "" : dd <= 1 ? "holnap " : long_days[pk_weekday_mon0(wt)];
        snprintf(b, sizeof b, "Következő: %s %s%s%s", to_on ? "BE" : "KI", day, (dd > 1 && *day) ? " " : "", b2);
        set_text(p.sub1, b);
        if (o.on) {  // aktív ablakban az ív a hátralévő időt mutatja
          const int len = ((int)sc.end_min - (int)sc.start_min + 1440) % 1440;
          const int32_t left = (int32_t)(when - now);
          arc_v = len ? (int)(1000L * left / (len * 60L)) : 0;
        }
      } else {
        set_text(p.sub1, "Nincs ütemezett kapcsolás");
      }
    }
    lv_arc_set_value(p.arc, arc_v < 0 ? 0 : arc_v > 1000 ? 1000 : arc_v);
    set_text(p.sub2, o.mode == OutputMode::Manual ? "Kézi felülbírálás" : "Automata");
    set_color(p.sub2, o.mode == OutputMode::Manual ? col::PENDING : col::MUTED);

    if (!s.clock_valid) {
      set_text(p.alert, LV_SYMBOL_WARNING " Érvénytelen óra – időzítés felfüggesztve");
      set_color(p.alert, col::ERR);
      arc_color = col::ERR;
    } else if (!s.link_ok) {
      set_text(p.alert, LV_SYMBOL_WARNING " A kimeneti modul nem válaszol");
      set_color(p.alert, col::WARN);
      arc_color = col::WARN;
    } else {
      set_text(p.alert, "");
    }
  }
  lv_obj_set_style_arc_color(p.arc, C(arc_color), LV_PART_INDICATOR);
}

// ---------- Vízértékek (Full) ----------

void build_water(lv_obj_t *t) {
  page_title_label(t, PageId::Water);
  static const char *names[3] = {"pH", "ORP", "Víz"};
  for (int i = 0; i < 3; ++i) {
    WaterRow &r = u.water.rows[i];
    const int y = 92 + i * 66;
    r.dot = lv_obj_create(t);
    lv_obj_set_size(r.dot, 10, 10);
    lv_obj_set_style_radius(r.dot, 5, 0);
    lv_obj_set_style_border_width(r.dot, 0, 0);
    lv_obj_align(r.dot, LV_ALIGN_TOP_LEFT, 58, y + 12);
    r.name = label(t, &pk_font_20, col::MUTED, LV_ALIGN_TOP_LEFT, 76, y + 4, names[i]);
    r.value = label(t, &pk_font_28, col::TEXT, LV_ALIGN_TOP_RIGHT, -96, y);
    r.unit = label(t, &pk_font_16, col::MUTED, LV_ALIGN_TOP_LEFT, 270, y + 9);
    r.age = label(t, &pk_font_16, col::MUTED, LV_ALIGN_TOP_LEFT, 76, y + 32);
  }
}

void refresh_water_row(WaterRow &w, const Reading &r, float v, int dec, const char *unit) {
  char b[32], a[24];
  set_text(w.unit, r.present ? unit : "");
  if (!r.present) {
    set_text(w.value, "–");
    set_color(w.value, col::STALE);
    set_text(w.age, "NINCS ADAT");
    set_color(w.age, col::ERR);
    lv_obj_set_style_bg_color(w.dot, C(col::ERR), 0);
    return;
  }
  if (dec < 0) snprintf(b, sizeof b, "%d", (int)v);
  else fmt_dec(b, sizeof b, v, dec);
  set_text(w.value, b);
  fmt_age(a, sizeof a, millis() - r.updated_ms);
  if (reading_stale(r)) {
    set_color(w.value, col::STALE);
    snprintf(b, sizeof b, "ELAVULT · %s", a);
    set_text(w.age, b);
    set_color(w.age, col::WARN);
    lv_obj_set_style_bg_color(w.dot, C(col::WARN), 0);
  } else {
    set_color(w.value, col::TEXT);
    set_text(w.age, a);
    set_color(w.age, col::MUTED);
    lv_obj_set_style_bg_color(w.dot, C(col::OK), 0);
  }
}

void refresh_water() {
  const AppState &s = g_state;
  refresh_water_row(u.water.rows[0], s.ph, s.ph.value, 2, "");
  refresh_water_row(u.water.rows[1], s.orp, s.orp.value, -1, "mV");
  refresh_water_row(u.water.rows[2], s.water_temp, s.water_temp.value, 1, "°C");
}

// ---------- Gépészet ----------

void eq_request_selected() {
  Output &o = g_state.outputs[u.eq.sel];
  if (o.cmd == CmdState::Pending) {
    toast("Már folyamatban…", col::PENDING);
    return;
  }
  backend().request_output(u.eq.sel, !o.on);
}

void eq_state_clicked(lv_event_t *) { eq_request_selected(); }
void eq_name_clicked(lv_event_t *) {
  u.eq.sel = (u.eq.sel + 1) % g_state.output_count;
  refresh_all();
}

void build_equipment(lv_obj_t *t) {
  EquipPage &e = u.eq;
  page_title_label(t, PageId::Equipment);
  e.name = label(t, &pk_font_28, col::TEXT, LV_ALIGN_TOP_MID, 0, 84);
  lv_obj_add_flag(e.name, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(e.name, eq_name_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_ext_click_area(e.name, 12);
  e.pager = label(t, &pk_font_16, col::MUTED, LV_ALIGN_TOP_MID, 0, 118);
  e.state = label(t, &pk_font_72, col::TEXT, LV_ALIGN_TOP_MID, 0, 136);
  lv_obj_add_flag(e.state, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(e.state, 16);
  lv_obj_add_event_cb(e.state, eq_state_clicked, LV_EVENT_CLICKED, nullptr);
  // Állapotsorok oszlopban: a tördelt üzenet lejjebb tolja a többit, nincs átfedés.
  lv_obj_t *colm = lv_obj_create(t);
  lv_obj_remove_style_all(colm);
  lv_obj_set_size(colm, 280, LV_SIZE_CONTENT);
  lv_obj_align(colm, LV_ALIGN_TOP_MID, 0, 216);
  lv_obj_set_flex_flow(colm, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(colm, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(colm, 4, 0);
  lv_obj_remove_flag(colm, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  e.cmd = label(colm, &pk_font_20, col::PENDING, LV_ALIGN_DEFAULT, 0, 0);
  lv_label_set_long_mode(e.cmd, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(e.cmd, 250);
  e.mode = label(colm, &pk_font_16, col::MUTED, LV_ALIGN_DEFAULT, 0, 0);
  e.feedback = label(colm, &pk_font_16, col::MUTED, LV_ALIGN_DEFAULT, 0, 0);
  e.sel = 0;
}

void refresh_equipment() {
  EquipPage &e = u.eq;
  const AppState &s = g_state;
  if (e.sel >= s.output_count) e.sel = 0;
  const Output &o = s.outputs[e.sel];
  char b[96], t[16];

  snprintf(b, sizeof b, u.interact ? LV_SYMBOL_LEFT "  %s  " LV_SYMBOL_RIGHT : "%s", o.name);
  set_text(e.name, b);
  set_color(e.name, u.interact ? col::TEAL : col::TEXT);
  snprintf(b, sizeof b, "%d / %d", e.sel + 1, s.output_count);
  set_text(e.pager, b);

  set_text(e.state, o.on ? "BE" : "KI");
  set_color(e.state, o.on ? col::TEAL : col::MUTED);

  const uint32_t now = millis();
  switch (o.cmd) {
    case CmdState::Pending:
      snprintf(b, sizeof b, LV_SYMBOL_REFRESH " Kérve: %s – várakozás…", o.cmd_target ? "BE" : "KI");
      set_text(e.cmd, b);
      set_color(e.cmd, col::PENDING);
      break;
    case CmdState::Confirmed:
      if (now - o.cmd_done_ms < 4000) {
        snprintf(b, sizeof b, LV_SYMBOL_OK " Visszaigazolva: %s", o.on ? "BE" : "KI");
        set_text(e.cmd, b);
        set_color(e.cmd, col::OK);
      } else {
        set_text(e.cmd, "");
      }
      break;
    case CmdState::Failed:
      if (now - o.cmd_done_ms < 8000) {
        snprintf(b, sizeof b, LV_SYMBOL_CLOSE " %s", o.cmd_error);
        set_text(e.cmd, b);
        set_color(e.cmd, col::ERR);
      } else {
        set_text(e.cmd, "");
      }
      break;
    default:
      set_text(e.cmd, "");
  }
  // Kapcsolatvesztés jelzése, ha nincs frissebb parancsállapot a sorban
  if (!s.link_ok && lv_label_get_text(e.cmd)[0] == 0) {
    set_text(e.cmd, s.profile == Profile::Full ? LV_SYMBOL_WARNING " Nincs kapcsolat – az állapot nem friss"
                                               : LV_SYMBOL_WARNING " A kimeneti modul nem válaszol");
    set_color(e.cmd, col::WARN);
  }
  show(e.cmd, lv_label_get_text(e.cmd)[0] != 0);

  if (!o.has_schedule) {
    set_text(e.mode, "Kézi vezérlés");
  } else if (o.mode == OutputMode::Manual && o.override_until) {
    fmt_hhmm(t, sizeof t, o.override_until);
    snprintf(b, sizeof b, "Kézi felülbírálás %s-ig", t);
    set_text(e.mode, b);
  } else {
    set_text(e.mode, "Automata");
  }
  set_color(e.mode, o.mode == OutputMode::Manual ? col::PENDING : col::MUTED);

  // Fizikai működést csak visszajelző bemenet esetén írunk ki.
  if (o.has_feedback) {
    snprintf(b, sizeof b, "%s: %s", o.feedback_label, o.feedback_ok ? "van" : "nincs");
    set_text(e.feedback, b);
    set_color(e.feedback, o.feedback_ok ? col::OK : col::MUTED);
  } else {
    set_text(e.feedback, "");
  }
  show(e.feedback, o.has_feedback);
}

// ---------- Időzítés ----------

const char *time_options() {
  // 15 perces lépések: 96 opció "00:00\n00:15\n..."
  static char opts[96 * 6 + 1];
  if (!opts[0]) {
    char *p = opts;
    for (int i = 0; i < 96; ++i) p += sprintf(p, "%02d:%02d%s", i / 4, (i % 4) * 15, i < 95 ? "\n" : "");
  }
  return opts;
}

void sch_load() {
  SchedPage &p = u.sch;
  const Schedule &sc = g_state.schedules[p.out_idx];
  char b[48];
  snprintf(b, sizeof b, "%s  " LV_SYMBOL_RIGHT, g_state.outputs[p.out_idx].name);
  lv_label_set_text(p.out_lbl, b);
  for (int d = 0; d < 7; ++d) {
    if ((sc.days >> d) & 1) lv_obj_add_state(p.days[d], LV_STATE_CHECKED);
    else lv_obj_remove_state(p.days[d], LV_STATE_CHECKED);
  }
  lv_roller_set_selected(p.start, sc.start_min / 15, LV_ANIM_OFF);
  lv_roller_set_selected(p.end, sc.end_min / 15, LV_ANIM_OFF);
}

void sch_out_cb(lv_event_t *) {
  u.sch.out_idx = next_sched_output(u.sch.out_idx, +1);
  sch_load();
  refresh_all();
}

void sch_save_cb(lv_event_t *) {
  SchedPage &p = u.sch;
  Schedule sc = {};
  sc.enabled = true;
  for (int d = 0; d < 7; ++d)
    if (lv_obj_has_state(p.days[d], LV_STATE_CHECKED)) sc.days |= 1 << d;
  sc.start_min = lv_roller_get_selected(p.start) * 15;
  sc.end_min = lv_roller_get_selected(p.end) * 15;
  if (sc.days == 0) {
    toast("Válassz legalább egy napot", col::ERR);
  } else if (sc.start_min == sc.end_min) {
    toast("A kezdés és a vége nem lehet azonos", col::ERR);
  } else if (backend().save_schedule(p.out_idx, sc)) {
    toast(LV_SYMBOL_OK " Mentve", col::OK);
  } else {
    toast(g_state.link_ok ? "Mentés sikertelen" : "Nincs kapcsolat – nem mentve", col::ERR);
  }
}

void refresh_schedule();
void sch_changed_cb(lv_event_t *) { refresh_schedule(); }

void sch_auto_cb(lv_event_t *) {
  backend().request_auto(u.sch.out_idx);
  toast("Vissza automata módba", col::TEAL);
}

void build_schedule(lv_obj_t *t) {
  SchedPage &p = u.sch;
  p.group = lv_group_create();
  lv_group_set_wrap(p.group, true);
  label(t, &pk_font_20, col::MUTED, LV_ALIGN_TOP_MID, 0, 40, page_title(PageId::Schedule));

  p.out_idx = next_sched_output(-1 + g_state.output_count, +1);
  p.out_btn = button(t, 170, 36, "", &p.out_lbl);
  lv_obj_align(p.out_btn, LV_ALIGN_TOP_MID, 0, 70);
  lv_obj_add_event_cb(p.out_btn, sch_out_cb, LV_EVENT_CLICKED, nullptr);
  lv_group_add_obj(p.group, p.out_btn);

  const int dw = 40, gap = 3, total = 7 * dw + 6 * gap;
  for (int d = 0; d < 7; ++d) {
    lv_obj_t *lbl;
    p.days[d] = button(t, dw, 34, pk_weekday_short(d), &lbl, &pk_font_16);
    lv_obj_set_style_radius(p.days[d], 8, 0);
    lv_obj_add_flag(p.days[d], LV_OBJ_FLAG_CHECKABLE);
    lv_obj_align(p.days[d], LV_ALIGN_TOP_LEFT, (PK_SCREEN_W - total) / 2 + d * (dw + gap), 118);
    lv_obj_add_event_cb(p.days[d], sch_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_group_add_obj(p.group, p.days[d]);
  }

  label(t, &pk_font_16, col::MUTED, LV_ALIGN_TOP_MID, -64, 160, "Kezdés");
  label(t, &pk_font_16, col::MUTED, LV_ALIGN_TOP_MID, 64, 160, "Vége");
  lv_obj_t **rollers[2] = {&p.start, &p.end};
  for (int i = 0; i < 2; ++i) {
    lv_obj_t *r = lv_roller_create(t);
    lv_roller_set_options(r, time_options(), LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(r, 1);
    lv_obj_set_width(r, 112);
    lv_obj_set_style_text_font(r, &pk_font_28, 0);
    lv_obj_set_style_text_font(r, &pk_font_28, LV_PART_SELECTED);
    lv_obj_set_style_bg_color(r, C(col::SURFACE), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_bg_color(r, C(col::SURFACE), LV_PART_SELECTED);
    lv_obj_set_style_text_color(r, C(col::TEXT), LV_PART_SELECTED);
    lv_obj_set_style_border_color(r, C(col::TRACK), 0);
    lv_obj_set_style_outline_width(r, 3, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_color(r, C(col::TEAL), LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_pad(r, 2, LV_STATE_FOCUS_KEY);
    // Szerkesztés közben (forgatás állítja az értéket) türkiz kitöltés
    lv_obj_set_style_bg_color(r, C(col::TEAL_DIM), LV_PART_SELECTED | LV_STATE_EDITED);
    lv_obj_set_style_text_color(r, C(col::TEAL), LV_PART_SELECTED | LV_STATE_EDITED);
    lv_obj_align(r, LV_ALIGN_TOP_MID, i == 0 ? -64 : 64, 182);
    lv_obj_add_event_cb(r, sch_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_group_add_obj(p.group, r);
    *rollers[i] = r;
  }

  p.save = button(t, 150, 40, LV_SYMBOL_SAVE " Mentés", &p.save_lbl);
  lv_obj_set_style_bg_color(p.save, C(col::TEAL_DIM), 0);
  lv_obj_set_style_border_color(p.save, C(col::TEAL), 0);
  lv_obj_align(p.save, LV_ALIGN_TOP_MID, 0, 238);
  lv_obj_add_event_cb(p.save, sch_save_cb, LV_EVENT_CLICKED, nullptr);
  lv_group_add_obj(p.group, p.save);

  // Mód sor: automata módban felirat, kézi felülbíráláskor "Vissza automatára" gomb.
  p.mode_lbl = label(t, &pk_font_16, col::MUTED, LV_ALIGN_TOP_MID, 0, 288);
  lv_label_set_long_mode(p.mode_lbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(p.mode_lbl, 220);
  p.auto_btn = button(t, 200, 32, "", &p.auto_lbl, &pk_font_16);
  lv_obj_align(p.auto_btn, LV_ALIGN_TOP_MID, 0, 284);
  lv_obj_add_event_cb(p.auto_btn, sch_auto_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_style_bg_color(p.auto_btn, C(0x3A3110), 0);
  lv_obj_set_style_border_color(p.auto_btn, C(col::PENDING), 0);
  show(p.auto_btn, false);

  sch_load();
}

void refresh_schedule() {
  SchedPage &p = u.sch;
  const Output &o = g_state.outputs[p.out_idx];
  char b[64], t[16];

  // Mentetlen módosítás jelzése
  const Schedule &cur = g_state.schedules[p.out_idx];
  uint8_t days = 0;
  for (int d = 0; d < 7; ++d)
    if (lv_obj_has_state(p.days[d], LV_STATE_CHECKED)) days |= 1 << d;
  const bool dirty = days != cur.days || lv_roller_get_selected(p.start) * 15 != cur.start_min ||
                     lv_roller_get_selected(p.end) * 15 != cur.end_min;
  set_text(p.save_lbl, dirty ? LV_SYMBOL_SAVE " Mentés *" : LV_SYMBOL_SAVE " Mentés");
  lv_obj_set_style_border_color(p.save, C(dirty ? col::PENDING : col::TEAL), 0);
  lv_obj_set_style_text_color(p.save_lbl, C(dirty ? col::PENDING : col::TEXT), 0);
  const bool manual = o.mode == OutputMode::Manual && o.override_until;
  if (!g_state.clock_valid) {
    set_text(p.mode_lbl, LV_SYMBOL_WARNING " Óra érvénytelen – felfüggesztve");
    set_color(p.mode_lbl, col::ERR);
  } else if (manual) {
    fmt_hhmm(t, sizeof t, o.override_until);
    snprintf(b, sizeof b, "Kézi %s · %s-ig", o.on ? "BE" : "KI", t);
    set_text(p.mode_lbl, b);
    set_color(p.mode_lbl, col::PENDING);
  } else {
    set_text(p.mode_lbl, "Automata mód");
    set_color(p.mode_lbl, col::MUTED);
  }
  // A "vissza automatára" gomb csak kézi felülbíráláskor létezik a fókuszcsoportban.
  const bool btn_visible = !lv_obj_has_flag(p.auto_btn, LV_OBJ_FLAG_HIDDEN);
  const bool want_btn = manual && g_state.clock_valid;
  if (want_btn) set_text(p.auto_lbl, "Vissza automatára");
  if (want_btn != btn_visible) {
    show(p.auto_btn, want_btn);
    if (want_btn) lv_group_add_obj(p.group, p.auto_btn);
    else lv_group_remove_obj(p.auto_btn);
  }
  show(p.mode_lbl, !want_btn);
}

// ---------- Rendszer (demó / telepítői) ----------

void sys_profile_cb(lv_event_t *) {
  backend_switch_profile(g_state.profile == Profile::Basic ? Profile::Full : Profile::Basic);
  ui_request_rebuild();
}
void sys_link_cb(lv_event_t *) { backend().sim_set_link(!g_state.link_ok); }
void sys_fault_cb(lv_event_t *) { backend().sim_set_sensor_fault(!backend().sim_sensor_fault()); }
void sys_clock_cb(lv_event_t *) { backend().sim_set_clock_valid(!g_state.clock_valid); }

void build_system(lv_obj_t *t) {
  SysPage &p = u.sys;
  p.group = lv_group_create();
  lv_group_set_wrap(p.group, true);
  page_title_label(t, PageId::System);
  p.info = label(t, &pk_font_16, col::MUTED, LV_ALIGN_TOP_MID, 0, 80);

  struct Item {
    lv_obj_t **lbl;
    lv_event_cb_t cb;
    lv_obj_t **btn;
  } items[4] = {
      {&p.profile_lbl, sys_profile_cb, nullptr},
      {&p.link_lbl, sys_link_cb, nullptr},
      {&p.clock_lbl, sys_clock_cb, nullptr},
      {&p.fault_lbl, sys_fault_cb, &p.fault_btn},
  };
  const int n = g_state.profile == Profile::Full ? 4 : 3;
  for (int i = 0; i < n; ++i) {
    lv_obj_t *b = button(t, 236, 38, "", items[i].lbl, &pk_font_16);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 108 + i * 46);
    lv_obj_add_event_cb(b, items[i].cb, LV_EVENT_CLICKED, nullptr);
    lv_group_add_obj(p.group, b);
    if (i == 0) p.first = b;
    if (items[i].btn) *items[i].btn = b;
  }
  label(t, &pk_font_16, col::STALE, LV_ALIGN_TOP_MID, 0, n == 4 ? 296 : 260, "Szimulációs kapcsolók (M1)");
}

void refresh_system() {
  SysPage &p = u.sys;
  const AppState &s = g_state;
  char b[64];
  snprintf(b, sizeof b, "PoolKeeper v" PK_FW_VERSION);
  set_text(p.info, b);
  set_text(p.profile_lbl, s.profile == Profile::Full ? "Profil: Full (Pi)" : "Profil: Basic");
  snprintf(b, sizeof b, "%s: %s", s.profile == Profile::Full ? "Pi-kapcsolat" : "Kimeneti modul",
           s.link_ok ? "rendben" : "MEGSZAKADT");
  set_text(p.link_lbl, b);
  set_color(p.link_lbl, s.link_ok ? col::TEXT : col::WARN);
  set_text(p.clock_lbl, s.clock_valid ? "Óra: érvényes" : "Óra: ÉRVÉNYTELEN");
  set_color(p.clock_lbl, s.clock_valid ? col::TEXT : col::ERR);
  if (p.fault_lbl) {
    set_text(p.fault_lbl, backend().sim_sensor_fault() ? "ORP-érzékelő: HIBA" : "ORP-érzékelő: rendben");
    set_color(p.fault_lbl, backend().sim_sensor_fault() ? col::ERR : col::TEXT);
  }
}

// ---------- közös fejléc / lábléc ----------

void refresh_chrome() {
  const AppState &s = g_state;
  char b[48], t[16];
  if (s.clock_valid) fmt_hhmm(t, sizeof t, time(nullptr));
  else strcpy(t, "--:--");
  const char *icon = s.link_ok ? "" : "  " LV_SYMBOL_WARNING;
  snprintf(b, sizeof b, "%s%s", t, icon);
  set_text(u.top, b);
  set_color(u.top, s.link_ok ? (s.clock_valid ? col::TEXT : col::ERR) : col::WARN);

  for (int i = 0; i < u.page_count; ++i) {
    lv_obj_t *d = u.dots[i];
    const bool act = i == u.cur;
    lv_obj_set_size(d, act ? 18 : 7, 7);
    lv_obj_set_style_bg_color(d, C(act ? col::TEAL : col::TRACK), 0);
    show(d, !u.interact);
  }
  const PageId p = u.pages[u.cur];
  if (u.interact) {
    // Az Időzítés oldalon az alsó sáv foglalt; ott a fókuszkeret jelzi a kezelő módot.
    set_text(u.hint, p == PageId::Schedule ? "" : "Dupla nyomás: vissza");
  } else if (page_interactive(p) && p != PageId::Schedule) {
    set_text(u.hint, "Nyomd meg a kezeléshez");
  } else {
    set_text(u.hint, "");
  }
  // Az alsó tipp a Gépészet oldalon ütközne a feliratokkal: ott fentebb kerül.
  lv_obj_align(u.hint, LV_ALIGN_BOTTOM_MID, 0, u.interact ? -22 : -36);

  // LED-gyűrű: állapotjelzés
  bool pending = false;
  for (int i = 0; i < s.output_count; ++i) pending |= s.outputs[i].cmd == CmdState::Pending;
  if (!s.link_ok || !s.clock_valid) board_ring_color(0x401800);
  else if (pending) board_ring_color(0x303000);
  else board_ring_color(0);
}

void refresh_all() {
  refresh_chrome();
  for (int i = 0; i < u.page_count; ++i) {
    switch (u.pages[i]) {
      case PageId::Pool: refresh_pool(); break;
      case PageId::Water: refresh_water(); break;
      case PageId::Equipment: refresh_equipment(); break;
      case PageId::Schedule: refresh_schedule(); break;
      case PageId::System: refresh_system(); break;
    }
  }
}

void build() {
  u.scr = lv_screen_active();
  lv_obj_clean(u.scr);
  if (u.sch.group) lv_group_delete(u.sch.group);
  if (u.sys.group) lv_group_delete(u.sys.group);
  u.sch = {};
  u.sys = {};
  u.pool = {};
  u.water = {};
  const int eq_sel = u.eq.sel;
  u.eq = {};
  u.eq.sel = eq_sel;
  u.interact = false;

  lv_obj_set_style_bg_color(u.scr, C(col::BG), 0);
  lv_obj_set_style_bg_opa(u.scr, LV_OPA_COVER, 0);
  lv_obj_set_style_text_font(u.scr, &pk_font_20, 0);

  u.page_count = 0;
  u.pages[u.page_count++] = PageId::Pool;
  if (g_state.profile == Profile::Full) u.pages[u.page_count++] = PageId::Water;
  u.pages[u.page_count++] = PageId::Equipment;
  u.pages[u.page_count++] = PageId::Schedule;
  u.pages[u.page_count++] = PageId::System;

  u.tv = lv_tileview_create(u.scr);
  lv_obj_set_size(u.tv, PK_SCREEN_W, PK_SCREEN_H);
  lv_obj_set_style_bg_color(u.tv, C(col::BG), 0);
  lv_obj_set_scrollbar_mode(u.tv, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_event_cb(u.tv, tv_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  for (int i = 0; i < u.page_count; ++i) {
    lv_dir_t dir = LV_DIR_NONE;
    if (i > 0) dir = (lv_dir_t)(dir | LV_DIR_LEFT);
    if (i < u.page_count - 1) dir = (lv_dir_t)(dir | LV_DIR_RIGHT);
    lv_obj_t *t = lv_tileview_add_tile(u.tv, i, 0, dir);
    lv_obj_set_scrollbar_mode(t, LV_SCROLLBAR_MODE_OFF);
    u.tiles[i] = t;
    switch (u.pages[i]) {
      case PageId::Pool: build_pool(t); break;
      case PageId::Water: build_water(t); break;
      case PageId::Equipment: build_equipment(t); break;
      case PageId::Schedule: build_schedule(t); break;
      case PageId::System: build_system(t); break;
    }
  }

  // Fejléc: óra + kapcsolat ikon
  u.top = label(u.scr, &pk_font_20, col::TEXT, LV_ALIGN_TOP_MID, 0, 14);
  // Lábléc: oldaljelző pontok
  const int n = u.page_count;
  for (int i = 0; i < n; ++i) {
    lv_obj_t *d = lv_obj_create(u.scr);
    lv_obj_remove_flag(d, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_border_width(d, 0, 0);
    lv_obj_set_style_radius(d, 4, 0);
    lv_obj_set_style_pad_all(d, 0, 0);
    lv_obj_align(d, LV_ALIGN_BOTTOM_MID, (i - (n - 1) / 2.0f) * 16, -16);
    u.dots[i] = d;
  }
  u.hint = label(u.scr, &pk_font_16, col::STALE, LV_ALIGN_BOTTOM_MID, 0, -36);
  u.toast = label(u.scr, &pk_font_20, col::TEXT, LV_ALIGN_TOP_MID, 0, 44);
  lv_obj_set_style_bg_color(u.toast, C(0x0B1316), 0);
  lv_obj_set_style_bg_opa(u.toast, LV_OPA_90, 0);
  lv_obj_set_style_pad_hor(u.toast, 14, 0);
  lv_obj_set_style_pad_ver(u.toast, 8, 0);
  lv_obj_set_style_radius(u.toast, 18, 0);
  lv_obj_set_style_border_width(u.toast, 1, 0);
  lv_obj_set_style_border_color(u.toast, C(col::TRACK), 0);
  lv_label_set_long_mode(u.toast, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_max_width(u.toast, 260, 0);
  show(u.toast, false);

  u.cur = 0;
  lv_tileview_set_tile_by_index(u.tv, 0, 0, LV_ANIM_OFF);
  refresh_all();
}

}  // namespace

// ---------- publikus API ----------

void ui_begin() {
  lv_display_t *disp = board_display();
  lv_theme_t *th = lv_theme_default_init(disp, C(col::TEAL), C(col::WARN), true, &pk_font_20);
  lv_display_set_theme(disp, th);

  u.enc = lv_indev_create();
  lv_indev_set_type(u.enc, LV_INDEV_TYPE_ENCODER);
  lv_indev_set_read_cb(u.enc, enc_read);
  lv_indev_set_display(u.enc, disp);
  build();
}

void ui_request_rebuild() { u.rebuild = true; }

void ui_loop() {
  if (u.rebuild) {
    u.rebuild = false;
    build();
    toast(g_state.profile == Profile::Full ? "Profil: Full" : "Profil: Basic", col::TEAL);
    u.last_rev = g_state.state_rev;
  }
  const uint32_t now = millis();
  if (g_state.state_rev != u.last_rev || now - u.last_tick >= 1000) {
    u.last_rev = g_state.state_rev;
    u.last_tick = now;
    refresh_all();
  }
  if (u.toast_until && (int32_t)(now - u.toast_until) >= 0) {
    u.toast_until = 0;
    show(u.toast, false);
  }
}

void ui_handle_knob(KnobEvent ev) {
  const PageId p = u.pages[u.cur];
  if (!u.interact) {
    switch (ev) {
      case KnobEvent::Cw: goto_page((u.cur + 1) % u.page_count, true); break;
      case KnobEvent::Ccw: goto_page((u.cur + u.page_count - 1) % u.page_count, true); break;
      case KnobEvent::Click: set_interact(true); break;
      case KnobEvent::LongPress:
      case KnobEvent::DoubleClick: goto_page(0, true); break;
      default: break;
    }
    return;
  }
  // Kezelő mód
  if (ev == KnobEvent::DoubleClick || (ev == KnobEvent::LongPress && p != PageId::Equipment)) {
    set_interact(false);
    return;
  }
  if (p == PageId::Equipment) {
    const int n = g_state.output_count;
    switch (ev) {
      case KnobEvent::Cw: u.eq.sel = (u.eq.sel + 1) % n; break;
      case KnobEvent::Ccw: u.eq.sel = (u.eq.sel + n - 1) % n; break;
      case KnobEvent::Click: eq_request_selected(); break;
      case KnobEvent::LongPress:
        if (g_state.outputs[u.eq.sel].mode == OutputMode::Manual) {
          backend().request_auto(u.eq.sel);
          toast("Vissza automata módba", col::TEAL);
        }
        break;
      default: break;
    }
    refresh_all();
    return;
  }
  // Fókuszcsoportos oldalak: az LVGL encoder indev kapja
  switch (ev) {
    case KnobEvent::Cw: u.enc_diff++; break;
    case KnobEvent::Ccw: u.enc_diff--; break;
    case KnobEvent::Click: u.enc_press_phase = 1; break;
    default: break;
  }
}

const char *ui_page_name() { return page_title(u.pages[u.cur]); }

void ui_goto_page(int idx) { goto_page(idx, false); }

void ui_send_screenshot() {
  lv_refr_now(nullptr);
  lv_draw_buf_t *snap = lv_snapshot_take(u.scr, LV_COLOR_FORMAT_RGB565);
  if (!snap) {
    Serial.println("PKSHOT ERR");
    return;
  }
  const uint32_t w = snap->header.w, h = snap->header.h, stride = snap->header.stride;
  Serial.printf("\nPKSHOT %lu %lu %lu\n", (unsigned long)w, (unsigned long)h, (unsigned long)(w * h * 2));
  Serial.flush();
  for (uint32_t y = 0; y < h; ++y) {
    Serial.write(snap->data + y * stride, w * 2);
  }
  Serial.flush();
  Serial.println("\nPKEND");
  lv_draw_buf_destroy(snap);
}
