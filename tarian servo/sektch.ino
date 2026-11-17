#include <ESP32Servo.h>

// Membuat array untuk 12 objek servo
Servo servos[12];

// Daftar pin GPIO ESP32 yang terhubung ke 12 servo sesuai diagram.json
const int servoPins[12] = {13, 12, 14, 27, 26, 25, 23, 22, 21, 19, 18, 5};

void setup() {
  Serial.begin(115200);

  // Menghubungkan setiap objek servo ke pin masing-masing
  for (int i = 0; i < 12; i++) {
    servos[i].attach(servoPins[i]);
  }
  Serial.println("Sistem Tarian 12 Servo Dimulai!");
}

void loop() {
  // --- TARI GERAKAN 1: Serentak (Semua bergerak bersamaan) ---
  Serial.println("Gerakan: Serentak");
  moveAll(0);
  delay(500);
  moveAll(180);
  delay(500);
  moveAll(90);
  delay(400);

  // --- TARI GERAKAN 2: Efek Gelombang (Wave) ---
  Serial.println("Gerakan: Efek Gelombang");
  for (int i = 0; i < 12; i++) {
    servos[i].write(150);
    delay(60);
  }
  for (int i = 0; i < 12; i++) {
    servos[i].write(30);
    delay(60);
  }

  // --- TARI GERAKAN 3: Selang-seling (Genap & Ganjil Berlawanan) ---
  Serial.println("Gerakan: Selang-seling");
  for (int i = 0; i < 12; i++) {
    if (i % 2 == 0) {
      servos[i].write(180);
    } else {
      servos[i].write(0);
    }
  }
  delay(500);

  for (int i = 0; i < 12; i++) {
    if (i % 2 == 0) {
      servos[i].write(0);
    } else {
      servos[i].write(180);
    }
  }
  delay(500);
}

// Fungsi bantu untuk menggerakkan seluruh servo secara serentak ke sudut tertentu
void moveAll(int angle) {
  for (int i = 0; i < 12; i++) {
    servos[i].write(angle);
  }
}
