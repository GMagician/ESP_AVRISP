#include <SPI.h>
#include "isp.h"
#include "pins.h"

void ISPClass::enterProgramMode() {
  SPI.begin();
  SPI.setDataMode(SPI_MODE1);
  SPI.setClockDivider(SPI_CLOCK_DIV128);

  digitalWrite(RESET_PIN, HIGH);
  pinMode(RESET_PIN, OUTPUT);
  delayMicroseconds(50);
  digitalWrite(RESET_PIN, LOW);
  delay(20);
  rawCommand(0xAC, 0x53, 0x00, 0x00);
}

void ISPClass::exitProgramMode() {
  SPI.end();
  digitalWrite(RESET_PIN, HIGH);
  pinMode(RESET_PIN, INPUT);
}

uint8_t ISPClass::rawCommand(uint8_t instruction1, uint8_t instruction2, uint8_t instruction3, uint8_t instruction4) {
  SPI.transfer(instruction1);
  SPI.transfer(instruction2);
  SPI.transfer(instruction3);

  return SPI.transfer(instruction4);
}

void ISPClass::writeFlash(uint16_t address, uint16_t data) {
  rawCommand(0x40, address >> 8, address, data);
  rawCommand(0x48, address >> 8, address, data >> 8);
}

void ISPClass::commitFlash(uint16_t page) {
  rawCommand(0x4C, page >> 8, page, 0x00);
  waitIdle();
}

void ISPClass::writeEEprom(uint16_t address, uint8_t data) {
  rawCommand(0xC1, address >> 8, address, data);
}

void ISPClass::commitEEprom(uint16_t page) {
  rawCommand(0xC2, page >> 8, page, 0x00);
  waitIdle();
}

uint16_t ISPClass::readFlash(uint16_t address) {
  uint8_t lowByte = rawCommand(0x20, address >> 8, address, 0x00);
  uint8_t highByte = rawCommand(0x28, address >> 8, address, 0x00);

  return (uint16_t(highByte) << 8) | lowByte;
}

uint8_t ISPClass::readEEprom(uint16_t addr) {
  return rawCommand(0xA0, (addr >> 8), addr, 0xFF);
}

uint32_t ISPClass::readSignature() {
  uint32_t signature = 0x00000000;
  for (uint8_t i = 0; i < 3; ++i)
    signature = (signature << 8) | rawCommand(0x30, 0x00, i, 0x00);

  return signature;
}

ISPClass ISP;

void ISPClass::waitIdle(void){
  while (rawCommand(0xF0, 0x00, 0x00, 0x00) & 1)
    ;
}
