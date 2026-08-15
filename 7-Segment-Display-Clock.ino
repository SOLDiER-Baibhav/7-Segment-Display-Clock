#include <SevSeg.h>
#include <RTClib.h>

SevSeg Display;
RTC_DS3231 rtc;
bool rtc_ok = true;

// ---------------- INTERNAL CLOCK ----------------
int h24 = 12; 
int mn = 0;
int sc = 0;
int dy = 27;
int mo = 5;
const int yr = 2025;  // Fixed year

// ---------------- STATE ----------------
bool is24Hour = true; 
int mode = 0;   // 0 = TIME, 1 = DATE
bool editing = false;

// ---------------- BUTTONS ----------------
const int BTN_HR  = 9;
const int BTN_MN  = 11;
const int BTN_FMT = 10;
const int BTN_DAT = 12;

// ---------------- LED ----------------
const int LEDPIN = 13;

// ---------------- TIMERS ----------------
unsigned long lastBtn[4] = {0,0,0,0};
unsigned long rtcTimer = 0;
unsigned long tickTimer = 0;

const unsigned long debounceMs = 200;
const unsigned long RTC_DELAY = 3000;   // 3 seconds
const unsigned long BLINK_DURATION = 200; // LED ON for 200 ms during commit

void setup() {
  Serial.begin(9600);
  
  pinMode(BTN_HR, INPUT_PULLUP);
  pinMode(BTN_MN, INPUT_PULLUP);
  pinMode(BTN_FMT, INPUT_PULLUP);
  pinMode(BTN_DAT, INPUT_PULLUP);
  pinMode(LEDPIN, OUTPUT);

  // SevSeg
  byte digitPins[]   = {A3, A2, A1, A0};
  byte segmentPins[] = {2, 3, 4, 5, 6, 7, 8};
  Display.begin(COMMON_CATHODE, 4, digitPins, segmentPins, true);
  //Display.begin(COMMON_ANODE, 4, digitPins, segmentPins, true);
  Display.setBrightness(90);

  // RTC
  if (!rtc.begin()) {
    rtc_ok = false;
    Serial.println("RTC not found!");
  } else {
    if (rtc.lostPower()) {
      Serial.println("RTC lost power, setting default time");
      rtc.adjust(DateTime(yr, mo, dy, h24, mn, 0));
    }
    DateTime now = rtc.now();
    h24 = now.hour();
    mn  = now.minute();
    sc  = now.second();
    dy  = now.day();
    mo  = now.month();
    Serial.print("RTC read at startup: ");
    Serial.print(h24); Serial.print(":"); Serial.print(mn);
    Serial.print(" "); Serial.print(dy); Serial.print("/"); Serial.println(mo);
  }

  tickTimer = millis();
}

void loop() {
  unsigned long ms = millis();

  // ----------------- RTC SYNC / TICK -----------------
  if (!editing) {
    if (rtc_ok && ms - tickTimer >= 1000) {
      tickTimer = ms;
      DateTime now = rtc.now();
      h24 = now.hour();
      mn  = now.minute();
      sc  = now.second();
      dy  = now.day();
      mo  = now.month();
    } else if (!rtc_ok && ms - tickTimer >= 1000) {
      tickTimer = ms;
      sc++;
      if (sc >= 60) { sc = 0; mn++; }
      if (mn >= 60) { mn = 0; h24++; }
      if (h24 >= 24) h24 = 0;
    }
  }

  // ----------------- BUTTONS -----------------
  // Date mode toggle
  if (digitalRead(BTN_DAT) == LOW && ms - lastBtn[3] > debounceMs) {
    lastBtn[3] = ms;
    mode = !mode;
    Serial.println(mode == 0 ? "Switched to TIME display" : "Switched to DATE display");
  }

  // 12/24 toggle
  if (digitalRead(BTN_FMT) == LOW && ms - lastBtn[2] > debounceMs) {
    lastBtn[2] = ms;
    is24Hour = !is24Hour;
    Serial.println(is24Hour ? "Switched to 24-Hour format" : "Switched to 12-Hour format");
  }

  // Hour / Day button
  if (digitalRead(BTN_HR) == LOW && ms - lastBtn[0] > debounceMs) {
    lastBtn[0] = ms;
    editing = true;
    rtcTimer = ms;

    if (mode == 0) {
      h24 = (h24 + 1) % 24;
      sc = 0;
      Serial.print("Hour incremented: "); Serial.println(h24);
    } else {
      dy++;
      if (dy > 31) dy = 1;
      Serial.print("Day incremented: "); Serial.println(dy);
    }
  }

  // Minute / Month button
  if (digitalRead(BTN_MN) == LOW && ms - lastBtn[1] > debounceMs) {
    lastBtn[1] = ms;
    editing = true;
    rtcTimer = ms;

    if (mode == 0) {
      mn = (mn + 1) % 60;
      sc = 0;
      Serial.print("Minute incremented: "); Serial.println(mn);
    } else {
      mo++;
      if (mo > 12) mo = 1;
      Serial.print("Month incremented: "); Serial.println(mo);
    }
  }

  // ----------------- LED & RTC COMMIT -----------------
  if (editing) {
    if (ms - rtcTimer < BLINK_DURATION) digitalWrite(LEDPIN, HIGH);
    else digitalWrite(LEDPIN, LOW);

    if (ms - rtcTimer >= RTC_DELAY) {
      // Ensure valid values
      if (h24 < 0) h24 = 0; if (h24 > 23) h24 = 23;
      if (mn < 0) mn = 0; if (mn > 59) mn = 59;
      if (dy < 1) dy = 1; if (dy > 31) dy = 31;
      if (mo < 1) mo = 1; if (mo > 12) mo = 12;

      if (rtc_ok) {
        rtc.adjust(DateTime(yr, mo, dy, h24, mn, 0));
        Serial.print("RTC updated: ");
        Serial.print(h24); Serial.print(":"); Serial.print(mn);
        Serial.print(" "); Serial.print(dy); Serial.print("/"); Serial.println(mo);
      }
      editing = false;
      Serial.println("Editing finished, RTC commit done.");
    }
  } else {
    digitalWrite(LEDPIN, (mode == 0 ? HIGH : LOW));
  }

  // ----------------- DISPLAY -----------------
  int dispH = (is24Hour ? h24 : (h24 % 12 == 0 ? 12 : h24 % 12));
  int value;
  if (mode == 0) value = dispH * 100 + mn; // HHMM
  else           value = dy * 100 + mo;    // DDMM

  Display.setNumber(value);
  Display.refreshDisplay();
  delay(5);
}


