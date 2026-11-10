#include <LiquidCrystal_12C.h>

LiquidCrystal_I2C lcd(0x27,16,2);

int jam = 0;
int menit = 0;
int detik = 0;

void setup() {
 serial.begin(115200);

 lcd.int();
 lcd.backlight();

 lcd.setCursor(0, 0);
 lcd.print("JAM DIGITAL");
}

void loop() {
  lcd.setCursor(4,1);

  if (jam < 10) lcdprint("0");
  lcd.print(jam);
  lcd.print(":");

  if (menit < 10) lcd.print("0");
  lcd.print(menit);
  lcd.print(detik);
  lcd.print(":");

  if (detik < 10) lcd.print("0");
  
  delay(1000);

  detik++;

  if (detik >=60) {
    detik = 0:
    menit++;
  }

  if (menit >=60) {
    menit = 0;
    jam++;
  }

if(jam >=24) {
  jam = 0;
}
}

