#pragma once
// Hardware abstraction for M5StickC Plus2 (M5Unified / M5GFX).
//
// The upstream firmware targets the original M5StickC Plus via the
// M5StickCPlus library (AXP192 PMIC, TFT_eSPI). The Plus2 has no AXP192:
// backlight is PWM on GPIO27, power-hold is GPIO4, battery is sensed on an
// ADC pin, and there is no VBUS / battery-current measurement. M5Unified
// handles all of that; this header maps the old call sites onto it.
#include <M5Unified.h>

// TFT_eSPI-compatible type names so the drawing code stays untouched.
// M5Canvas (sprite) and M5GFX (panel) both derive from lgfx::LovyanGFX.
using TFT_eSprite = M5Canvas;
using TFT_eSPI    = lgfx::LovyanGFX;

// Plus2 red LED is GPIO19, active-high (Plus was GPIO10, active-low).
#define LED_PIN_HW 19
#define LED_ON     HIGH
#define LED_OFF    LOW

namespace hal {

// Upstream used AXP ScreenBreath(0..100); M5GFX brightness is 0..255.
inline void screenBreath(uint8_t pct) {
  if (pct > 100) pct = 100;
  M5.Display.setBrightness((uint16_t)pct * 255 / 100);
}

// Upstream toggled AXP LDO2 (LCD backlight + panel power).
inline void screenPower(bool on) {
  if (on) M5.Display.wakeup();
  else  { M5.Display.setBrightness(0); M5.Display.sleep(); }
}

// Plus2 reads battery voltage directly on an ESP32 ADC pin (no AXP192 fuel
// gauge). Single-shot ADC reads jitter by ±30–50mV, which makes the 60fps
// DEVICE info page flicker constantly. Oversample + EMA filter + 15mV
// hysteresis deadband so the displayed percentage (10mV = 1%) stays steady.
inline int batteryMillivolts() {
  static float    ema     = 0.0f;
  static int      latched = 0;
  static uint32_t lastMs  = 0;

  uint32_t now = millis();
  if (ema == 0.0f) {
    int32_t sum = 0;
    for (int i = 0; i < 8; i++) sum += M5.Power.getBatteryVoltage();
    ema     = sum / 8.0f;
    latched = (int)(ema + 0.5f);
    lastMs  = now;
    return latched;
  }

  if (now - lastMs >= 250) {
    lastMs = now;
    int32_t sum = 0;
    for (int i = 0; i < 4; i++) sum += M5.Power.getBatteryVoltage();
    float sample = sum / 4.0f;
    float alpha = (fabsf(sample - ema) > 150.0f) ? 0.5f : 0.1f;
    ema += alpha * (sample - ema);
    int cur = (int)(ema + 0.5f);
    if (abs(cur - latched) >= 15) latched = cur;
  }
  return latched;
}

inline float batteryVolts() { return batteryMillivolts() / 1000.0f; }

// Plus2 cannot measure battery current.
inline float batteryMilliamps() { return 0.0f; }

// Plus2 has no VBUS sense. M5Unified may report charging state on some
// revisions; otherwise fall back to "battery sitting at full voltage",
// which is what you see while the charger is holding it topped up.
inline bool usbPowered() {
  auto c = M5.Power.isCharging();
  if (c == m5::Power_Class::is_charging_t::is_charging) return true;
  static bool usb = false;
  int mv = batteryMillivolts();
  if (mv >= 4150) usb = true;
  else if (mv < 4100) usb = false;
  return usb;
}

// No VBUS reading on Plus2; report 5V when we believe USB is connected.
inline float vbusVolts() { return usbPowered() ? 5.0f : 0.0f; }

// No PMIC temperature sensor; use the ESP32 internal sensor instead.
inline float temperatureC() { return temperatureRead(); }

inline void accel(float* ax, float* ay, float* az) {
  M5.Imu.update();
  M5.Imu.getAccel(ax, ay, az);
}

// Power button (GPIO35) short click. Upstream polled AXP GetBtnPress()==0x02.
inline bool powerButtonClicked() { return M5.BtnPWR.wasClicked(); }

inline void powerOff() { M5.Power.powerOff(); }

inline void beep(uint16_t freq, uint16_t durMs) { M5.Speaker.tone(freq, durMs); }

inline void begin() {
  auto cfg = M5.config();
  cfg.internal_spk = true;   // Plus2 passive buzzer on GPIO2
  M5.begin(cfg);
  M5.Speaker.setVolume(128);
}

// RTC helpers matching the shapes upstream stored (hour/min/sec, date).
inline void rtcSet(const struct tm& lt) {
  m5::rtc_datetime_t dt;
  dt.time.hours   = lt.tm_hour;
  dt.time.minutes = lt.tm_min;
  dt.time.seconds = lt.tm_sec;
  dt.date.year    = lt.tm_year + 1900;
  dt.date.month   = lt.tm_mon + 1;
  dt.date.date    = lt.tm_mday;
  dt.date.weekDay = lt.tm_wday;
  M5.Rtc.setDateTime(dt);
}

}  // namespace hal

// Field-compatible stand-ins for the M5StickCPlus RTC structs.
struct RTC_TimeTypeDef { uint8_t Hours, Minutes, Seconds; };
struct RTC_DateTypeDef { uint8_t WeekDay, Month, Date; uint16_t Year; };

namespace hal {
inline void rtcRead(RTC_TimeTypeDef* t, RTC_DateTypeDef* d) {
  m5::rtc_datetime_t dt;
  if (!M5.Rtc.getDateTime(&dt)) return;
  t->Hours = dt.time.hours; t->Minutes = dt.time.minutes; t->Seconds = dt.time.seconds;
  d->WeekDay = dt.date.weekDay; d->Month = dt.date.month;
  d->Date = dt.date.date; d->Year = dt.date.year;
}
}  // namespace hal
