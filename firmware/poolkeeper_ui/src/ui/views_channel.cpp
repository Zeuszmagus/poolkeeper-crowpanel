// UI-terv 04, 05, 06, 10: csatorna háromállású kezelése, napi időprogram, sóbontó ciklusai.

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "../backend/controller.h"
#include "ui_internal.h"

namespace {

// ============ közös: 24 órás körszámlap ============
struct DialSeg {
  int on, off;  // perc
  uint32_t color;
  int width;
  int radius;
};

void draw_day_dial(lv_layer_t *l, const DialSeg *segs, int n) {
  const int cx = 180, cy = 180;
  draw_arc(l, cx, cy, 146, 20, 0, 360, col::RING);
  // Órarovátkák, 0/6/12/18 kiemelve
  for (int h = 0; h < 24; ++h) {
    const float a = h * 15.0f - 90.0f;
    int x0, y0, x1, y1;
    polar(cx, cy, 174, a, &x0, &y0);
    polar(cx, cy, h % 6 == 0 ? 163 : 168, a, &x1, &y1);
    draw_line(l, x0, y0, x1, y1, h % 6 == 0 ? 3 : 2, h % 6 == 0 ? col::MUTED : col::DIM, false);
  }
  static const char *lbl[4] = {"0", "6", "12", "18"};
  for (int k = 0; k < 4; ++k) {
    int x, y;
    polar(cx, cy, 120, k * 90.0f - 90.0f, &x, &y);
    if (k != 0) draw_text(l, &pk_font_16, col::DIM, lbl[k], x, y, 40);  // a "0" helyén a cím áll
  }
  for (int i = 0; i < n; ++i) {
    const DialSeg &s = segs[i];
    draw_arc(l, cx, cy, s.radius, s.width, (int)min_to_deg(s.on), (int)min_to_deg(s.off), s.color, LV_OPA_COVER);
  }
  // Aktuális idő jelölése
  const int now = pk_now_min();
  if (now >= 0) {
    int x0, y0, x1, y1;
    polar(cx, cy, 162, min_to_deg(now), &x0, &y0);
    polar(cx, cy, 136, min_to_deg(now), &x1, &y1);
    draw_line(l, x0, y0, x1, y1, 3, col::TEXT);
  }
}

// ============ közös: BE–KI időpár szerkesztő ============
// Sorrend: BE óra → BE perc → KI óra → KI perc. Képernyőnként egy nagy szám és mezőfelirat.
struct PairEditor {
  int v[4];      // -1 = nincs megadva
  int field;
  const char *err;
  int lo, hi;    // megengedett tartomány (sóbontó: a ciklus határai), egyébként 0..1439

  void start(int on_min, int off_min, int lo_ = 0, int hi_ = 1439) {
    v[0] = on_min >= 0 ? on_min / 60 : -1;
    v[1] = on_min >= 0 ? on_min % 60 : -1;
    v[2] = off_min >= 0 ? off_min / 60 : -1;
    v[3] = off_min >= 0 ? off_min % 60 : -1;
    field = 0;
    err = nullptr;
    lo = lo_;
    hi = hi_;
  }
  int on_min() const { return v[0] < 0 || v[1] < 0 ? -1 : v[0] * 60 + v[1]; }
  int off_min() const { return v[2] < 0 || v[3] < 0 ? -1 : v[2] * 60 + v[3]; }

  void step(int d) {
    const int mod = field % 2 == 0 ? 24 : 60;
    int &x = v[field];
    if (x < 0) {
      // Üres mezőnél az első lépés ésszerű kezdőértéket ad
      if (field == 0) x = lo > 0 ? lo / 60 : 8;
      else if (field == 2) x = v[0] >= 0 ? (v[0] + 1 > 23 ? 23 : v[0] + 1) : 12;
      else x = 0;
    } else {
      x = ((x + d) % mod + mod) % mod;
    }
    err = nullptr;
  }

  // Körbe húzás: a mező értéke a jelölő helyzetéből (órák: 24, percek: 60 osztás)
  void set_from_angle(float deg) {
    v[field] = angle_to_value(deg, field % 2 == 0 ? 24 : 60);
    err = nullptr;
  }

  // true: kész, érvényes időszak
  bool next() {
    if (v[field] < 0) {
      err = "Add meg a be- és kikapcsolás idejét";
      return false;
    }
    err = nullptr;
    if (field < 3) {
      field++;
      // Üres KI mezőnél azonnali kezdőérték: KI óra = BE óra + 1 (legfeljebb 23), KI perc = 00
      if (field == 2 && v[2] < 0) v[2] = v[0] + 1 > 23 ? 23 : v[0] + 1;
      if (field == 3 && v[3] < 0) v[3] = 0;
      return false;
    }
    if (on_min() < 0 || off_min() < 0) {
      err = "Add meg a be- és kikapcsolás idejét";
      field = on_min() < 0 ? 0 : 2;
      return false;
    }
    if (off_min() <= on_min()) {
      err = "A kikapcsolás ideje legyen későbbi";
      field = 2;
      return false;
    }
    if (on_min() < lo || off_min() > hi) {
      err = "A sóbontás a ciklus határain belül legyen";
      field = 0;
      return false;
    }
    return true;
  }

  void draw(lv_layer_t *l, uint32_t accent) const {
    const int cx = 180, cy = 180;
    static const char *names[4] = {"BE óra", "BE perc", "KI óra", "KI perc"};
    const int n = field % 2 == 0 ? 24 : 60;
    const int val = v[field];
    for (int i = 0; i < n; ++i) {
      const float a = i * 360.0f / n - 90.0f;
      const bool major = n == 24 ? i % 6 == 0 : i % 5 == 0;
      int x0, y0, x1, y1;
      polar(cx, cy, 172, a, &x0, &y0);
      polar(cx, cy, major ? 157 : 164, a, &x1, &y1);
      const bool lit = val >= 0 && i <= val;
      draw_line(l, x0, y0, x1, y1, major ? 4 : 2, lit ? col::YELLOW : (major ? col::MUTED : col::DIM), false);
    }
    if (val >= 0) {
      int x, y;
      polar(cx, cy, 146, val * 360.0f / n - 90.0f, &x, &y);
      draw_circle(l, x, y, 7, col::YELLOW);
    }
    draw_text(l, &pk_font_28, field < 2 ? col::ON : col::OFF, names[field], cx, 92, 220);
    char b[24];
    if (val >= 0) snprintf(b, sizeof b, "%02d", val);
    else snprintf(b, sizeof b, "--");
    draw_text(l, &pk_font_120, val >= 0 ? col::YELLOW : col::DIM, b, cx, cy + 2, 260);
    // Teljes időszak előnézete, az aktív fél kiemelve
    char on[8], off[8];
    auto part = [](char *o, int h, int m) {
      char hh[4], mm[4];
      if (h < 0) strcpy(hh, "--"); else snprintf(hh, sizeof hh, "%02d", h);
      if (m < 0) strcpy(mm, "--"); else snprintf(mm, sizeof mm, "%02d", m);
      snprintf(o, 8, "%s:%s", hh, mm);
    };
    part(on, v[0], v[1]);
    part(off, v[2], v[3]);
    draw_text(l, &pk_font_28, field < 2 ? col::YELLOW : col::MUTED, on, cx - 58, 262, 120);
    draw_text(l, &pk_font_28, col::DIM, "–", cx, 262, 30);
    draw_text(l, &pk_font_28, field >= 2 ? col::YELLOW : col::MUTED, off, cx + 58, 262, 120);
    if (err) draw_text(l, &pk_font_16, col::ERR, err, cx, 298, 240);
    else draw_text(l, &pk_font_16, col::DIM, field < 3 ? "Húzd körbe · nyomd meg: tovább" : "Húzd körbe · nyomd meg: kész", cx, 298, 240);
  }
};

// ============ Csatorna nézet: AUTO / 0 / KÉZI ============
class ChannelView : public View {
public:
  const char *name() const override { return pk_channel_name(id_); }
  void set(ChannelId id) { id_ = id; }

  // Elrendezés (minden időzíthető eszközre egységes):
  //   fent az eszköz ikonja | középen az üzemmód a két nyíl között | alatta BE / KI | legalul „Vissza”
  static constexpr int Y_ICON = 70, Y_MODE = 172, Y_STATE = 248, Y_NOTE = 282, Y_BACK = 318;

  void build(lv_obj_t *root) override {
    const uint32_t acc = pk_channel_accent(id_);
    canvas_ = make_canvas(root, draw_cb, this);
    mk_label(root, &pk_icons_56, acc, 180, Y_ICON, channel_icon(id_));
    mode_ = mk_label(root, &pk_font_72, col::TEXT, 180, Y_MODE);
    // Választható irányok: nyíl és a cél üzemmód neve a két oldalon
    left_arrow_ = mk_label(root, &pk_font_36, col::YELLOW, 34, Y_MODE - 12, LV_SYMBOL_LEFT);
    left_ = mk_label(root, &pk_font_16, col::MUTED, 40, Y_MODE + 20, "");
    right_arrow_ = mk_label(root, &pk_font_36, col::YELLOW, 326, Y_MODE - 12, LV_SYMBOL_RIGHT);
    right_ = mk_label(root, &pk_font_16, col::MUTED, 320, Y_MODE + 20, "");
    state_ = mk_label(root, &pk_font_36, col::MUTED, 180, Y_STATE);
    note_ = mk_label(root, &pk_font_16, col::WARN, 180, Y_NOTE, "", 220);
    // Vissza gomb
    lv_obj_t *back = mk_label(root, &pk_font_20, col::TEXT, 180, Y_BACK, LV_SYMBOL_LEFT "  Vissza");
    lv_obj_set_style_pad_hor(back, 16, 0);
    lv_obj_set_style_pad_ver(back, 5, 0);
    lv_obj_set_style_radius(back, 18, 0);
    lv_obj_set_style_border_width(back, 2, 0);
    lv_obj_set_style_border_color(back, C(col::DIM), 0);
    set_label_center(back, 180, Y_BACK);
  }

  void refresh() override {
    const Channel &c = g_state.ch[id_];
    const Mode m = c.mode;
    if (m == Mode::Off) {
      lv_obj_set_style_text_font(mode_, &pk_font_120, 0);
      set_label(mode_, "0");
    } else {
      lv_obj_set_style_text_font(mode_, &pk_font_72, 0);
      set_label(mode_, m == Mode::Auto ? "AUTO" : "KÉZI");
    }
    set_label_color(mode_, mode_color(m));

    // Oldalt csak a forgatással elérhető szomszédos állás: 0-ból balra AUTO, jobbra KÉZI;
    // AUTO-ból jobbra 0; KÉZI-ből balra 0.
    const char *l = m == Mode::Off ? "AUTO" : m == Mode::Manual ? "0" : "";
    const char *r = m == Mode::Off ? "KÉZI" : m == Mode::Auto ? "0" : "";
    set_label(left_, l);
    set_label_color(left_, m == Mode::Off ? col::AUTO : col::OFF);
    set_label(right_, r);
    set_label_color(right_, m == Mode::Off ? col::ON : col::OFF);
    lv_obj_set_style_text_opa(left_arrow_, l[0] ? LV_OPA_COVER : LV_OPA_20, 0);
    lv_obj_set_style_text_opa(right_arrow_, r[0] ? LV_OPA_COVER : LV_OPA_20, 0);

    // Működési állapot a kimeneti modul visszajelzése szerint
    const char *st = "KI";
    uint32_t sc = col::MUTED;
    switch (c.out) {
      case OutState::On: st = "BE"; sc = col::ON; break;
      case OutState::Off: st = "KI"; sc = col::MUTED; break;
      case OutState::PendingOn: st = "BE…"; sc = col::YELLOW; break;
      case OutState::PendingOff: st = "KI…"; sc = col::YELLOW; break;
      case OutState::Error: st = LV_SYMBOL_WARNING " HIBA"; sc = col::ERR; break;
    }
    set_label(state_, st);
    set_label_color(state_, sc);

    // Csak a fontos, biztonsági jelzések
    const char *note = "";
    uint32_t nc = col::WARN;
    if (c.out == OutState::Error) {
      note = "Nincs visszaigazolás";
      nc = col::ERR;
    } else if (id_ == CH_SALT && c.waiting_flow) {
      note = "Keringésre vár";
    } else if (id_ == CH_SALT && g_state.salt_needs_check) {
      note = LV_SYMBOL_WARNING " Időzítés ellenőrzése";
    } else if (m == Mode::Auto && !g_state.clock_valid) {
      note = "Állítsd be az órát";
      nc = col::YELLOW;
    }
    set_label(note_, note);
    set_label_color(note_, nc);
    lv_obj_invalidate(canvas_);
  }

  void on_knob(KnobEvent ev) override {
    Channel &c = g_state.ch[id_];
    switch (ev) {
      case KnobEvent::Cw:  // jobbra: AUTO → 0 → KÉZI
        if (c.mode == Mode::Auto) controller_set_mode(id_, Mode::Off);
        else if (c.mode == Mode::Off) controller_set_mode(id_, Mode::Manual);
        break;
      case KnobEvent::Ccw:  // balra: KÉZI → 0 → AUTO
        if (c.mode == Mode::Manual) controller_set_mode(id_, Mode::Off);
        else if (c.mode == Mode::Off) controller_set_mode(id_, Mode::Auto);
        break;
      case KnobEvent::Click: open_program(); break;
      case KnobEvent::DoubleClick:
      case KnobEvent::LongPress: ui_pop(-1); break;
      default: break;
    }
    refresh();
  }

  void on_tap(int x, int y) override {
    if (y > Y_BACK - 26 && x > 100 && x < 260) ui_pop(-1);           // „Vissza”
    else if (y < 104 && x > 120 && x < 240) ui_pop(-1);              // ikon (fejléc)
    else if (y > 110 && y < Y_BACK - 26 && x > 90 && x < 270) open_program();
  }

private:
  void open_program() {
    if (g_state.ch[id_].mode != Mode::Auto) {
      ui_toast("Az időprogram AUTO módban nyitható meg", col::MUTED);
      return;
    }
    ui_push(id_ == CH_SALT ? view_salt() : view_program(id_), +1);
  }

  static void draw_cb(lv_event_t *e) {
    ChannelView *v = (ChannelView *)lv_event_get_user_data(e);
    lv_layer_t *l = draw_begin(e);
    const Mode m = g_state.ch[v->id_].mode;
    const uint32_t acc = pk_channel_accent(v->id_);
    // 24 órás programgyűrű a peremen: a beállított időszakok az eszköz saját színével
    // (0 óra felül). AUTO módban teljes színnel, egyébként halványan (a program most nem vezérel).
    const int R = 172, W = 10;
    draw_arc(l, 180, 180, R, W, 0, 360, col::RING);
    const lv_opa_t opa = m == Mode::Auto ? LV_OPA_COVER : LV_OPA_50;
    if (v->id_ == CH_SALT) {
      const Program &pp = g_state.ch[CH_PUMP].program;
      for (int i = 0; i < pp.count; ++i)  // szivattyúciklusok halványan, a sóbontási ablakok az eszköz színével
        draw_arc(l, 180, 180, R, 4, (int)min_to_deg(pp.p[i].on_min), (int)min_to_deg(pp.p[i].off_min),
                 pk_channel_accent(CH_PUMP), LV_OPA_50);
      const SaltConfig &sc = g_state.salt;
      for (int i = 0; i < sc.count; ++i) {
        const SaltCycle &c = sc.c[i];
        if (c.invalid || c.choice == SaltChoice::Skip) continue;
        for (int k = 0; k < pp.count; ++k) {
          if (pp.p[k].id != c.cycle_id) continue;
          const int a = c.choice == SaltChoice::Full ? pp.p[k].on_min : c.on_min;
          const int b = c.choice == SaltChoice::Full ? pp.p[k].off_min : c.off_min;
          draw_arc(l, 180, 180, R, W, (int)min_to_deg(a), (int)min_to_deg(b), acc, opa);
        }
      }
    } else {
      const Program &pr = g_state.ch[v->id_].program;
      for (int i = 0; i < pr.count; ++i)
        draw_arc(l, 180, 180, R, W, (int)min_to_deg(pr.p[i].on_min), (int)min_to_deg(pr.p[i].off_min), acc, opa);
    }
    // 0 / 6 / 12 / 18 óra jelölők és az aktuális idő
    for (int k = 0; k < 4; ++k) {
      int x0, y0, x1, y1;
      polar(180, 180, 179, k * 90.0f - 90.0f, &x0, &y0);
      polar(180, 180, 165, k * 90.0f - 90.0f, &x1, &y1);
      draw_line(l, x0, y0, x1, y1, 2, col::MUTED, false);
    }
    const int now = pk_now_min();
    if (now >= 0) {
      int x0, y0, x1, y1;
      polar(180, 180, 179, min_to_deg(now), &x0, &y0);
      polar(180, 180, 160, min_to_deg(now), &x1, &y1);
      draw_line(l, x0, y0, x1, y1, 3, col::TEXT);
    }
  }

  ChannelId id_ = CH_PUMP;
  lv_obj_t *canvas_ = nullptr, *mode_ = nullptr, *left_ = nullptr, *right_ = nullptr, *left_arrow_ = nullptr,
           *right_arrow_ = nullptr, *state_ = nullptr, *note_ = nullptr;
};

// ============ Napi időprogram ============
class ProgramView : public View {
public:
  const char *name() const override { return "Időprogram"; }
  uint32_t idle_timeout() const override { return PK_IDLE_EDITOR_MS; }
  bool wants_ring_drag() const override { return state_ == St::Field; }
  void set(ChannelId id) {
    id_ = id;
    fresh_ = true;
  }

  void build(lv_obj_t *root) override {
    if (fresh_) {
      // Munkapéldány: a csatornát a mentett program vezérli, amíg a Mentés nem véglegesít.
      work_ = g_state.ch[id_].program;
      dirty_ = false;
      sel_ = 0;
      state_ = St::Overview;
      last_off_ = -1;
      for (int i = 0; i < work_.count; ++i)
        if (work_.p[i].off_min > last_off_) last_off_ = work_.p[i].off_min;
      fresh_ = false;
    }
    canvas_ = make_canvas(root, draw_cb, this);
    char b[48];
    snprintf(b, sizeof b, "%s", pk_channel_name(id_));
    header_ = nullptr;  // a körszámlap miatt cím a számlapon belül (draw_cb); a felső sáv koppintásra vissza
  }

  void refresh() override { lv_obj_invalidate(canvas_); }

  void on_leave() override {
    if (dirty_) ui_toast("Mentés nélkül kiléptél – a korábbi program marad", col::WARN, 2600);
    dirty_ = false;
    fresh_ = true;
  }

  void on_knob(KnobEvent ev) override {
    switch (state_) {
      case St::Overview: knob_overview(ev); break;
      case St::Action: knob_action(ev); break;
      case St::Field: knob_field(ev); break;
    }
    lv_obj_invalidate(canvas_);
  }

  void on_ring_angle(float deg) override {
    if (state_ != St::Field) return;
    ed_.set_from_angle(deg);
    lv_obj_invalidate(canvas_);
  }

  void on_tap(int x, int y) override {
    if (tap_in_header(x, y)) {
      on_knob(KnobEvent::DoubleClick);
      return;
    }
    // Áttekintőben a két oldalsó nyílra koppintva lapozás az elemek között
    if (state_ == St::Overview && (x < 70 || x > 290)) {
      on_knob(x < 70 ? KnobEvent::Ccw : KnobEvent::Cw);
      return;
    }
    if (state_ == St::Action && y > 140 && y < 260) {
      act_ = y < 184 ? 0 : y < 224 ? 1 : 2;
    }
    on_knob(KnobEvent::Click);  // érintés: közvetlen megerősítés (pl. Mentés)
  }

  // Áttekintőben vízszintes húzás: lapozás (balra húzás = következő)
  void on_swipe_h(int dir) override {
    if (state_ == St::Overview) on_knob(dir > 0 ? KnobEvent::Cw : KnobEvent::Ccw);
  }

private:
  enum class St : uint8_t { Overview, Action, Field };
  // Kijelölhető elemek: időszakok, [Új időszak], Mentés
  int item_count() const { return work_.count + (work_.count < PK_MAX_PERIODS ? 1 : 0) + 1; }
  bool is_new(int i) const { return work_.count < PK_MAX_PERIODS && i == work_.count; }
  bool is_save(int i) const { return i == item_count() - 1; }

  void knob_overview(KnobEvent ev) {
    const int n = item_count();
    switch (ev) {
      case KnobEvent::Cw: sel_ = (sel_ + 1) % n; break;
      case KnobEvent::Ccw: sel_ = (sel_ + n - 1) % n; break;
      case KnobEvent::Click:
        if (is_save(sel_)) save();
        else if (is_new(sel_)) {
          editing_ = -1;
          // Új időszak: a BE idő az utoljára beállított teljes időszak KI idejéről indul
          // Új időszak: BE óra = az előző időszak KI órája (első időszaknál 00), BE perc = 00.
          ed_.start(last_off_ >= 0 && last_off_ < 1439 ? (last_off_ / 60) * 60 : 0, -1);
          state_ = St::Field;
        } else {
          act_ = 0;
          state_ = St::Action;
        }
        break;
      case KnobEvent::DoubleClick:
      case KnobEvent::LongPress: ui_pop(-1); break;
      default: break;
    }
  }

  void knob_action(KnobEvent ev) {
    switch (ev) {
      case KnobEvent::Cw: act_ = (act_ + 1) % 3; break;
      case KnobEvent::Ccw: act_ = (act_ + 2) % 3; break;
      case KnobEvent::Click:
        if (act_ == 0) {
          editing_ = sel_;
          ed_.start(work_.p[sel_].on_min, work_.p[sel_].off_min);
          state_ = St::Field;
        } else if (act_ == 1) {
          for (int i = sel_; i < work_.count - 1; ++i) work_.p[i] = work_.p[i + 1];
          work_.count--;
          dirty_ = true;
          if (sel_ >= item_count()) sel_ = item_count() - 1;
          state_ = St::Overview;
          ui_toast("Időszak törölve – ne felejts el menteni", col::WARN);
        } else {
          state_ = St::Overview;
        }
        break;
      case KnobEvent::DoubleClick:
      case KnobEvent::LongPress: state_ = St::Overview; break;
      default: break;
    }
  }

  void knob_field(KnobEvent ev) {
    switch (ev) {
      case KnobEvent::Cw: ed_.step(+1); break;
      case KnobEvent::Ccw: ed_.step(-1); break;
      case KnobEvent::Click:
        if (ed_.next()) {
          Period p = {0, (uint16_t)ed_.on_min(), (uint16_t)ed_.off_min()};
          if (editing_ >= 0) {
            p.id = work_.p[editing_].id;  // stabil azonosító megmarad
            work_.p[editing_] = p;
          } else {
            work_.p[work_.count++] = p;
          }
          pk_program_sort(work_);
          last_off_ = p.off_min;
          dirty_ = true;
          state_ = St::Overview;
          sel_ = item_count() - 1;  // a Mentés legyen kijelölve
        }
        break;
      case KnobEvent::DoubleClick:
      case KnobEvent::LongPress: state_ = St::Overview; break;  // a félkész időszak elvetése
      default: break;
    }
  }

  void save() {
    if (controller_save_program(id_, work_)) {
      work_ = g_state.ch[id_].program;  // az azonosítókkal együtt
      dirty_ = false;
      ui_toast(LV_SYMBOL_OK " Program mentve", col::ON);
      ui_pop(-1);
    } else {
      ui_toast("Hibás időszak – a program nem menthető", col::ERR);
    }
  }

  static void draw_cb(lv_event_t *e) {
    ProgramView *v = (ProgramView *)lv_event_get_user_data(e);
    lv_layer_t *l = draw_begin(e);
    const uint32_t acc = pk_channel_accent(v->id_);
    if (v->state_ == St::Field) {
      v->ed_.draw(l, acc);
      return;
    }
    DialSeg segs[PK_MAX_PERIODS];
    for (int i = 0; i < v->work_.count; ++i) {
      const bool s = v->state_ != St::Field && i == v->sel_;
      segs[i] = {v->work_.p[i].on_min, v->work_.p[i].off_min, acc, s ? 28 : 20, 146};
    }
    draw_day_dial(l, segs, v->work_.count);
    // Kijelölt időszak: az eszköz színe marad, sárga perem jelöli
    if (v->sel_ < v->work_.count)
      draw_arc(l, 180, 180, 163, 5, (int)min_to_deg(v->work_.p[v->sel_].on_min),
               (int)min_to_deg(v->work_.p[v->sel_].off_min), col::YELLOW, LV_OPA_COVER, true);
    {
      char tb[48];
      snprintf(tb, sizeof tb, "%s · időprogram", pk_channel_name(v->id_));
      draw_text(l, &pk_font_16, acc, tb, 180, 100, 210);
    }

    char b[48], on[8], off[8];
    const int cx = 180;
    if (v->state_ == St::Action) {
      fmt_hhmm_min(on, sizeof on, v->work_.p[v->sel_].on_min);
      fmt_hhmm_min(off, sizeof off, v->work_.p[v->sel_].off_min);
      snprintf(b, sizeof b, "%s–%s", on, off);
      draw_text(l, &pk_font_20, col::TEXT, b, cx, 126, 220);
      static const char *opts[3] = {"Szerkesztés", "Törlés", "Mégse"};
      for (int i = 0; i < 3; ++i) {
        const bool s = i == v->act_;
        const int y = 164 + i * 40;
        draw_text(l, s ? &pk_font_28 : &pk_font_20, s ? (i == 1 ? col::ERR : col::YELLOW) : col::MUTED, opts[i], cx, y, 200);
      }
      return;
    }
    const int i = v->sel_;
    if (v->is_save(i)) {
      draw_text(l, &pk_font_36, col::YELLOW, LV_SYMBOL_SAVE " Mentés", cx, 168, 230);
      draw_text(l, &pk_font_16, v->dirty_ ? col::YELLOW : col::DIM,
                v->dirty_ ? "Nem mentett módosítás" : "Nincs változás", cx, 210, 220);
      if (v->work_.count < PK_MAX_PERIODS)
        draw_text(l, &pk_font_16, col::MUTED, LV_SYMBOL_LEFT " Forgasd balra: új időszak", cx, 232, 220);
    } else if (v->is_new(i)) {
      draw_text(l, &pk_font_28, col::TEXT, LV_SYMBOL_PLUS " Új időszak", cx, 168, 230);
      snprintf(b, sizeof b, "%d / %d időszak", v->work_.count, PK_MAX_PERIODS);
      draw_text(l, &pk_font_16, col::DIM, b, cx, 206, 220);
    } else {
      snprintf(b, sizeof b, "%d. időszak", i + 1);
      draw_text(l, &pk_font_16, col::MUTED, b, cx, 134, 200);
      fmt_hhmm_min(on, sizeof on, v->work_.p[i].on_min);
      fmt_hhmm_min(off, sizeof off, v->work_.p[i].off_min);
      snprintf(b, sizeof b, "%s–%s", on, off);
      draw_text(l, &pk_font_36, col::TEXT, b, cx, 172, 240);
      draw_text(l, &pk_font_16, col::DIM, "Nyomd meg: szerkesztés / törlés", cx, 210, 220);
    }
    if (v->work_.count == 0) draw_text(l, &pk_font_16, col::MUTED, "Még nincs időszak", cx, 236, 200);
    // Elemjelző pontok
    const int n = v->item_count();
    for (int k = 0; k < n; ++k) {
      const int x = cx + (k - (n - 1) / 2.0f) * 14;
      draw_circle(l, x, 258, k == i ? 5 : 3, k == i ? col::YELLOW : col::DIM);
    }
    // Lapozó nyilak (mint a menüben): forgatással vagy húzással lehet lépni az elemek között
    draw_text(l, &pk_font_28, col::YELLOW, LV_SYMBOL_LEFT, 34, 180, 30);
    draw_text(l, &pk_font_28, col::YELLOW, LV_SYMBOL_RIGHT, 326, 180, 30);
  }

  ChannelId id_ = CH_PUMP;
  Program work_ = {};
  bool dirty_ = false, fresh_ = true;
  int sel_ = 0, act_ = 0, editing_ = -1, last_off_ = -1;
  St state_ = St::Overview;
  PairEditor ed_ = {};
  lv_obj_t *canvas_ = nullptr, *header_ = nullptr;
};

// ============ Sóbontó: ciklusok kiválasztása ============
class SaltView : public View {
public:
  const char *name() const override { return "Sóbontás"; }
  uint32_t idle_timeout() const override { return PK_IDLE_EDITOR_MS; }
  bool wants_ring_drag() const override { return state_ == St::Field; }

  void build(lv_obj_t *root) override {
    if (fresh_) {
      pk_salt_revalidate();  // a lista a szivattyú mentett ciklusait követi
      work_ = g_state.salt;
      dirty_ = false;
      sel_ = 0;
      state_ = St::Overview;
      fresh_ = false;
    }
    canvas_ = make_canvas(root, draw_cb, this);
    // cím a számlapon belül (draw_cb)
  }
  void refresh() override { lv_obj_invalidate(canvas_); }
  void on_leave() override {
    if (dirty_) ui_toast("Mentés nélkül kiléptél", col::WARN);
    dirty_ = false;
    fresh_ = true;
  }

  void on_knob(KnobEvent ev) override {
    const int n = work_.count + 1;
    if (state_ == St::Overview) {
      switch (ev) {
        case KnobEvent::Cw: sel_ = (sel_ + 1) % n; break;
        case KnobEvent::Ccw: sel_ = (sel_ + n - 1) % n; break;
        case KnobEvent::Click:
          if (sel_ == work_.count) save();
          else {
            choice_ = (int)work_.c[sel_].choice;
            state_ = St::Choice;
          }
          break;
        case KnobEvent::DoubleClick:
        case KnobEvent::LongPress: ui_pop(-1); break;
        default: break;
      }
    } else if (state_ == St::Choice) {
      switch (ev) {
        case KnobEvent::Cw: choice_ = (choice_ + 1) % 3; break;
        case KnobEvent::Ccw: choice_ = (choice_ + 2) % 3; break;
        case KnobEvent::Click: {
          SaltCycle &c = work_.c[sel_];
          const Period *p = pump_period(c.cycle_id);
          if (!p) break;
          if (choice_ == (int)SaltChoice::Shorter) {
            const bool had = c.choice == SaltChoice::Shorter && !c.invalid;
            ed_.start(had ? c.on_min : p->on_min, had ? c.off_min : p->off_min, p->on_min, p->off_min);
            state_ = St::Field;
          } else {
            c.choice = (SaltChoice)choice_;
            c.invalid = false;
            dirty_ = true;
            state_ = St::Overview;
            sel_ = work_.count;
          }
          break;
        }
        case KnobEvent::DoubleClick:
        case KnobEvent::LongPress: state_ = St::Overview; break;
        default: break;
      }
    } else {
      switch (ev) {
        case KnobEvent::Cw: ed_.step(+1); break;
        case KnobEvent::Ccw: ed_.step(-1); break;
        case KnobEvent::Click:
          if (ed_.next()) {
            SaltCycle &c = work_.c[sel_];
            c.choice = SaltChoice::Shorter;
            c.on_min = ed_.on_min();
            c.off_min = ed_.off_min();
            c.invalid = false;
            dirty_ = true;
            state_ = St::Overview;
            sel_ = work_.count;
          }
          break;
        case KnobEvent::DoubleClick:
        case KnobEvent::LongPress: state_ = St::Overview; break;
        default: break;
      }
    }
    lv_obj_invalidate(canvas_);
  }
  void on_ring_angle(float deg) override {
    if (state_ == St::Field) {
      ed_.set_from_angle(deg);
      lv_obj_invalidate(canvas_);
    }
  }
  void on_tap(int x, int y) override {
    if (tap_in_header(x, y)) on_knob(KnobEvent::DoubleClick);
    else {
      if (state_ == St::Choice && y > 120 && y < 260) choice_ = y < 166 ? 0 : y < 212 ? 1 : 2;
      on_knob(KnobEvent::Click);
    }
  }

private:
  enum class St : uint8_t { Overview, Choice, Field };

  static const Period *pump_period(uint16_t id) {
    const Program &pr = g_state.ch[CH_PUMP].program;
    for (int i = 0; i < pr.count; ++i)
      if (pr.p[i].id == id) return &pr.p[i];
    return nullptr;
  }

  void save() {
    controller_save_salt(work_);
    dirty_ = false;
    ui_toast(LV_SYMBOL_OK " Sóbontás mentve", col::ON);
    ui_pop(-1);
  }

  static const char *choice_name(SaltChoice c) {
    switch (c) {
      case SaltChoice::Skip: return "Kihagyás";
      case SaltChoice::Full: return "Teljes ciklus";
      case SaltChoice::Shorter: return "Rövidebb időszak";
    }
    return "";
  }

  static void draw_cb(lv_event_t *e) {
    SaltView *v = (SaltView *)lv_event_get_user_data(e);
    lv_layer_t *l = draw_begin(e);
    const int cx = 180;
    if (v->state_ == St::Field) {
      v->ed_.draw(l, pk_channel_accent(CH_SALT));
      return;
    }
    // Külső gyűrű: szivattyúciklusok; belső vékony ív: sóbontási időablakok
    DialSeg segs[PK_MAX_PERIODS * 2];
    int n = 0;
    int sel_on = -1, sel_off = -1;
    for (int i = 0; i < v->work_.count; ++i) {
      const SaltCycle &c = v->work_.c[i];
      const Period *p = pump_period(c.cycle_id);
      if (!p) continue;
      const bool s = i == v->sel_;
      segs[n++] = {p->on_min, p->off_min, pk_channel_accent(CH_PUMP), s ? 28 : 20, 146};
      if (s) sel_on = p->on_min, sel_off = p->off_min;
      if (c.choice != SaltChoice::Skip && !c.invalid) {
        const int a = c.choice == SaltChoice::Full ? p->on_min : c.on_min;
        const int b = c.choice == SaltChoice::Full ? p->off_min : c.off_min;
        segs[n++] = {a, b, pk_channel_accent(CH_SALT), 8, 124};
      }
    }
    draw_day_dial(l, segs, n);
    if (sel_on >= 0) draw_arc(l, 180, 180, 163, 5, (int)min_to_deg(sel_on), (int)min_to_deg(sel_off), col::YELLOW, LV_OPA_COVER, true);
    draw_text(l, &pk_font_16, pk_channel_accent(CH_SALT), "Sóbontó · ciklusok", cx, 100, 210);

    char b[48], on[8], off[8];
    if (v->work_.count == 0) {
      draw_text(l, &pk_font_20, col::YELLOW, "Nincs mentett szivattyúciklus", cx, 160, 220);
      draw_text(l, &pk_font_16, col::MUTED, "Előbb a Szivattyú időprogramját állítsd be", cx, 206, 220);
      return;
    }
    if (v->sel_ == v->work_.count) {
      draw_text(l, &pk_font_36, col::YELLOW, LV_SYMBOL_SAVE " Mentés", cx, 168, 230);
      draw_text(l, &pk_font_16, v->dirty_ ? col::YELLOW : col::DIM, v->dirty_ ? "Nem mentett módosítás" : "Nincs változás",
                cx, 210, 220);
      return;
    }
    const SaltCycle &c = v->work_.c[v->sel_];
    const Period *p = pump_period(c.cycle_id);
    snprintf(b, sizeof b, "%d. ciklus", v->sel_ + 1);
    draw_text(l, &pk_font_16, col::MUTED, b, cx, 126, 200);
    if (p) {
      fmt_hhmm_min(on, sizeof on, p->on_min);
      fmt_hhmm_min(off, sizeof off, p->off_min);
      snprintf(b, sizeof b, "%s–%s", on, off);
      draw_text(l, &pk_font_28, col::TEXT, b, cx, 154, 240);
    }
    if (v->state_ == St::Choice) {
      for (int i = 0; i < 3; ++i) {
        const bool s = i == v->choice_;
        draw_text(l, s ? &pk_font_20 : &pk_font_16, s ? col::YELLOW : col::MUTED, choice_name((SaltChoice)i), cx,
                  184 + i * 30, 230);
      }
      return;
    }
    if (c.invalid) {
      draw_text(l, &pk_font_20, col::WARN, LV_SYMBOL_WARNING " Időzítés ellenőrzése", cx, 190, 240);
    } else if (c.choice == SaltChoice::Shorter) {
      fmt_hhmm_min(on, sizeof on, c.on_min);
      fmt_hhmm_min(off, sizeof off, c.off_min);
      snprintf(b, sizeof b, "Sóbontás %s–%s", on, off);
      draw_text(l, &pk_font_20, col::TEXT, b, cx, 190, 240);
    } else {
      draw_text(l, &pk_font_20, c.choice == SaltChoice::Skip ? col::MUTED : col::TEXT, choice_name(c.choice), cx, 190, 240);
    }
    draw_text(l, &pk_font_16, col::DIM, "Nyomd meg: módosítás", cx, 222, 220);
  }

  SaltConfig work_ = {};
  bool dirty_ = false, fresh_ = true;
  int sel_ = 0, choice_ = 0;
  St state_ = St::Overview;
  PairEditor ed_ = {};
  lv_obj_t *canvas_ = nullptr;
};

ChannelView g_channel;
ProgramView g_program;
SaltView g_salt;

}  // namespace

View *view_channel(ChannelId id) {
  g_channel.set(id);
  return &g_channel;
}
View *view_program(ChannelId id) {
  g_program.set(id);
  return &g_program;
}
View *view_salt() { return &g_salt; }
