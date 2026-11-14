// UI-terv 07 (Statisztika), 08 (Névjegy), 09 (Telepítői beállítások).

#include <Arduino.h>
#include <stdio.h>

#include "../app_config.h"
#include "../model/storage.h"
#include "ui_internal.h"
#include "../net/net.h"

namespace {

// ======================= Statisztika =======================
class StatsView : public View {
public:
  const char *name() const override { return "Statisztika"; }
  void build(lv_obj_t *root) override {
    canvas_ = make_canvas(root, draw_cb, this);
    mk_header(root, "Statisztika", 0x9FB4D9);
    // Csak a vezérlőkimenet állapota ismert → "Bekapcsolt kimenet ideje" (UI-terv 07)
    mk_label(root, &pk_font_16, col::MUTED, 180, 86, "Szivattyú – bekapcsolt kimenet ideje", 250);
    static const char *names[3] = {"Ma", "Tegnap", "Összesen"};
    for (int i = 0; i < 3; ++i) {
      mk_label(root, &pk_font_20, col::MUTED, 180, 126 + i * 64, names[i]);
      val_[i] = mk_label(root, &pk_font_28, col::TEXT, 180, 154 + i * 64);
    }
  }
  void refresh() override {
    char b[32];
    const Stats &s = g_state.stats;
    fmt_duration(b, sizeof b, s.today_s);
    set_label(val_[0], b);
    fmt_duration(b, sizeof b, s.yesterday_s);
    set_label(val_[1], b);
    fmt_duration(b, sizeof b, s.total_s);
    set_label(val_[2], b);
  }
  void on_knob(KnobEvent ev) override {
    if (ev == KnobEvent::Click || ev == KnobEvent::DoubleClick || ev == KnobEvent::LongPress) ui_pop(-1);
  }
  void on_tap(int, int) override { ui_pop(-1); }

private:
  static void draw_cb(lv_event_t *e) {
    lv_layer_t *l = draw_begin(e);
    draw_arc(l, 180, 180, 172, 8, 200, 340, 0x9FB4D9, LV_OPA_COVER, true);
  }
  lv_obj_t *canvas_ = nullptr, *val_[3] = {};
};

// ======================= Névjegy =======================
class AboutView : public View {
public:
  const char *name() const override { return "Névjegy"; }
  void build(lv_obj_t *root) override {
    make_canvas(root, draw_cb, this);
    mk_label(root, &pk_font_36, col::YELLOW, 180, 120, "PoolKeeper");
    mk_label(root, &pk_font_20, col::MUTED, 180, 168, "Ügyfélszolgálat");
    const bool has_phone = PK_SUPPORT_PHONE[0] != 0, has_mail = PK_SUPPORT_EMAIL[0] != 0;
    char b[96];
    if (has_phone || has_mail)
      snprintf(b, sizeof b, "%s%s%s", PK_SUPPORT_PHONE, has_phone && has_mail ? "\n" : "", PK_SUPPORT_EMAIL);
    else
      snprintf(b, sizeof b, "Az elérhetőség hamarosan");
    mk_label(root, &pk_font_20, col::TEXT, 180, 206, b, 250);
    const NetStatus &ns = net_status();
    snprintf(b, sizeof b, "v" PK_FW_VERSION "  ·  %s%s%s", ns.device_id, ns.connected ? "  ·  " : "", ns.connected ? ns.ip : "");
    mk_label(root, &pk_font_16, col::DIM, 180, 270, b, 260);
  }
  void on_knob(KnobEvent ev) override {
    if (ev == KnobEvent::LongPress) ui_push(view_installer(), +1);  // telepítői beállítások
    else if (ev == KnobEvent::Click || ev == KnobEvent::DoubleClick) ui_pop(-1);
  }
  void on_tap(int, int) override { ui_pop(-1); }

private:
  static void draw_cb(lv_event_t *e) {
    lv_layer_t *l = draw_begin(e);
    draw_arc(l, 180, 180, 172, 8, 0, 360, col::RING);
    draw_arc(l, 180, 180, 172, 8, 200, 340, col::YELLOW, LV_OPA_COVER, true);
    draw_arc(l, 180, 180, 172, 8, 20, 160, 0x3D9BFF, LV_OPA_COVER, true);
  }
};

// ======================= Telepítői beállítások =======================
// Mely eszközök szerepeljenek a menüben és milyen sorrendben. Az átrendezés és elrejtés nem kapcsol kimenetet.
class InstallerView : public View {
public:
  const char *name() const override { return "Telepítő"; }
  uint32_t idle_timeout() const override { return PK_IDLE_EDITOR_MS; }

  void build(lv_obj_t *root) override {
    if (fresh_) {
      work_ = g_state.layout;
      sel_ = 0;
      state_ = St::List;
      dirty_ = false;
      fresh_ = false;
    }
    child_open_ = false;
    canvas_ = make_canvas(root, draw_cb, this);
    mk_header(root, "Telepítői beállítások", col::YELLOW);
  }
  void refresh() override { lv_obj_invalidate(canvas_); }
  // A Hálózat nézet megnyitása nem veti el a még nem mentett elrendezést
  void on_leave() override {
    if (!child_open_) fresh_ = true;
  }

  void on_knob(KnobEvent ev) override {
    const int n = CH_COUNT + 3;  // eszközök + Hálózat + Mentés + Kilépés
    switch (state_) {
      case St::List:
        if (ev == KnobEvent::Cw) sel_ = (sel_ + 1) % n;
        else if (ev == KnobEvent::Ccw) sel_ = (sel_ + n - 1) % n;
        else if (ev == KnobEvent::Click) {
          if (sel_ == CH_COUNT) {
            child_open_ = true;
            ui_push(view_net(), +1);
          } else if (sel_ == CH_COUNT + 1) save();
          else if (sel_ == CH_COUNT + 2) exit_();
          else {
            act_ = 0;
            state_ = St::Action;
          }
        } else if (ev == KnobEvent::DoubleClick || ev == KnobEvent::LongPress) exit_();
        break;
      case St::Action:
        if (ev == KnobEvent::Cw) act_ = (act_ + 1) % 4;
        else if (ev == KnobEvent::Ccw) act_ = (act_ + 3) % 4;
        else if (ev == KnobEvent::Click) {
          const ChannelId id = (ChannelId)work_.order[sel_];
          if (act_ == 0) {
            work_.visible[id] = !work_.visible[id];
            dirty_ = true;
          } else if (act_ == 1) {
            state_ = St::Move;
          } else if (act_ == 2) {
            ui_toast("Az átnevezés a következő változatban", col::MUTED);
          } else {
            state_ = St::List;
          }
        } else if (ev == KnobEvent::DoubleClick || ev == KnobEvent::LongPress) state_ = St::List;
        break;
      case St::Move:
        if (ev == KnobEvent::Cw && sel_ < CH_COUNT - 1) {
          swap(sel_, sel_ + 1);
          sel_++;
        } else if (ev == KnobEvent::Ccw && sel_ > 0) {
          swap(sel_, sel_ - 1);
          sel_--;
        } else if (ev == KnobEvent::Click || ev == KnobEvent::DoubleClick || ev == KnobEvent::LongPress) {
          state_ = St::List;  // a gombnyomás rögzíti a pozíciót
        }
        break;
    }
    lv_obj_invalidate(canvas_);
  }
  void on_swipe_h(int dir) override {
    if (state_ == St::List) on_knob(dir > 0 ? KnobEvent::Cw : KnobEvent::Ccw);
  }
  void on_tap(int x, int y) override {
    if (tap_in_header(x, y)) on_knob(KnobEvent::DoubleClick);
    else on_knob(KnobEvent::Click);
  }

private:
  enum class St : uint8_t { List, Action, Move };
  void swap(int a, int b) {
    const uint8_t t = work_.order[a];
    work_.order[a] = work_.order[b];
    work_.order[b] = t;
    dirty_ = true;
  }
  void save() {
    g_state.layout = work_;
    storage_save_layout();
    dirty_ = false;
    ui_toast(LV_SYMBOL_OK " Telepítési profil mentve", col::ON);
    ui_home();
  }
  void exit_() {
    if (dirty_) ui_toast("Mentés nélkül kiléptél", col::WARN);
    ui_pop(-1);
  }

  static void draw_cb(lv_event_t *e) {
    InstallerView *v = (InstallerView *)lv_event_get_user_data(e);
    lv_layer_t *l = draw_begin(e);
    const int cx = 180;
    draw_arc(l, 180, 180, 172, 8, 0, 360, col::RING);
    char b[64];
    if (v->sel_ == CH_COUNT) {
      draw_text(l, &pk_font_36, col::YELLOW, LV_SYMBOL_WIFI " Hálózat", cx, 172, 260);
      draw_text(l, &pk_font_16, col::MUTED, "Wi-Fi, pontos idő, távoli frissítés", cx, 214, 240);
    } else if (v->sel_ > CH_COUNT) {
      draw_text(l, &pk_font_36, col::YELLOW, v->sel_ == CH_COUNT + 1 ? LV_SYMBOL_SAVE " Mentés" : "Kilépés", cx, 172, 240);
      if (v->sel_ == CH_COUNT + 1)
        draw_text(l, &pk_font_16, v->dirty_ ? col::YELLOW : col::DIM, v->dirty_ ? "Nem mentett módosítás" : "Nincs változás",
                  cx, 214, 220);
    } else {
      const ChannelId id = (ChannelId)v->work_.order[v->sel_];
      const char *nm = pk_channel_name(id);
      snprintf(b, sizeof b, "%d. hely", v->sel_ + 1);
      draw_text(l, &pk_font_16, col::MUTED, b, cx, 104, 200);
      if (v->state_ == St::Move) {
        if (v->sel_ > 0) draw_text(l, &pk_font_16, col::DIM, pk_channel_name((ChannelId)v->work_.order[v->sel_ - 1]), cx, 132, 220);
        draw_text(l, title_font(nm), col::YELLOW, nm, cx, 168, 260);
        if (v->sel_ < CH_COUNT - 1)
          draw_text(l, &pk_font_16, col::DIM, pk_channel_name((ChannelId)v->work_.order[v->sel_ + 1]), cx, 204, 220);
        draw_text(l, &pk_font_16, col::YELLOW, "Áthelyezés – forgasd, majd nyomd meg", cx, 246, 240);
      } else {
        draw_text(l, title_font(nm), v->state_ == St::Action ? col::TEXT : pk_channel_accent(id), nm, cx, 140, 260);
        if (v->state_ == St::Action) {
          snprintf(b, sizeof b, "Megjelenítés: %s", v->work_.visible[id] ? "látható" : "rejtett");
          const char *opts[4] = {b, "Áthelyezés", "Név", "Vissza"};
          for (int i = 0; i < 4; ++i) {
            const bool s = i == v->act_;
            draw_text(l, s ? &pk_font_20 : &pk_font_16, s ? col::YELLOW : col::MUTED, opts[i], cx, 184 + i * 28, 250);
          }
        } else {
          draw_text(l, &pk_font_20, v->work_.visible[id] ? col::ON : col::DIM,
                    v->work_.visible[id] ? "Látható" : "Rejtett", cx, 186, 200);
          draw_text(l, &pk_font_16, col::DIM, "Nyomd meg: beállítás", cx, 222, 220);
        }
      }
    }
    for (int k = 0; k < CH_COUNT + 3; ++k) {
      const int x = cx + (k - (CH_COUNT + 2) / 2.0f) * 14;
      draw_circle(l, x, 296, k == v->sel_ ? 5 : 3, k == v->sel_ ? col::YELLOW : col::DIM);
    }
  }

  Layout work_ = {};
  bool fresh_ = true, dirty_ = false, child_open_ = false;
  int sel_ = 0, act_ = 0;
  St state_ = St::List;
  lv_obj_t *canvas_ = nullptr;
};

StatsView g_stats;
AboutView g_about;
InstallerView g_installer;

}  // namespace

View *view_stats() { return &g_stats; }
View *view_about() { return &g_about; }
View *view_installer() { return &g_installer; }
