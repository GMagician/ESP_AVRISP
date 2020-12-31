#include <Arduino.h>
#include "STK500v1.h"
#include "pins.h"

void setup() {
  wdt_enable(WDTO_1S);

  pinMode(PROGRAMMINGLED_PIN, OUTPUT);
  digitalWrite(PROGRAMMINGLED_PIN, HIGH);
  pinMode(PROGRAMMINGERR_PIN, OUTPUT);
  digitalWrite(PROGRAMMINGERR_PIN, HIGH);

  Serial.begin(115200);
}

void loop() {
  wdt_reset();

  if (Serial.available()) {
    STK500V1.process(Serial.read());

    digitalWrite(PROGRAMMINGLED_PIN, STK500V1.isProgramMode() ? LOW : HIGH);
    digitalWrite(PROGRAMMINGERR_PIN, STK500V1.isProgrammingError() ? LOW : HIGH);
    }
}
