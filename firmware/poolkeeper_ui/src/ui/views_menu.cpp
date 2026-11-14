// UI-terv 02 és 06: főmenü. Egységes séma minden menüpontra:
//   fent: az eszköz ikonja (saját színével)
//   középen, a két lapozó nyíl között: a név
//   alatta: üzemmódjelző AUTO · 0 · KÉZI (az aktuális kiemelve)
//   legalul: tényleges működési állapot BE / KI (a vezérlő visszajelzése szerint)
// Jobbra forgatás / balra húzás: következő. Balra forgatás / jobbra húzás: előző. Nyomás: megnyitás.

#include <Arduino.h>
#include <stdio.h>

#include "../model/storage.h"
#include "ui_internal.h"

const char *channel_icon(ChannelId id) {
  switch (id) {
    case CH_PUMP: return PK_ICON_PUMP;
    case CH_LIGHT: return PK_ICON_LIGHT;
    case CH_NECK: return PK_ICON_NECK;
    case CH_MASSAGE: return PK_ICON_MASSAGE;
    case CH_COUNTER: return PK_ICON_COUNTER;
    case CH_AIR: return PK_ICON_AIR;
    case CH_SALT: return PK_ICON_SALT;
    default: return "";
  }
}

namespace {

enum class ItemKind : uint8_t { Clock, Channel, Stats, About };
struct Item {
  ItemKind kind;
  ChannelId ch;
};

constexpr int Y_ICON = 98, Y_NAME = 180, Y_MODE = 226, Y_STATE = 262, Y_NOTE = 290, Y_BACK = 320;
constexpr int MODE_X[3] = {112, 180, 248};  // AUTO, 0, KÉZI

class MenuView : public View {
public:
  const char *name() const override { return "Menü"; }

  void focus_first() { sel_ = 0; }  // megnyitáskor az első menüpont (Szivattyú)

  void build(lv_obj_t *root) override {
    collect();
    if (sel_ >= count_) sel_ = count_ - 1;
    const Item &it = items_[sel_];
    canvas_ = make_canvas(root, draw_cb, this);

    icon_ = mk_label(root, &pk_icons_56, item_accent(it), 180, Y_ICON, item_icon(it));
    const char *title = item_title(it);
    name_ = mk_label(root, title_font(title), col::TEXT, 180, Y_NAME, title);
    left_ = mk_label(root, &pk_font_36, sel_ > 0 ? col::YELLOW : col::DIM, 26, Y_NAME, LV_SYMBOL_LEFT);
    right_ = mk_label(root, &pk_font_36, sel_ < count_ - 1 ? col::YELLOW : col::DIM, 334, Y_NAME, LV_SYMBOL_RIGHT);

    for (auto &m : mode_) m = nullptr;
    state_ = note_ = sub_ = face_btn_ = face_lbl_ = nullptr;

    if (it.kind == ItemKind::Channel) {
      // Üzemmódjelző: mindhárom állás látszik, az aktuális színes „pirulában”
      static const char *names[3] = {"AUTO", "0", "KÉZI"};
      for (int i = 0; i < 3; ++i) {
        lv_obj_t *l = mk_label(root, &pk_font_20, col::DIM, MODE_X[i], Y_MODE, names[i]);
        lv_obj_set_style_pad_hor(l, 10, 0);
        lv_obj_set_style_pad_ver(l, 3, 0);
        lv_obj_set_style_radius(l, 14, 0);
        mode_[i] = l;
      }
      state_ = mk_label(root, &pk_font_28, col::MUTED, 180, Y_STATE, "");
      note_ = mk_label(root, &pk_font_16, col::WARN, 180, Y_NOTE, "", 220);
    } else {
      sub_ = mk_label(root, &pk_font_20, col::MUTED, 180, 226, "", 230);
    }

    // Vissza a főképernyőre (minden menüpontnál)
    lv_obj_t *back = mk_label(root, &pk_font_20, col::TEXT, 180, Y_BACK, LV_SYMBOL_LEFT "  Vissza");
    lv_obj_set_style_pad_hor(back, 16, 0);
    lv_obj_set_style_pad_ver(back, 4, 0);
    lv_obj_set_style_radius(back, 18, 0);
    lv_obj_set_style_border_width(back, 2, 0);
    lv_obj_set_style_border_color(back, C(col::DIM), 0);
    set_label_center(back, 180, Y_BACK);

    // Óra pont: számlapváltó gomb, a megnyitó területtől elkülönítve
    if (it.kind == ItemKind::Clock) {
      face_btn_ = lv_obj_create(root);
      lv_obj_remove_style_all(face_btn_);
      lv_obj_set_size(face_btn_, FACE_BTN_W, FACE_BTN_H);
      lv_obj_align(face_btn_, LV_ALIGN_CENTER, 0, FACE_BTN_Y - 180);
      lv_obj_remove_flag(face_btn_, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
      lv_obj_set_style_radius(face_btn_, FACE_BTN_H / 2, 0);
      lv_obj_set_style_border_width(face_btn_, 2, 0);
      lv_obj_set_style_border_color(face_btn_, C(col::YELLOW), 0);
      lv_obj_set_style_bg_color(face_btn_, C(0x0B2A55), 0);
      lv_obj_set_style_bg_opa(face_btn_, LV_OPA_COVER, 0);
      face_lbl_ = lv_label_create(face_btn_);
      lv_obj_set_style_text_font(face_lbl_, &pk_font_16, 0);
      lv_obj_set_style_text_color(face_lbl_, C(col::YELLOW), 0);
      lv_obj_center(face_lbl_);
    }
  }

  void refresh() override {
    const Item &it = items_[sel_];
    char b[64], t[16];
    switch (it.kind) {
      case ItemKind::Clock: {
        struct tm tm;
        if (pk_clock_now(&tm)) snprintf(t, sizeof t, "%02d:%02d", tm.tm_hour, tm.tm_min);
        else snprintf(t, sizeof t, "--:--");
        set_label(sub_, t);
        set_label_color(sub_, g_state.clock_valid ? col::TEXT : col::YELLOW);
        if (face_lbl_)
          set_label(face_lbl_, g_state.face == ClockFace::Analog ? LV_SYMBOL_REFRESH "  Számlap: analóg"
                                                                 : LV_SYMBOL_REFRESH "  Számlap: digitális");
        break;
      }
      case ItemKind::Channel: refresh_channel(it.ch); break;
      case ItemKind::Stats:
        fmt_duration(t, sizeof t, g_state.stats.today_s);
        snprintf(b, sizeof b, "Szivattyú ma: %s", t);
        set_label(sub_, b);
        break;
      case ItemKind::About:
        set_label(sub_, "PoolKeeper");
        set_label_color(sub_, col::YELLOW);
        break;
    }
  }

  void on_knob(KnobEvent ev) override {
    switch (ev) {
      case KnobEvent::Cw: page(+1); break;
      case KnobEvent::Ccw: page(-1); break;
      case KnobEvent::Click: open(); break;
      case KnobEvent::LongPress:
      case KnobEvent::DoubleClick: ui_pop(-1); break;  // vissza a főképernyőre
      default: break;
    }
  }

  void on_swipe_h(int dir) override { page(dir); }

  void on_tap(int x, int y) override {
    if (y > Y_BACK - 22 && x > 110 && x < 250) ui_pop(-1);  // „Vissza” → főképernyő
    else if (x < 70) page(-1);
    else if (x > 290) page(+1);
    else if (items_[sel_].kind == ItemKind::Clock) {
      // Gomb (kissé nagyobb érintési sávval) = számlapváltás; fölötte = időbeállítás
      if (y >= FACE_BTN_Y - FACE_BTN_H / 2 - 8 && y <= FACE_BTN_Y + FACE_BTN_H / 2 + 12) toggle_face();
      else if (y > 66 && y < FACE_BTN_Y - FACE_BTN_H / 2 - 8) open();
    } else if (y > 66 && y < Y_BACK - 22) open();
  }

private:
  void refresh_channel(ChannelId id) {
    const Channel &c = g_state.ch[id];
    for (int i = 0; i < 3; ++i) {
      const Mode m = (Mode)i;  // Auto=0, Off=1, Manual=2 — balról jobbra
      const bool act = c.mode == m;
      set_label_color(mode_[i], act ? 0x061936 : col::DIM);
      lv_obj_set_style_bg_color(mode_[i], C(mode_color(m)), 0);
      lv_obj_set_style_bg_opa(mode_[i], act ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
    // Működési állapot a kimeneti modul visszajelzése szerint (időprogram vagy kézi mód kapcsolta)
    uint32_t sc = col::MUTED;
    const char *st = "KI";
    switch (c.out) {
      case OutState::On: st = "BE"; sc = col::ON; break;
      case OutState::Off: st = "KI"; sc = col::MUTED; break;
      case OutState::PendingOn: st = "BE…"; sc = col::YELLOW; break;
      case OutState::PendingOff: st = "KI…"; sc = col::YELLOW; break;
      case OutState::Error: st = LV_SYMBOL_WARNING " HIBA"; sc = col::ERR; break;
    }
    set_label(state_, st);
    set_label_color(state_, sc);

    const char *note = "";
    uint32_t nc = col::WARN;
    if (c.out == OutState::Error) {
      note = "Nincs visszaigazolás";
      nc = col::ERR;
    } else if (id == CH_SALT && c.waiting_flow) {
      note = "Keringésre vár";
    } else if (id == CH_SALT && g_state.salt_needs_check) {
      note = LV_SYMBOL_WARNING " Időzítés ellenőrzése";
    } else if (c.mode == Mode::Auto && !g_state.clock_valid) {
      note = "Állítsd be az órát";
      nc = col::YELLOW;
    }
    set_label(note_, note);
    set_label_color(note_, nc);
  }

  void collect() {
    // Sorrend (ügyfél kérése, 2026-11-13): a csatornák elöl (első a Szivattyú), az Óra a sor végén.
    count_ = 0;
    for (int i = 0; i < CH_COUNT; ++i) {
      const ChannelId id = (ChannelId)g_state.layout.order[i];
      if (g_state.layout.visible[id]) items_[count_++] = {ItemKind::Channel, id};
    }
    items_[count_++] = {ItemKind::Stats, CH_COUNT};
    items_[count_++] = {ItemKind::About, CH_COUNT};
    items_[count_++] = {ItemKind::Clock, CH_COUNT};
  }

  static const char *item_title(const Item &it) {
    switch (it.kind) {
      case ItemKind::Clock: return "Óra";
      case ItemKind::Channel: return pk_channel_name(it.ch);
      case ItemKind::Stats: return "Statisztika";
      case ItemKind::About: return "Névjegy";
    }
    return "?";
  }

  static const char *item_icon(const Item &it) {
    switch (it.kind) {
      case ItemKind::Clock: return PK_ICON_CLOCK;
      case ItemKind::Channel: return channel_icon(it.ch);
      case ItemKind::Stats: return PK_ICON_STATS;
      case ItemKind::About: return PK_ICON_ABOUT;
    }
    return "";
  }

  static uint32_t item_accent(const Item &it) {
    switch (it.kind) {
      case ItemKind::Clock: return col::YELLOW;
      case ItemKind::Channel: return pk_channel_accent(it.ch);
      case ItemKind::Stats: return 0x9FB4D9;
      case ItemKind::About: return col::YELLOW;
    }
    return col::TEXT;
  }

  void page(int d) {
    const int n = sel_ + d;
    if (n < 0 || n >= count_) return;
    sel_ = n;
    ui_slide(d);
  }

  void open() {
    const Item &it = items_[sel_];
    switch (it.kind) {
      case ItemKind::Clock:
        if (g_state.profile == Profile::Full) {
          ui_toast("Full módban az időt a vezérlő adja", col::WARN);
          return;
        }
        ui_push(view_time_edit(), +1);
        break;
      case ItemKind::Channel: ui_push(view_channel(it.ch), +1); break;
      case ItemKind::Stats: ui_push(view_stats(), +1); break;
      case ItemKind::About: ui_push(view_about(), +1); break;
    }
  }

  void toggle_face() {
    g_state.face = g_state.face == ClockFace::Analog ? ClockFace::Digital : ClockFace::Analog;
    storage_save_face();
    // Nincs megerősítő üzenet: a gomb felirata azonnal mutatja az új számlapot.
    pk_touch();
  }

  static void draw_cb(lv_event_t *e) {
    MenuView *v = (MenuView *)lv_event_get_user_data(e);
    lv_layer_t *l = draw_begin(e);
    const uint32_t acc = item_accent(v->items_[v->sel_]);
    // Menüpont saját kiegészítő színe: felső körív
    draw_arc(l, 180, 180, 172, 8, 0, 360, col::RING, LV_OPA_60);
    draw_arc(l, 180, 180, 172, 8, 200, 340, acc, LV_OPA_COVER, true);
    // Helyzetjelző pontok az alsó íven
    const int n = v->count_;
    const float span = 5.0f;
    for (int i = 0; i < n; ++i) {
      const float a = 90.0f + (((n - 1) / 2.0f) - i) * span;
      int x, y;
      polar(180, 180, 172, a, &x, &y);  // a perem gyűrűjén, a Vissza gomb alatt
      draw_circle(l, x, y, i == v->sel_ ? 5 : 3, i == v->sel_ ? col::YELLOW : col::DIM);
    }
  }

  Item items_[CH_COUNT + 3];
  int count_ = 0;
  int sel_ = 0;
  static constexpr int FACE_BTN_Y = 262, FACE_BTN_W = 210, FACE_BTN_H = 36;
  lv_obj_t *canvas_ = nullptr, *icon_ = nullptr, *name_ = nullptr, *left_ = nullptr, *right_ = nullptr;
  lv_obj_t *mode_[3] = {}, *state_ = nullptr, *note_ = nullptr, *sub_ = nullptr;
  lv_obj_t *face_btn_ = nullptr, *face_lbl_ = nullptr;
};

MenuView g_menu;

}  // namespace

View *view_menu() { return &g_menu; }
void menu_focus_first_item() { g_menu.focus_first(); }
