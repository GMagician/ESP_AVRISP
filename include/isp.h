#pragma once

class ISPClass {
  public:
    bool enterProgramMode(void);
    void exitProgramMode(void);
    uint8_t rawCommand(uint8_t instruction1, uint8_t instruction2, uint8_t instruction3, uint8_t instruction4);
    void writeFlash(uint16_t address, uint16_t data);
    bool commitFlash(uint16_t page);
    void writeEEprom(uint16_t address, uint8_t data);
    bool commitEEprom(uint16_t page);
    uint16_t readFlash(uint16_t address);
    uint8_t readEEprom(uint16_t address);
    uint32_t readSignature(void);

    bool waitIdle(void);
};

extern ISPClass ISP;
