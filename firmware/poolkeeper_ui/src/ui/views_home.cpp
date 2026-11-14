// Főképernyő (óra → főképernyő → menü): a rendszer pillanatnyi állapota egy pillantással.
//   fent:   idő és nap
//   közép:  Szivattyú (a vízforgatás miatt kiemelve): ikon, BE/KI, üzemmód, következő kapcsolás
//   sor:    a többi eszköz kis ikonja — saját színnel, ha BE; szürkén, ha KI; alatta A / 0 / K
//   állapot: „Minden rendben” vagy a legfontosabb figyelmeztetés
//   perem:  24 órás gyűrű a szivattyú mai programjával és a pontos idővel
//   alul:   MENÜ gomb
// Nyomás / koppintás: menü. Hosszú nyomás: vissza az órához.

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "ui_internal.h"
#include "../net/net.h"

namespace {

const char *WEEKDAYS[7] = {"vasárnap", "hétfő", "kedd", "szerda", "csütörtök", "péntek", "szombat"};

constexpr int Y_TIME = 50, Y_PUMP_LBL = 86, Y_PUMP = 122, Y_NEXT = 158, Y_ROW = 204, Y_ROW_MODE = 234, Y_STATUS = 268,
              Y_MENU = 314;
constexpr int ROW_MAX = CH_COUNT - 1;

const char *mode_letter(Mode m) { return m == Mode::Auto ? "A" : m == Mode::Manual ? "K" : "0"; }
bool is_on(const Channel &c) { return c.out == OutState::On || c.out == OutState::PendingOff; }

class HomeView : public View {
public:
  const char *name() const override { return "Főképernyő"; }

  void build(lv_obj_t *root) override {
    canvas_ = make_canvas(root, draw_cb, this);
    time_ = mk_label(root, &pk_font_20, col::MUTED, 180, Y_TIME);

    // Szivattyú blokk
    pump_shown_ = g_state.layout.visible[CH_PUMP];
    pump_lbl_ = mk_label(root, &pk_font_16, col::MUTED, 180, Y_PUMP_LBL, pump_shown_ ? "Szivattyú" : "");
    pump_icon_ = mk_label(root, &pk_icons_32, pk_channel_accent(CH_PUMP), 112, Y_PUMP, pump_shown_ ? PK_ICON_PUMP : "");
    pump_state_ = mk_label(root, &pk_font_36, col::MUTED, 180, Y_PUMP);
    pump_mode_ = mk_label(root, &pk_font_16, col::TEXT, 250, Y_PUMP);
    lv_obj_set_style_pad_hor(pump_mode_, 8, 0);
    lv_obj_set_style_pad_ver(pump_mode_, 2, 0);
    lv_obj_set_style_radius(pump_mode_, 10, 0);
    lv_obj_set_style_bg_opa(pump_mode_, LV_OPA_COVER, 0);
    next_ = mk_label(root, &pk_font_16, col::MUTED, 180, Y_NEXT, "", 250);

    // A többi látható eszköz egy sorban, menüsorrendben
    row_n_ = 0;
    for (int i = 0; i < CH_COUNT && row_n_ < ROW_MAX; ++i) {
      const ChannelId id = (ChannelId)g_state.layout.order[i];
      if (id == CH_PUMP || !g_state.layout.visible[id]) continue;
      row_ids_[row_n_++] = id;
    }
    const int step = 46;
    for (int k = 0; k < row_n_; ++k) {
      const int x = 180 + (int)((k - (row_n_ - 1) / 2.0f) * step);
      row_icon_[k] = mk_label(root, &pk_icons_32, col::DIM, x, Y_ROW, channel_icon(row_ids_[k]));
      row_mode_[k] = mk_label(root, &pk_font_16, col::DIM, x, Y_ROW_MODE, "");
    }

    status_ = mk_label(root, &pk_font_16, col::ON, 180, Y_STATUS, "", 250);

    // MENÜ gomb
    lv_obj_t *menu = mk_label(root, &pk_font_20, col::YELLOW, 180, Y_MENU, LV_SYMBOL_LIST "   MENÜ");
    lv_obj_set_style_pad_hor(menu, 22, 0);
    lv_obj_set_style_pad_ver(menu, 6, 0);
    lv_obj_set_style_radius(menu, 20, 0);
    lv_obj_set_style_border_width(menu, 2, 0);
    lv_obj_set_style_border_color(menu, C(col::YELLOW), 0);
    lv_obj_set_style_bg_color(menu, C(0x0B2A55), 0);
    lv_obj_set_style_bg_opa(menu, LV_OPA_COVER, 0);
    set_label_center(menu, 180, Y_MENU);
  }

  void refresh() override {
    char b[64], t[16];
    struct tm tm;
    if (pk_clock_now(&tm)) {
      snprintf(b, sizeof b, "%02d:%02d  ·  %s", tm.tm_hour, tm.tm_min, WEEKDAYS[tm.tm_wday]);
      set_label(time_, b);
      set_label_color(time_, col::MUTED);
    } else {
      set_label(time_, "--:--");
      set_label_color(time_, col::YELLOW);
    }

    // Szivattyú
    if (pump_shown_) {
      const Channel &p = g_state.ch[CH_PUMP];
      const char *st = "KI";
      uint32_t sc = col::MUTED;
      switch (p.out) {
        case OutState::On: st = "BE"; sc = col::ON; break;
        case OutState::Off: break;
        case OutState::PendingOn: st = "BE…"; sc = col::YELLOW; break;
        case OutState::PendingOff: st = "KI…"; sc = col::YELLOW; break;
        case OutState::Error: st = LV_SYMBOL_WARNING; sc = col::ERR; break;
      }
      set_label(pump_state_, st);
      set_label_color(pump_state_, sc);
      set_label(pump_mode_, pk_mode_name(p.mode));
      lv_obj_set_style_bg_color(pump_mode_, C(mode_color(p.mode)), 0);
      set_label_color(pump_mode_, 0x061936);

      const int now = pk_now_min();
      if (p.mode == Mode::Auto) {
        int at;
        bool to_on;
        if (now < 0) {
          set_label(next_, "Időzítés szünetel");
        } else if (pk_program_next_change(p.program, now, &at, &to_on)) {
          fmt_hhmm_min(t, sizeof t, at);
          snprintf(b, sizeof b, "Következő: %s %s%s", to_on ? "BE" : "KI", at >= 1440 ? "holnap " : "", t);
          set_label(next_, b);
        } else {
          set_label(next_, "Nincs időprogram");
        }
      } else {
        set_label(next_, p.mode == Mode::Manual ? "Kézi üzem" : "Kikapcsolt üzemmód");
      }
    } else {
      set_label(pump_state_, "");
      set_label(pump_mode_, "");
      lv_obj_set_style_bg_opa(pump_mode_, LV_OPA_TRANSP, 0);
      set_label(next_, "");
    }

    // Eszközsor
    for (int k = 0; k < row_n_; ++k) {
      const Channel &c = g_state.ch[row_ids_[k]];
      const bool on = is_on(c);
      const bool err = c.out == OutState::Error;
      const bool waiting = row_ids_[k] == CH_SALT && c.waiting_flow;
      // Ikon: saját szín, ha BE; szürke, ha KI (várakozáskor is); piros, ha nincs visszaigazolás
      set_label_color(row_icon_[k], err ? col::ERR : on ? pk_channel_accent(row_ids_[k]) : col::DIM);
      set_label(row_mode_[k], mode_letter(c.mode));
      set_label_color(row_mode_[k], (err || waiting) ? col::WARN : c.mode == Mode::Off ? col::DIM : mode_color(c.mode));
    }

    // Állapotsor: a legfontosabb figyelmeztetés, különben „Minden rendben”
    const char *msg = nullptr;
    uint32_t mc = col::WARN;
    static char buf[64];
    if (!g_state.module_ok) {
      msg = LV_SYMBOL_WARNING " A kimeneti modul nem válaszol";
      mc = col::ERR;
    }
    for (int i = 0; !msg && i < CH_COUNT; ++i) {
      if (g_state.ch[i].out == OutState::Error) {
        snprintf(buf, sizeof buf, LV_SYMBOL_WARNING " %s: nincs visszaigazolás", pk_channel_name((ChannelId)i));
        msg = buf;
        mc = col::ERR;
      }
    }
    if (!msg && !g_state.clock_valid) {
      msg = "Állítsd be az órát";
      mc = col::YELLOW;
    }
    if (!msg && g_state.ch[CH_SALT].waiting_flow) msg = "Sóbontó: keringésre vár";
    if (!msg && g_state.salt_needs_check) msg = LV_SYMBOL_WARNING " Sóbontó: időzítés ellenőrzése";
    if (!msg) {
      // Wi-Fi: csak ha be van állítva, de tartósan nincs kapcsolat (pl. router nem indult el)
      const NetStatus &ns = net_status();
      static uint32_t down_since = 0;
      if (ns.configured && !ns.connected && !ns.portal) {
        if (!down_since) down_since = millis();
        if (millis() - down_since > 120000) msg = LV_SYMBOL_WIFI " Nincs Wi-Fi kapcsolat";
      } else {
        down_since = 0;
      }
    }
    if (!msg) {
      msg = LV_SYMBOL_OK " Minden rendben";
      mc = col::ON;
    }
    set_label(status_, msg);
    set_label_color(status_, mc);

    // A peremgyűrű percenként frissül
    const int now = pk_now_min();
    if (now != last_min_ || g_state.rev != last_rev_) {
      last_min_ = now;
      last_rev_ = g_state.rev;
      lv_obj_invalidate(canvas_);
    }
  }

  void on_knob(KnobEvent ev) override {
    if (ev == KnobEvent::Click) open_menu();
    else if (ev == KnobEvent::LongPress || ev == KnobEvent::DoubleClick) ui_pop(-1);  // vissza az órához
  }
  void on_tap(int, int) override { open_menu(); }

private:
  static void open_menu() {
    menu_focus_first_item();
    ui_push(view_menu(), +1);
  }

  static void draw_cb(lv_event_t *e) {
    lv_layer_t *l = draw_begin(e);
    // 24 órás gyűrű: a szivattyú programja (AUTO-ban élénk, egyébként halvány) és a pontos idő
    const int R = 172, W = 10;
    draw_arc(l, 180, 180, R, W, 0, 360, col::RING);
    if (g_state.layout.visible[CH_PUMP]) {
      const Channel &p = g_state.ch[CH_PUMP];
      const lv_opa_t opa = p.mode == Mode::Auto ? LV_OPA_COVER : LV_OPA_50;
      for (int i = 0; i < p.program.count; ++i)
        draw_arc(l, 180, 180, R, W, (int)min_to_deg(p.program.p[i].on_min), (int)min_to_deg(p.program.p[i].off_min),
                 pk_channel_accent(CH_PUMP), opa);
    }
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

  lv_obj_t *canvas_ = nullptr, *time_ = nullptr, *pump_lbl_ = nullptr, *pump_icon_ = nullptr, *pump_state_ = nullptr,
           *pump_mode_ = nullptr, *next_ = nullptr, *status_ = nullptr;
  lv_obj_t *row_icon_[ROW_MAX] = {}, *row_mode_[ROW_MAX] = {};
  ChannelId row_ids_[ROW_MAX] = {};
  int row_n_ = 0;
  bool pump_shown_ = true;
  int last_min_ = -2;
  uint32_t last_rev_ = 0;
};

HomeView g_home;

}  // namespace

View *view_home() { return &g_home; }
