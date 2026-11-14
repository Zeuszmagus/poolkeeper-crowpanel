// UI-terv 01 és 03: óra nézet (analóg / digitális), pontos idő beállítása.

#include <Arduino.h>
#include <stdio.h>

#include "../model/storage.h"
#include "ui_internal.h"

namespace {

const char *MONTHS[12] = {"január", "február", "március", "április", "május", "június",
                          "július", "augusztus", "szeptember", "október", "november", "december"};
const char *WEEKDAYS[7] = {"vasárnap", "hétfő", "kedd", "szerda", "csütörtök", "péntek", "szombat"};

// ======================= Óra nézet =======================
class ClockView : public View {
public:
  const char *name() const override { return "Óra"; }
  uint32_t idle_timeout() const override { return 0; }

  void build(lv_obj_t *root) override {
    canvas_ = make_canvas(root, draw_cb, this);
    last_sec_ = -1;
  }

  void refresh() override {
    struct tm t;
    const int sec = pk_clock_now(&t) ? t.tm_sec + t.tm_min * 60 : -2;
    if (sec != last_sec_ || g_state.face != last_face_) {
      last_sec_ = sec;
      last_face_ = g_state.face;
      lv_obj_invalidate(canvas_);
    }
  }

  void on_knob(KnobEvent ev) override {
    if (ev == KnobEvent::Click) open_menu();
    // Forgatás az óra nézetben: számlapváltás (analóg / digitális), mentve
    else if (ev == KnobEvent::Cw || ev == KnobEvent::Ccw) {
      g_state.face = g_state.face == ClockFace::Analog ? ClockFace::Digital : ClockFace::Analog;
      storage_save_face();
      pk_touch();
    }
  }
  void on_tap(int, int) override { open_menu(); }

private:
  static void open_menu() {
    // Az óráról a főképernyőre lépünk (rendszerállapot); onnan nyílik a menü.
    ui_push(view_home(), +1);
  }

  // Az aktuális félnap (délelőtt 0–12 / délután 12–24) időszakai a 12 órás számlapon, a mutatókkal egy skálán.
  // Legkülső ív: Szivattyú, eggyel beljebb: Sóbontó, utána a többi eszköz a menüsorrend szerint.
  // AUTO módban teljes színnel, egyébként halványan. A menüben elrejtett és a program nélküli eszköz
  // nem jelenik meg; a körívek szorosan egymás mellé záródnak.
  static void draw_half_arc(lv_layer_t *l, int r, int on, int off, int h0, uint32_t color, lv_opa_t opa) {
    const int a = on > h0 ? on : h0;
    const int b = off < h0 + 720 ? off : h0 + 720;
    if (b <= a) return;
    draw_arc(l, 180, 180, r, RING_W, 270 + (a - h0) / 2, 270 + (b - h0) / 2, color, opa);
  }

  static bool has_program(ChannelId id) {
    if (id != CH_SALT) return g_state.ch[id].program.count > 0;
    const SaltConfig &sc = g_state.salt;
    for (int k = 0; k < sc.count; ++k)
      if (!sc.c[k].invalid && sc.c[k].choice != SaltChoice::Skip) return true;
    return false;
  }

  static void draw_program_rings(lv_layer_t *l, bool pm) {
    const int h0 = pm ? 720 : 0;
    ChannelId order[CH_COUNT];
    int n = 0;
    order[n++] = CH_PUMP;
    order[n++] = CH_SALT;
    for (int i = 0; i < CH_COUNT; ++i) {
      const ChannelId id = (ChannelId)g_state.layout.order[i];
      if (id != CH_PUMP && id != CH_SALT) order[n++] = id;
    }
    int r = RING_R0;
    for (int i = 0; i < n; ++i) {
      const ChannelId id = order[i];
      if (!g_state.layout.visible[id] || !has_program(id)) continue;  // program nélkül nincs saját köríve
      const uint32_t color = pk_channel_accent(id);
      const lv_opa_t opa = g_state.ch[id].mode == Mode::Auto ? LV_OPA_COVER : LV_OPA_40;
      const Program &pp = g_state.ch[CH_PUMP].program;
      if (id == CH_SALT) {
        const SaltConfig &sc = g_state.salt;
        for (int k = 0; k < sc.count; ++k) {
          const SaltCycle &c = sc.c[k];
          if (c.invalid || c.choice == SaltChoice::Skip) continue;
          for (int j = 0; j < pp.count; ++j) {
            if (pp.p[j].id != c.cycle_id) continue;
            const int a = c.choice == SaltChoice::Full ? pp.p[j].on_min : c.on_min;
            const int b = c.choice == SaltChoice::Full ? pp.p[j].off_min : c.off_min;
            draw_half_arc(l, r, a, b, h0, color, opa);
          }
        }
      } else {
        const Program &pr = g_state.ch[id].program;
        for (int k = 0; k < pr.count; ++k) draw_half_arc(l, r, pr.p[k].on_min, pr.p[k].off_min, h0, color, opa);
      }
      r -= RING_STEP;
    }
  }

  static constexpr int RING_R0 = 102, RING_STEP = 6, RING_W = 3;

  static void draw_analog(lv_layer_t *l, bool valid, const struct tm &t) {
    const int cx = 180, cy = 180;
    draw_circle(l, cx, cy, 179, col::FACE);
    draw_arc(l, cx, cy, 177, 3, 0, 360, col::RING);
    draw_circle(l, cx, cy, 150, 0x0C2E5E);
    // Perc- és óraosztások
    for (int i = 0; i < 60; ++i) {
      const float a = i * 6.0f - 90.0f;
      int x0, y0, x1, y1;
      if (i % 5 == 0) {
        const bool cardinal = i % 15 == 0;
        polar(cx, cy, 170, a, &x0, &y0);
        polar(cx, cy, cardinal ? 146 : 152, a, &x1, &y1);
        draw_line(l, x0, y0, x1, y1, cardinal ? 7 : 5, i == 0 ? col::YELLOW : col::TEXT);
      } else {
        polar(cx, cy, 170, a, &x0, &y0);
        polar(cx, cy, 163, a, &x1, &y1);
        draw_line(l, x0, y0, x1, y1, 2, col::MUTED, false);
      }
    }
    // 24 órás számozás: 00:00:01 – 12:00:00 között 12/3/6/9, 12:00:01 – 24:00:00 között 24/15/18/21
    const int sod = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
    const bool pm = valid && (sod > 12 * 3600 || sod == 0);
    static const char *am_nums[4] = {"12", "3", "6", "9"};
    static const char *pm_nums[4] = {"24", "15", "18", "21"};
    for (int k = 0; k < 4; ++k) {
      int x, y;
      polar(cx, cy, 122, k * 90.0f - 90.0f, &x, &y);
      draw_text(l, &pk_font_28, col::TEXT, pm ? pm_nums[k] : am_nums[k], x, y, 60);
    }
    // Időzítő-céltábla: eszközönként egy vékony körív a saját színével
    if (valid) draw_program_rings(l, pm);
    // Visszafogott márkafelirat a számlap közepén
    draw_text(l, &pk_font_16, col::YELLOW, "PoolKeeper", cx, cy + 40, 160);

    if (!valid) {
      draw_text(l, &pk_font_20, col::YELLOW, "Állítsd be az órát", cx, cy - 44, 220);
      draw_text(l, &pk_font_16, col::MUTED, "Nyomd meg a menühöz", cx, cy - 16, 220);
      draw_circle(l, cx, cy, 6, col::YELLOW);
      return;
    }
    const float hour_a = (t.tm_hour % 12) * 30.0f + t.tm_min * 0.5f - 90.0f;
    const float min_a = t.tm_min * 6.0f + t.tm_sec * 0.1f - 90.0f;
    const float sec_a = t.tm_sec * 6.0f - 90.0f;
    int x, y, xt, yt;
    // Óramutató
    polar(cx, cy, 88, hour_a, &x, &y);
    polar(cx, cy, -14, hour_a, &xt, &yt);
    draw_line(l, xt, yt, x, y, 10, col::TEXT);
    // Percmutató
    polar(cx, cy, 136, min_a, &x, &y);
    polar(cx, cy, -18, min_a, &xt, &yt);
    draw_line(l, xt, yt, x, y, 6, col::TEXT);
    // Másodpercmutató (sárga, ellensúllyal)
    polar(cx, cy, 150, sec_a, &x, &y);
    polar(cx, cy, -30, sec_a, &xt, &yt);
    draw_line(l, xt, yt, x, y, 2, col::YELLOW);
    draw_circle(l, cx, cy, 9, col::YELLOW);
    draw_circle(l, cx, cy, 3, col::FACE);
  }

  static void draw_digital(lv_layer_t *l, bool valid, const struct tm &t) {
    const int cx = 180, cy = 180;
    draw_arc(l, cx, cy, 172, 6, 0, 360, col::RING);
    char b[48];
    if (!valid) {
      draw_text(l, &pk_font_96, col::DIM, "--:--", cx, cy - 8, 340);
      draw_text(l, &pk_font_20, col::YELLOW, "Állítsd be az órát", cx, cy + 66, 260);
      draw_text(l, &pk_font_16, col::YELLOW, "PoolKeeper", cx, 96, 200);
      return;
    }
    // Másodperc-körív
    if (t.tm_sec > 0) draw_arc(l, cx, cy, 172, 6, 270, 270 + t.tm_sec * 6, col::YELLOW, LV_OPA_COVER, true);
    snprintf(b, sizeof b, "%02d:%02d", t.tm_hour, t.tm_min);
    draw_text(l, &pk_font_96, col::TEXT, b, cx, cy - 8, 340);
    snprintf(b, sizeof b, "%s %d., %s", MONTHS[t.tm_mon], t.tm_mday, WEEKDAYS[t.tm_wday]);
    draw_text(l, &pk_font_20, col::MUTED, b, cx, cy + 66, 280);
    draw_text(l, &pk_font_16, col::YELLOW, "PoolKeeper", cx, 96, 200);
  }

  static void draw_cb(lv_event_t *e) {
    lv_layer_t *l = draw_begin(e);
    struct tm t = {};
    const bool valid = pk_clock_now(&t);
    if (g_state.face == ClockFace::Analog) draw_analog(l, valid, t);
    else draw_digital(l, valid, t);
  }

  lv_obj_t *canvas_ = nullptr;
  int last_sec_ = -1;
  ClockFace last_face_ = ClockFace::Analog;
};

// ======================= Pontos idő beállítása =======================
// Óra (00–23) → gombnyomás → perc (00–59) → gombnyomás: a teljes új idő egyszerre érvényesül.
class TimeEditView : public View {
public:
  const char *name() const override { return "Időbeállítás"; }
  uint32_t idle_timeout() const override { return PK_IDLE_EDITOR_MS; }
  bool wants_ring_drag() const override { return true; }

  void build(lv_obj_t *root) override {
    struct tm t;
    if (pk_clock_now(&t)) {
      hour_ = t.tm_hour;
      minute_ = t.tm_min;
    } else {
      hour_ = 12;
      minute_ = 0;
    }
    stage_ = 0;
    canvas_ = make_canvas(root, draw_cb, this);
    mk_header(root, "Pontos idő", col::YELLOW);
  }

  void on_knob(KnobEvent ev) override {
    switch (ev) {
      case KnobEvent::Cw: step(+1); break;
      case KnobEvent::Ccw: step(-1); break;
      // Rövid nyomás: megerősítés. Hosszú nyomás (1,5 s): megszakítás.
      case KnobEvent::Click: confirm(); break;
      case KnobEvent::DoubleClick:
      case KnobEvent::LongPress: cancel(); break;
      default: break;
    }
  }
  void on_ring_angle(float deg) override {
    int &val = stage_ == 0 ? hour_ : minute_;
    const int nv = angle_to_value(deg, stage_ == 0 ? 24 : 60);
    if (nv != val) {
      val = nv;
      lv_obj_invalidate(canvas_);
    }
  }
  void on_tap(int x, int y) override {
    if (tap_in_header(x, y)) cancel();
    else confirm();
  }

private:
  void step(int d) {
    if (stage_ == 0) hour_ = ((hour_ + d) % 24 + 24) % 24;
    else minute_ = ((minute_ + d) % 60 + 60) % 60;
    lv_obj_invalidate(canvas_);
  }
  void confirm() {
    if (stage_ == 0) {
      stage_ = 1;  // az óra ideiglenesen rögzítve
      lv_obj_invalidate(canvas_);
      return;
    }
    pk_set_local_time(hour_, minute_);
    // Nincs megerősítő üzenet: a menüben azonnal látszik az új idő.
    Serial.printf("[time] beállítva %02d:%02d\n", hour_, minute_);
    ui_pop(-1);
  }
  void cancel() {
    ui_toast("Időbeállítás megszakítva", col::MUTED);
    ui_pop(-1);
  }

  static void draw_cb(lv_event_t *e) {
    TimeEditView *v = (TimeEditView *)lv_event_get_user_data(e);
    lv_layer_t *l = draw_begin(e);
    const int cx = 180, cy = 180;
    const int n = v->stage_ == 0 ? 24 : 60;
    const int val = v->stage_ == 0 ? v->hour_ : v->minute_;
    // Rovátkák
    for (int i = 0; i < n; ++i) {
      const float a = i * 360.0f / n - 90.0f;
      const bool major = v->stage_ == 0 ? (i % 6 == 0) : (i % 5 == 0);
      int x0, y0, x1, y1;
      polar(cx, cy, 172, a, &x0, &y0);
      polar(cx, cy, major ? 156 : 163, a, &x1, &y1);
      draw_line(l, x0, y0, x1, y1, major ? 4 : 2, i <= val ? col::YELLOW : (major ? col::MUTED : col::DIM), false);
    }
    int x, y;
    polar(cx, cy, 145, val * 360.0f / n - 90.0f, &x, &y);
    draw_circle(l, x, y, 7, col::YELLOW);

    char b[16];
    draw_text(l, &pk_font_28, col::MUTED, v->stage_ == 0 ? "Óra" : "Perc", cx, 98, 200);
    snprintf(b, sizeof b, "%02d", val);
    draw_text(l, &pk_font_120, col::YELLOW, b, cx, cy + 4, 260);
    if (v->stage_ == 0) snprintf(b, sizeof b, "%02d:__", v->hour_);
    else snprintf(b, sizeof b, "%02d:%02d", v->hour_, v->minute_);
    draw_text(l, &pk_font_28, col::TEXT, b, cx, 268, 200);
    draw_text(l, &pk_font_16, col::DIM, v->stage_ == 0 ? "Húzd körbe · nyomd meg: perc" : "Húzd körbe · nyomd meg: mentés", cx, 302, 240);
  }

  lv_obj_t *canvas_ = nullptr;
  int stage_ = 0, hour_ = 0, minute_ = 0;
};

ClockView g_clock;
TimeEditView g_time_edit;

}  // namespace

View *view_clock() { return &g_clock; }
View *view_time_edit() { return &g_time_edit; }
