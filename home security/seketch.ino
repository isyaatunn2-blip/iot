#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ==========================================
// DEFINISI PIN GPIO ESP32
// ==========================================
#define PIN_PIR         19
#define PIN_TRIG        5
#define PIN_ECHO        18
#define PIN_BUTTON      4

#define PIN_BUZZER      25
#define PIN_LED_GREEN   14
#define PIN_LED_YELLOW  27
#define PIN_LED_RED     13

// ==========================================
// KONSTANTA SISTEM KEAMANAN
// ==========================================
const float DOOR_THRESHOLD_CM    = 50.0;  // Batas deteksi objek depan pintu (< 50 cm)
const float MIN_VALID_DIST_CM    = 2.0;   // Batas minimum pembacaan valid HC-SR04
const float MAX_VALID_DIST_CM    = 400.0; // Batas maksimum pembacaan valid HC-SR04
const unsigned long DEBOUNCE_MS  = 50;    // Waktu debounce tombol (ms)

// ==========================================
// OBJEK PERANGKAT KERAS
// ==========================================
LiquidCrystal_I2C lcd(0x3F, 16, 2);

// ==========================================
// ENUMERASI & VARIABEL STATUS SISTEM
// ==========================================
enum AlarmState {
  STATE_CLEAR = 0,
  STATE_OBJECT_DETECTED = 1,
  STATE_MOTION_DETECTED = 2,
  STATE_MULTIPLE_DETECT = 3
};

bool isArmed            = false; // Sistem dimulai dalam kondisi DISARM (false)
AlarmState activeAlarm  = STATE_CLEAR;

// Variabel Pembacaan Sensor
bool  pirDetected       = false;
float doorDistanceCm    = 200.0;
bool  objectDetected    = false;

// Variabel Debounce Push Button
int lastButtonReading   = HIGH;
int stableButtonState   = HIGH;
unsigned long lastDebounceTime = 0;

// Variabel Timer Non-Blocking (millis)
unsigned long lastSensorMillis = 0;
unsigned long lastLcdMillis    = 0;
unsigned long lastBuzzerMillis = 0;
bool buzzerPhase               = false;

// Variabel Cache LCD (mencegah layar berkedip/flicker)
String prevLine0 = "";
String prevLine1 = "";

// ==========================================
// FUNGSI PEMBACAAN SENSOR ULTRASONIK + VALIDASI
// ==========================================
float readUltrasonicDistance() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  // Timeout 25000 us (~4.2 meter) agar non-blocking
  long duration = pulseIn(PIN_ECHO, HIGH, 25000);
  if (duration <= 0) {
    return MAX_VALID_DIST_CM; // Jika tidak ada pantulan, anggap area pintu jauh/kosong
  }

  float distance = (duration * 0.0343) / 2.0;

  // Validasi rentang pembacaan sensor
  if (distance < MIN_VALID_DIST_CM || distance > MAX_VALID_DIST_CM) {
    return MAX_VALID_DIST_CM;
  }
  return distance;
}

// ==========================================
// FUNGSI UPDATE TAMPILAN LCD TANPA FLICKER
// ==========================================
void updateLcdText(const char* line0, const char* line1) {
  char buf0[17];
  char buf1[17];
  snprintf(buf0, sizeof(buf0), "%-16s", line0);
  snprintf(buf1, sizeof(buf1), "%-16s", line1);

  String s0 = String(buf0);
  String s1 = String(buf1);

  if (s0 != prevLine0 || s1 != prevLine1) {
    lcd.setCursor(0, 0);
    lcd.print(s0);
    lcd.setCursor(0, 1);
    lcd.print(s1);
    prevLine0 = s0;
    prevLine1 = s1;
  }
}

// ==========================================
// SETUP AWAL SISTEM
// ==========================================
void setup() {
  Serial.begin(115200);

  // Konfigurasi Pin Input
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  // Konfigurasi Pin Output
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);

  // Kondisi Awal: DISARM MODE
  digitalWrite(PIN_LED_GREEN, HIGH);
  digitalWrite(PIN_LED_YELLOW, LOW);
  digitalWrite(PIN_LED_RED, LOW);
  noTone(PIN_BUZZER);

  // Inisialisasi LCD I2C
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  updateLcdText("SMART SECURITY", "STATUS: DISARM");

  Serial.println("==========================================");
  Serial.println("SMART HOME SECURITY SYSTEM - ESP32 READY");
  Serial.println("Mode Awal: DISARM (Tekan tombol untuk ARM)");
  Serial.println("==========================================");
}

// ==========================================
// LOOP UTAMA (NON-BLOCKING)
// ==========================================
void loop() {
  unsigned long currentMillis = millis();

  // ------------------------------------------
  // 1. BACA PUSH BUTTON DENGAN DEBOUNCE
  // ------------------------------------------
  int currentReading = digitalRead(PIN_BUTTON);

  if (currentReading != lastButtonReading) {
    lastDebounceTime = currentMillis;
  }

  if ((currentMillis - lastDebounceTime) > DEBOUNCE_MS) {
    if (currentReading != stableButtonState) {
      stableButtonState = currentReading;

      // Jika tombol ditekan (transisi ke LOW karena INPUT_PULLUP)
      if (stableButtonState == LOW) {
        isArmed = !isArmed; // Toggle antara ARM dan DISARM

        if (!isArmed) {
          // Saat beralih ke DISARM: Matikan semua alarm dan reset status
          activeAlarm = STATE_CLEAR;
          noTone(PIN_BUZZER);
          Serial.println("[MODE] Sistem Dinonaktifkan -> STATUS: DISARM");
        } else {
          activeAlarm = STATE_CLEAR;
          Serial.println("[MODE] Sistem Diaktifkan -> STATUS: ARM");
        }
      }
    }
  }
  lastButtonReading = currentReading;

  // ------------------------------------------
  // 2. BACA SENSOR & LOGIKA KEAMANAN (Setiap 200 ms)
  // ------------------------------------------
  if (currentMillis - lastSensorMillis >= 200) {
    lastSensorMillis = currentMillis;

    pirDetected    = (digitalRead(PIN_PIR) == HIGH);
    doorDistanceCm = readUltrasonicDistance();
    objectDetected = (doorDistanceCm < DOOR_THRESHOLD_CM);

    if (!isArmed) {
      // Aturan DISARM: Alarm, Buzzer, dan LED Merah wajib OFF
      activeAlarm = STATE_CLEAR;
    } else {
      // Aturan ARM: Periksa kondisi sensor
      if (pirDetected && objectDetected) {
        // Kedua sensor mendeteksi aktivitas -> Prioritas Tinggi!
        activeAlarm = STATE_MULTIPLE_DETECT;
      } else if (pirDetected) {
        // PIR mendeteksi gerakan (jangan turunkan jika sebelumnya sudah MULTIPLE_DETECT)
        if (activeAlarm != STATE_MULTIPLE_DETECT) {
          activeAlarm = STATE_MOTION_DETECTED;
        }
      } else if (objectDetected) {
        // HC-SR04 mendeteksi objek < 50 cm
        if (activeAlarm == STATE_CLEAR) {
          activeAlarm = STATE_OBJECT_DETECTED;
        }
      }
      // Catatan: Alarm tetap terkunci (latched) sampai sistem di-DISARM oleh pengguna
    }

    // Pembaruan Indikator LED sesuai kondisi
    if (!isArmed) {
      // Kondisi DISARM: LED Hijau ON, lainnya OFF
      digitalWrite(PIN_LED_GREEN, HIGH);
      digitalWrite(PIN_LED_YELLOW, LOW);
      digitalWrite(PIN_LED_RED, LOW);
    } else {
      digitalWrite(PIN_LED_GREEN, LOW);

      if (activeAlarm == STATE_CLEAR) {
        // Kondisi ARM & Aman: LED Kuning ON, LED Merah OFF
        digitalWrite(PIN_LED_YELLOW, HIGH);
        digitalWrite(PIN_LED_RED, LOW);
      } else {
        // Kondisi ARM & Alarm Aktif: LED Kuning OFF, LED Merah ON
        digitalWrite(PIN_LED_YELLOW, LOW);
        digitalWrite(PIN_LED_RED, HIGH);
      }
    }

    // Log ke Serial Monitor untuk pemantauan pengujian
    Serial.print("[MONITOR] Mode: ");
    Serial.print(isArmed ? "ARM   " : "DISARM");
    Serial.print(" | PIR: ");
    Serial.print(pirDetected ? "MOTION" : "CLEAR ");
    Serial.print(" | Jarak Pintu: ");
    Serial.print(doorDistanceCm, 1);
    Serial.print(" cm | Status: ");
    if (!isArmed) Serial.println("DISARMED");
    else if (activeAlarm == STATE_CLEAR) Serial.println("SAFE (SIAGA)");
    else if (activeAlarm == STATE_OBJECT_DETECTED) Serial.println("WARNING: OBJECT DETECTED");
    else if (activeAlarm == STATE_MOTION_DETECTED) Serial.println("WARNING: MOTION DETECTED");
    else if (activeAlarm == STATE_MULTIPLE_DETECT) Serial.println("ALERT: MULTIPLE DETECT!");
  }

  // ------------------------------------------
  // 3. KONTROL SIRINE BUZZER (Non-Blocking)
  // ------------------------------------------
  if (!isArmed || activeAlarm == STATE_CLEAR) {
    noTone(PIN_BUZZER);
  } else {
    // Kecepatan sirine lebih tinggi jika kedua sensor aktif (Prioritas Tinggi)
    unsigned long alarmInterval = (activeAlarm == STATE_MULTIPLE_DETECT) ? 150 : 300;

    if (currentMillis - lastBuzzerMillis >= alarmInterval) {
      lastBuzzerMillis = currentMillis;
      buzzerPhase = !buzzerPhase;

      if (activeAlarm == STATE_MULTIPLE_DETECT) {
        // Sirine Prioritas Tinggi (2 nada cepat: 1800 Hz & 1200 Hz)
        tone(PIN_BUZZER, buzzerPhase ? 1800 : 1200);
      } else {
        // Alarm Peringatan Tunggal (Beep putus-putus 1000 Hz)
        if (buzzerPhase) {
          tone(PIN_BUZZER, 1000);
        } else {
          noTone(PIN_BUZZER);
        }
      }
    }
  }

  // ------------------------------------------
  // 4. UPDATE TAMPILAN LCD 16x2 (Setiap 250 ms)
  // ------------------------------------------
  if (currentMillis - lastLcdMillis >= 250) {
    lastLcdMillis = currentMillis;

    if (!isArmed) {
      updateLcdText("SMART SECURITY", "STATUS: DISARM");
    } else {
      switch (activeAlarm) {
        case STATE_CLEAR:
          updateLcdText("SMART SECURITY", "STATUS: ARM");
          break;
        case STATE_MOTION_DETECTED:
          updateLcdText("WARNING!", "MOTION DETECTED");
          break;
        case STATE_OBJECT_DETECTED:
          updateLcdText("WARNING!", "OBJECT DETECTED");
          break;
        case STATE_MULTIPLE_DETECT:
          updateLcdText("SECURITY ALERT!", "MULTIPLE DETECT");
          break;
      }
    }
  }
}
