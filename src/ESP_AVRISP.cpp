#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "STK500v1.h"
#include "pins.h"

static WiFiServer serialServer(8880);


void setup() {
  wdt_disable();
  
  digitalWrite(PROGRAMMINGLED_PIN, HIGH);
  pinMode(PROGRAMMINGLED_PIN, OUTPUT);
  digitalWrite(ERRORLED_PIN, HIGH);
  pinMode(ERRORLED_PIN, OUTPUT);

  Serial.begin(115200);
  WiFi.softAPConfig(IPAddress(192, 168, 0, 1),      // ip address
                    IPAddress(0, 0, 0, 0),          // gateway (disabled)
                    IPAddress(255, 255, 255, 0));   // subnet mask
  WiFi.softAP("WiFi ESP-AVR", nullptr, 1, false, 1);
  serialServer.begin();
  serialServer.setNoDelay(true);

  digitalWrite(PROGRAMMINGLED_PIN, LOW);
  digitalWrite(ERRORLED_PIN, LOW);
  delay(500);

  wdt_enable(WDTO_1S);
}

void loop() {
  static WiFiClient client;
  static unsigned long blink;
  static bool greenLed = false;

  wdt_reset();

  if (client.connected()) {
    if (client.available())
      STK500V1.process(&client);
  }
  else if (Serial.available())
    STK500V1.process(&Serial);
  else
    client = serialServer.accept();

  unsigned long time = millis();

  STK500V1Class::Status status = STK500V1.getStatus();
  if (status != STK500V1Class::Status::ProgramMode) {
    blink = time;
    greenLed = (status==STK500V1Class::Status::Done);
  }
  else if ((long)(blink - time) <= 0) {
    blink = (time + 250);
    greenLed = !greenLed;
  }
  digitalWrite(PROGRAMMINGLED_PIN, greenLed ? LOW : HIGH);
  digitalWrite(ERRORLED_PIN, status==STK500V1Class::Status::Error ? LOW : HIGH);
}
