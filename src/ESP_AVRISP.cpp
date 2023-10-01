#include <Arduino.h>
#include "STK500v1.h"
#include "pins.h"


void setup() {
  wdt_disable();
  
  digitalWrite(PROGRAMMINGLED_PIN, HIGH);
  pinMode(PROGRAMMINGLED_PIN, OUTPUT);
  digitalWrite(PROGRAMMINGERR_PIN, HIGH);
  pinMode(PROGRAMMINGERR_PIN, OUTPUT);

  Serial.begin(115200);

  digitalWrite(PROGRAMMINGERR_PIN, LOW);
  digitalWrite(PROGRAMMINGLED_PIN, LOW);
  delay(500);

  wdt_enable(WDTO_1S);
}

void loop() {
  static bool greenLed = false;
  static unsigned long blink;

  wdt_reset();

  if (Serial.available())
    STK500V1.process(&Serial);

  unsigned long time = millis();

  STK500V1Class::Status status = STK500V1.getStatus();
  if (status != STK500V1Class::Status::Programming) {
    blink = time;
    greenLed = (status==STK500V1Class::Status::Done);
  }
  else if ((long)(blink - time) <= 0) {
    blink = (time + 250);
    greenLed = !greenLed;
  }
  digitalWrite(PROGRAMMINGERR_PIN, status==STK500V1Class::Status::Error ? LOW : HIGH);
  digitalWrite(PROGRAMMINGLED_PIN, greenLed ? LOW : HIGH);
}
