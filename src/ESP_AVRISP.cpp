#include <Arduino.h>
#include "STK500v1.h"
#include "pins.h"

void setup() {
  wdt_enable(WDTO_1S);
  pinMode(PROGRAMMINGLED_PIN, OUTPUT);
  Serial.begin(115200);
}

void loop() {
  static bool programLedSts;

  wdt_reset();
  digitalWrite(PROGRAMMINGLED_PIN, programLedSts ? LOW : HIGH);

  if (Serial.available()) {
    programLedSts = STK500V1.isProgramMode();
    STK500V1.process(Serial.read());
    }
}
