#pragma once
// A nézetréteg belső felülete: nézetek, navigáció, rajzolási segédek, színek.

#include <lvgl.h>

#include "../hal/board.h"
#include "../model/model.h"

LV_FONT_DECLARE(pk_font_16);
LV_FONT_DECLARE(pk_font_20);
LV_FONT_DECLARE(pk_font_28);
LV_FONT_DECLARE(pk_font_30);
LV_FONT_DECLARE(pk_font_36);
LV_FONT_DECLARE(pk_font_72);
LV_FONT_DECLARE(pk_font_96);
LV_FONT_DECLARE(pk_font_120);
LV_FONT_DECLARE(pk_icons_56);  // Material Design Icons 7.4.47 (Apache 2.0)
LV_FONT_DECLARE(pk_icons_32);

// Menüikonok (UTF-8, pk_icons_56)
#define PK_ICON_PUMP     "\xF3\xB1\x90\x82"
#define PK_ICON_LIGHT    "\xF3\xB0\x9B\xA8"
#define PK_ICON_NECK     "\xF3\xB0\xA6\xA1"
#define PK_ICON_MASSAGE  "\xF3\xB0\xA0\xA8"
#define PK_ICON_COUNTER  "\xF3\xB1\xA1\x9A"
#define PK_ICON_AIR      "\xF3\xB0\x97\xA3"
#define PK_ICON_SALT     "\xF3\xB1\x84\x8E"
#define PK_ICON_STATS    "\xF3\xB0\x84\xA8"
#define PK_ICON_ABOUT    "\xF3\xB0\x8B\xBD"
#define PK_ICON_CLOCK    "\xF3\xB0\x85\x90"
const char *channel_icon(ChannelId id);

// PoolKeeper arculat: mélykék alap, világos szöveg, sárga kiemelés (UI-terv 11).
namespace col {
constexpr uint32_t BG = 0x051936;
constexpr uint32_t FACE = 0x0A2852;
constexpr uint32_t RING = 0x133A70;
constexpr uint32_t TEXT = 0xF3F7FF;
constexpr uint32_t MUTED = 0x93A9CD;
constexpr uint32_t DIM = 0x47618C;
constexpr uint32_t YELLOW = 0xFFC928;  // aktív kijelölés
// Állapotjelentések — minden menüben egységesek
constexpr uint32_t AUTO = 0x38C6FF;
constexpr uint32_t OFF = 0xA3B1C9;
constexpr uint32_t ON = 0x4CD97B;
constexpr uint32_t WARN = 0xFF9F40;
constexpr uint32_t ERR = 0xFF5A5A;
}  // namespace col

inline lv_color_t C(uint32_t h) { return lv_color_hex(h); }
uint32_t mode_color(Mode m);

// ---- Nézetek ----
class View {
public:
  virtual ~View() = default;
  virtual void build(lv_obj_t *root) = 0;
  virtual void refresh() {}
  virtual void on_knob(KnobEvent ev) = 0;
  virtual void on_tap(int x, int y) { (void)x; (void)y; }
  virtual void on_swipe_h(int dir) { (void)dir; }   // +1: ujj balra húzva (következő), -1: jobbra (előző)
  // Körbe húzás a számlap peremén. deg: 0 = 12 óra irány, óramutató járása szerint 0..360.
  virtual void on_ring_angle(float deg) { (void)deg; }
  virtual bool wants_ring_drag() const { return false; }
  virtual uint32_t idle_timeout() const;             // 0 = nincs visszatérés az órához
  virtual void on_leave() {}                          // nézet elhagyása előtt (pl. mentetlen elvetése)
  virtual const char *name() const = 0;
};

// Navigáció. dir: +1 = az új nézet jobbról csúszik be, -1 = balról, 0 = animáció nélkül.
void ui_push(View *v, int dir = +1);
void ui_pop(int dir = -1);
void ui_home();              // vissza az óra nézetre
void ui_rebuild_current();   // a jelenlegi nézet újraépítése (pl. elrendezés-változás)
void ui_slide(int dir);      // a jelenlegi nézet újraépítése oldalirányú átmenettel (lapozás)
void ui_toast(const char *msg, uint32_t color = col::TEXT, uint32_t ms = 2200);
View *ui_current();

// Nézetpéldányok (views_*.cpp)
View *view_clock();
View *view_home();   // főképernyő: a rendszer pillanatnyi állapota
View *view_menu();
View *view_time_edit();
View *view_channel(ChannelId id);
View *view_program(ChannelId id);
View *view_salt();
View *view_stats();
View *view_about();
View *view_installer();
View *view_net();         // telepítő: Wi-Fi, idő, frissítés
void menu_focus_first_item();   // a menü az első ponton (Szivattyú) nyíljon

// ---- Rajzolási segédek (egyedi rajzolású objektumokhoz) ----
// Szögek fokban, 0° = 3 óra irány, óramutató járásával egyezően (LVGL).
lv_obj_t *make_canvas(lv_obj_t *parent, lv_event_cb_t draw_cb, void *user);
lv_layer_t *draw_begin(lv_event_t *e);  // a rajzoló callback első hívása
void draw_arc(lv_layer_t *l, int cx, int cy, int r, int w, int a0, int a1, uint32_t color, lv_opa_t opa = LV_OPA_COVER,
              bool rounded = false);
void draw_line(lv_layer_t *l, int x0, int y0, int x1, int y1, int w, uint32_t color, bool round = true);
void draw_circle(lv_layer_t *l, int cx, int cy, int r, uint32_t color, lv_opa_t opa = LV_OPA_COVER);
void draw_text(lv_layer_t *l, const lv_font_t *f, uint32_t color, const char *txt, int cx, int cy, int w = 300);
void polar(int cx, int cy, float r, float deg, int *x, int *y);
inline float min_to_deg(int minute_of_day) { return 270.0f + minute_of_day * 0.25f; }
// Körbe húzás szögéből érték: n osztás egy teljes körön (0 felül)
inline int angle_to_value(float deg, int n) {
  int v = (int)(deg * n / 360.0f + 0.5f);
  return v >= n ? v - n : v;
}

// ---- Widget segédek ----
lv_obj_t *mk_label(lv_obj_t *parent, const lv_font_t *f, uint32_t color, int cx, int cy, const char *txt = "",
                   int width = 0);
void set_label(lv_obj_t *l, const char *txt);
void set_label_color(lv_obj_t *l, uint32_t color);
void set_label_center(lv_obj_t *l, int cx, int cy);
// Felirat font választása: a legnagyobb, amelyben a szöveg elfér max_w szélességben.
const lv_font_t *fit_font(const char *txt, int max_w, const lv_font_t *const *fonts, int n);
// Menüpont-/eszköznév: a legnagyobb méret (36 → 30 → 28 → 20), amelyben a két lapozó nyíl között elfér.
const lv_font_t *title_font(const char *txt);
// Fejléc: "‹ Név" a kör tetején; koppintással vissza.
lv_obj_t *mk_header(lv_obj_t *parent, const char *txt, uint32_t color);
bool tap_in_header(int x, int y);

void fmt_duration(char *buf, int n, uint32_t secs);
