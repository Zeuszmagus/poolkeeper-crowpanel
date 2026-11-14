#pragma once
// Waveshare Modbus RTU Relay 4CH vezérlése RS485-ön (UART0, J34 csatlakozó).
// Saját FreeRTOS-feladatban fut: a kívánt relé-bitmaszkot (relay_set_wanted) kiírja a modulba (FC 0x0F),
// majd visszaolvassa a tényleges állapotot (FC 0x01). A vezérlő csak a visszaolvasott állapotot tekinti érvényesnek.

#include <stdint.h>

void relay_begin();
void relay_set_wanted(uint8_t mask);  // bit0 = RELAY 1 … bit3 = RELAY 4

struct RelayStatus {
  bool link_ok;          // az utolsó lekérdezés sikeres volt
  bool ever_ok;          // volt már sikeres kapcsolat indulás óta
  uint8_t actual;        // visszaolvasott relé-állapot (csak link_ok mellett érvényes)
  uint32_t last_ok_ms;   // utolsó sikeres válasz ideje
  uint32_t ok_count, err_count;
  char last_err[40];
};
RelayStatus relay_status();

// Diagnosztika (konzol): nyers lekérdezések
int relay_read_version();  // szoftververzió ×100 (pl. 300 = V3.00), <0 hiba
