// Telepítői menü → Hálózat: Wi-Fi beállítás telefonról (QR-kódos beállító portál), állapot, frissítés.

#include <Arduino.h>
#include <stdio.h>

#include "../net/net.h"
#include "ui_internal.h"

namespace {

class NetView : public View {
public:
  const char *name() const override { return "Hálózat"; }
  uint32_t idle_timeout() const override { return portal_ ? 0 : PK_IDLE_EDITOR_MS; }  // portál alatt nincs visszalépés

  void build(lv_obj_t *root) override {
    canvas_ = make_canvas(root, draw_cb, this);
    qr_ = nullptr;
    portal_ = net_status().portal;
    if (portal_) {
      // QR-kód: a telefon kamerájával beolvasva csatlakozik a beállító hálózathoz
      qr_ = lv_qrcode_create(root);
      lv_qrcode_set_size(qr_, 132);
      lv_qrcode_set_dark_color(qr_, lv_color_hex(0x000000));
      lv_qrcode_set_light_color(qr_, lv_color_hex(0xFFFFFF));
      const NetStatus &st = net_status();
      char data[96];
      snprintf(data, sizeof data, "WIFI:T:WPA;S:%s;P:%s;;", st.ap_ssid, st.admin_pass);
      lv_qrcode_update(qr_, data, strlen(data));
      lv_obj_set_style_border_color(qr_, lv_color_hex(0xFFFFFF), 0);
      lv_obj_set_style_border_width(qr_, 6, 0);
      lv_obj_align(qr_, LV_ALIGN_TOP_MID, 0, 34);
    }
  }

  void refresh() override {
    if (net_status().portal != portal_) ui_rebuild_current();  // a portál magától leállt / elindult
    else lv_obj_invalidate(canvas_);
  }

  void on_knob(KnobEvent ev) override {
    if (portal_) {
      if (ev == KnobEvent::Click || ev == KnobEvent::LongPress || ev == KnobEvent::DoubleClick) {
        net_stop_portal();
        ui_rebuild_current();
      }
      return;
    }
    switch (ev) {
      case KnobEvent::Cw: sel_ = (sel_ + 1) % 3; break;
      case KnobEvent::Ccw: sel_ = (sel_ + 2) % 3; break;
      case KnobEvent::Click:
        if (sel_ == 0) {
          net_start_portal();
          ui_rebuild_current();
          return;
        } else if (sel_ == 1) {
          ota_check_now();
          ui_toast("Frissítés keresése…", col::TEXT);
        } else {
          ui_pop(-1);
          return;
        }
        break;
      case KnobEvent::LongPress:
      case KnobEvent::DoubleClick: ui_pop(-1); return;
      default: break;
    }
    lv_obj_invalidate(canvas_);
  }

  void on_tap(int x, int y) override {
    if (portal_) {
      on_knob(KnobEvent::Click);
      return;
    }
    if (tap_in_header(x, y)) {
      ui_pop(-1);
      return;
    }
    if (y > 222 && y < 322) {
      sel_ = y < 252 ? 0 : y < 284 ? 1 : 2;
      on_knob(KnobEvent::Click);
    }
  }

private:
  static void draw_cb(lv_event_t *e) {
    NetView *v = (NetView *)lv_event_get_user_data(e);
    lv_layer_t *l = draw_begin(e);
    const NetStatus &st = net_status();
    const int cx = 180;
    char b[96];
    draw_arc(l, 180, 180, 172, 8, 0, 360, col::RING);

    if (v->portal_) {
      // A QR-kód felül van (widget); alatta a kézi csatlakozás adatai
      snprintf(b, sizeof b, "Wi-Fi: %s", st.ap_ssid);
      draw_text(l, &pk_font_16, col::TEXT, b, cx, 196, 260);
      snprintf(b, sizeof b, "Jelszó: %s", st.admin_pass);
      draw_text(l, &pk_font_20, col::YELLOW, b, cx, 220, 260);
      draw_text(l, &pk_font_16, col::MUTED, "Olvasd be a QR-kódot a telefonnal,", cx, 248, 330);
      draw_text(l, &pk_font_16, col::MUTED, "majd válaszd ki az otthoni Wi-Fi-t", cx, 268, 320);
      if (st.connected) {
        snprintf(b, sizeof b, LV_SYMBOL_OK " Kapcsolódva: %s", st.ssid);
        draw_text(l, &pk_font_16, col::ON, b, cx, 296, 260);
      } else {
        draw_text(l, &pk_font_16, col::DIM, "Nyomd meg a befejezéshez", cx, 296, 260);
      }
      return;
    }

    draw_text(l, &pk_font_20, col::YELLOW, LV_SYMBOL_WIFI "  Hálózat", cx, 46, 240);
    snprintf(b, sizeof b, "Eszköz: %s", st.device_id);
    draw_text(l, &pk_font_16, col::TEXT, b, cx, 80, 260);
    if (!st.configured) snprintf(b, sizeof b, "Wi-Fi: nincs beállítva");
    else if (st.connected) snprintf(b, sizeof b, "Wi-Fi: %s (%d dBm)", st.ssid, st.rssi);
    else snprintf(b, sizeof b, "Wi-Fi: kapcsolódás…");
    draw_text(l, &pk_font_16, st.connected ? col::ON : (st.configured ? col::YELLOW : col::MUTED), b, cx, 104, 270);
    snprintf(b, sizeof b, "IP: %s", st.connected ? st.ip : "–");
    draw_text(l, &pk_font_16, col::MUTED, b, cx, 128, 260);
    draw_text(l, &pk_font_16, st.time_synced ? col::ON : col::MUTED,
              st.time_synced ? "Idő: internetről szinkronizálva" : "Idő: nincs internetes szinkron", cx, 152, 270);
    snprintf(b, sizeof b, "Frissítés: %s", st.ota_msg);
    draw_text(l, &pk_font_16, st.ota_result == OtaResult::Error ? col::WARN : col::MUTED, b, cx, 176, 340);
    snprintf(b, sizeof b, "Telepítői jelszó: %s", st.admin_pass);
    draw_text(l, &pk_font_16, col::DIM, b, cx, 200, 260);

    static const char *opts[3] = {"Wi-Fi beállítás telefonról", "Frissítés keresése most", "Vissza"};
    for (int i = 0; i < 3; ++i) {
      const bool s = i == v->sel_;
      draw_text(l, s ? &pk_font_20 : &pk_font_16, s ? col::YELLOW : col::MUTED, opts[i], cx, 236 + i * 32, 270);
    }
  }

  lv_obj_t *canvas_ = nullptr, *qr_ = nullptr;
  bool portal_ = false;
  int sel_ = 0;
};

NetView g_net;

}  // namespace

View *view_net() { return &g_net; }
