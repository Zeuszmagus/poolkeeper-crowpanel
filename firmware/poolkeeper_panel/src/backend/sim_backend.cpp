// Szimulált vezérlő mindkét profilhoz (M1 demó).
// Bemutatja: friss adat, függő kapcsolás → visszaigazolás, elutasítás (retesz),
// lejárat kapcsolatvesztéskor, elavult adat és újracsatlakozáskori friss állapot.

#include <Arduino.h>
#include <string.h>
#include <sys/time.h>
#include "backend.h"
#include "../app_config.h"

namespace {

constexpr uint32_t ACK_DELAY_MS = 1200;     // Pi / modul válaszideje a szimulációban
constexpr uint32_t REJECT_DELAY_MS = 900;
constexpr uint32_t TELEMETRY_PERIOD_MS = 5000;

Output make_output(const char *key, const char *name, bool sched, bool fb, const char *fb_label) {
  Output o = {};
  o.key = key;
  o.name = name;
  o.has_schedule = sched;
  o.has_feedback = fb;
  o.feedback_label = fb_label;
  o.mode = OutputMode::Auto;
  o.cmd = CmdState::Idle;
  return o;
}

float jitter(float span) { return ((int32_t)(esp_random() % 2001) - 1000) / 1000.0f * span; }

class SimBackend : public Backend {
public:
  void begin(Profile p) override {
    AppState &s = g_state;
    s.profile = p;
    s.link_ok = true;
    s.link_lost_ms = 0;
    s.output_count = 0;
    if (p == Profile::Basic) {
      s.outputs[s.output_count++] = make_output("filter", "Szűrés", true, false, nullptr);
      s.outputs[s.output_count++] = make_output("light", "Világítás", true, false, nullptr);
      s.schedules[0] = {true, 0x7F, 8 * 60, 20 * 60};         // minden nap 08:00–20:00
      s.schedules[1] = {true, 0x7F, 21 * 60, 0 * 60 + 30};    // 21:00–00:30 (éjfélen át)
    } else {
      s.outputs[s.output_count++] = make_output("pump", "Szivattyú", true, true, "Áramlás");
      s.outputs[s.output_count++] = make_output("heater", "Fűtés", true, false, nullptr);
      s.outputs[s.output_count++] = make_output("light", "Világítás", true, false, nullptr);
      s.outputs[s.output_count++] = make_output("jet", "Ellenáram", false, false, nullptr);
      s.schedules[0] = {true, 0x7F, 8 * 60, 20 * 60};
      s.schedules[1] = {true, 0x7F, 10 * 60, 18 * 60};
      s.schedules[2] = {true, 0x7F, 21 * 60, 0 * 60 + 30};
      s.schedules[3] = {false, 0, 0, 0};
    }
    s.water_temp = {p == Profile::Full, 27.4f, millis()};
    s.ph = {p == Profile::Full, 7.32f, millis()};
    s.orp = {p == Profile::Full, 712.0f, millis()};
    sensor_fault_ = false;
    last_telemetry_ = millis();
    apply_schedules(true);
    pk_state_touch();
  }

  void loop() override {
    const uint32_t now = millis();
    AppState &s = g_state;

    // Parancsok feldolgozása
    for (int i = 0; i < s.output_count; ++i) {
      Output &o = s.outputs[i];
      if (o.cmd != CmdState::Pending) continue;
      const uint32_t age = now - o.cmd_sent_ms;
      if (!s.link_ok) {
        if (age >= PK_CMD_TIMEOUT_MS) fail(o, "Lejárt, nincs visszaigazolás");
        continue;
      }
      const char *reject = interlock_reason(i, o.cmd_target);
      if (reject && age >= REJECT_DELAY_MS) {
        fail(o, reject);
      } else if (!reject && age >= ACK_DELAY_MS) {
        o.on = o.cmd_target;
        o.cmd = CmdState::Confirmed;
        o.cmd_done_ms = now;
        // Kézi felülbírálás az automata időzítéssel szemben
        if (o.has_schedule && g_state.clock_valid) {
          o.mode = OutputMode::Manual;
          o.override_until = time(nullptr) + PK_OVERRIDE_DEFAULT_MIN * 60;
        }
        Serial.printf("[sim] ACK cmd=%lu %s -> %s\n", (unsigned long)o.cmd_id, o.key, o.on ? "BE" : "KI");
        pk_state_touch();
      }
    }

    // Visszajelző bemenetek (szivattyú áramlás)
    for (int i = 0; i < s.output_count; ++i) {
      Output &o = s.outputs[i];
      if (o.has_feedback && o.feedback_ok != o.on) {
        o.feedback_ok = o.on;
        pk_state_touch();
      }
    }

    // Telemetria — csak élő kapcsolatnál frissül
    if (s.profile == Profile::Full && s.link_ok && now - last_telemetry_ >= TELEMETRY_PERIOD_MS) {
      last_telemetry_ = now;
      publish_telemetry(now);
    }

    // Időzítés újraértékelése másodpercenként
    if (now - last_sched_eval_ >= 1000) {
      last_sched_eval_ = now;
      apply_schedules(false);
    }
  }

  void request_output(int idx, bool target_on) override {
    AppState &s = g_state;
    if (idx < 0 || idx >= s.output_count) return;
    Output &o = s.outputs[idx];
    if (o.cmd == CmdState::Pending) return;  // egyszerre egy függő parancs kimenetenként
    o.cmd = CmdState::Pending;
    o.cmd_target = target_on;
    o.cmd_id = ++next_cmd_id_;
    o.cmd_sent_ms = millis();
    o.cmd_error[0] = 0;
    Serial.printf("[sim] CMD id=%lu %s target=%s\n", (unsigned long)o.cmd_id, o.key, target_on ? "BE" : "KI");
    pk_state_touch();
  }

  void request_auto(int idx) override {
    AppState &s = g_state;
    if (idx < 0 || idx >= s.output_count) return;
    Output &o = s.outputs[idx];
    o.mode = OutputMode::Auto;
    o.override_until = 0;
    apply_schedules(true);
    pk_state_touch();
  }

  bool save_schedule(int idx, const Schedule &sc) override {
    if (idx < 0 || idx >= g_state.output_count) return false;
    if (sc.enabled && (sc.days == 0 || sc.start_min == sc.end_min)) return false;
    if (g_state.profile == Profile::Full && !g_state.link_ok) return false;  // a Pi dönt
    g_state.schedules[idx] = sc;
    Serial.printf("[sim] SCHED %s days=%02X %02u:%02u-%02u:%02u en=%d\n", g_state.outputs[idx].key, sc.days,
                  sc.start_min / 60, sc.start_min % 60, sc.end_min / 60, sc.end_min % 60, sc.enabled);
    apply_schedules(true);
    pk_state_touch();
    return true;
  }

  void sim_set_link(bool ok) override {
    AppState &s = g_state;
    if (s.link_ok == ok) return;
    s.link_ok = ok;
    if (!ok) {
      s.link_lost_ms = millis();
      Serial.println("[sim] LINK LOST");
    } else {
      Serial.println("[sim] LINK RESTORED -> friss állapot kérése");
      // Visszacsatlakozás: lejárt parancsokat eldobjuk, friss állapotot kérünk.
      for (int i = 0; i < s.output_count; ++i) {
        Output &o = s.outputs[i];
        if (o.cmd == CmdState::Pending && millis() - o.cmd_sent_ms >= PK_CMD_TIMEOUT_MS)
          fail(o, "Lejárt, eldobva");
      }
      if (s.profile == Profile::Full) publish_telemetry(millis());
    }
    pk_state_touch();
  }

  void sim_set_sensor_fault(bool fault) override {
    sensor_fault_ = fault;
    if (fault) g_state.orp.present = false;
    else if (g_state.profile == Profile::Full) g_state.orp = {true, 705.0f, millis()};
    pk_state_touch();
  }

  void sim_set_clock_valid(bool valid) override {
    g_state.clock_valid = valid;
    pk_state_touch();
  }

  bool sim_sensor_fault() const override { return sensor_fault_; }

private:
  void fail(Output &o, const char *why) {
    o.cmd = CmdState::Failed;
    o.cmd_done_ms = millis();
    strncpy(o.cmd_error, why, sizeof(o.cmd_error) - 1);
    o.cmd_error[sizeof(o.cmd_error) - 1] = 0;
    Serial.printf("[sim] FAIL cmd=%lu %s: %s\n", (unsigned long)o.cmd_id, o.key, why);
    pk_state_touch();
  }

  // A vezérlő (Pi) gépészeti feltételei — Full módban a fűtés keringetést igényel.
  const char *interlock_reason(int idx, bool target_on) {
    const AppState &s = g_state;
    const Output &o = s.outputs[idx];
    if (s.profile == Profile::Full && target_on && strcmp(o.key, "heater") == 0) {
      const Output &pump = s.outputs[0];
      if (!pump.on || !pump.feedback_ok) return "Elutasítva: nincs keringetés";
    }
    return nullptr;
  }

  void publish_telemetry(uint32_t now) {
    AppState &s = g_state;
    s.water_temp.value += jitter(0.05f);
    s.water_temp.updated_ms = now;
    s.ph.value += jitter(0.01f);
    s.ph.updated_ms = now;
    if (!sensor_fault_) {
      s.orp.value += jitter(2.0f);
      s.orp.updated_ms = now;
    }
    pk_state_touch();
  }

  void apply_schedules(bool force) {
    AppState &s = g_state;
    if (!s.clock_valid) return;  // érvénytelen óránál az automata időzítés felfüggesztve
    const time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    bool changed = false;
    for (int i = 0; i < s.output_count; ++i) {
      Output &o = s.outputs[i];
      if (!o.has_schedule) continue;
      if (o.mode == OutputMode::Manual) {
        if (o.override_until && now >= o.override_until) {
          o.mode = OutputMode::Auto;  // lejárt felülbírálás: vissza automatára
          o.override_until = 0;
          changed = true;
        } else {
          continue;
        }
      }
      if (o.cmd == CmdState::Pending) continue;
      const bool want = pk_schedule_active(s.schedules[i], lt);
      if (want != o.on) {
        // Az ütemező is a háttéren keresztül kapcsol: Basicben a modul, Fullban a Pi igazolja vissza.
        if (s.link_ok) {
          o.on = want;
          changed = true;
        }
      }
    }
    if (changed || force) pk_state_touch();
  }

  bool sensor_fault_ = false;
  uint32_t last_telemetry_ = 0;
  uint32_t last_sched_eval_ = 0;
  uint32_t next_cmd_id_ = 0;
};

SimBackend g_sim;

}  // namespace

Backend &backend() { return g_sim; }

void backend_switch_profile(Profile p) { g_sim.begin(p); }
