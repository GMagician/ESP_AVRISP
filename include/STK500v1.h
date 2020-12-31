#pragma once

#include <stdint.h>

class STK500V1Class {
  public:
    void process(char cmd);
    bool isProgramMode(void);
    bool isProgrammingError(void);

  private:
    #define HWVER   2
    #define SWMAJ   1
    #define SWMIN   18

    bool programMode,
         programmingError;
    uint16_t curWordAddress;
    struct Parameter {
      uint8_t deviceCode;
      uint8_t revision;
      uint8_t progType;
      uint8_t parMode;
      uint8_t polling;
      uint8_t selfTimed;
      uint8_t lockBytes;
      uint8_t fuseBytes;
      uint8_t flashPoll;
      uint8_t :8;
      uint16_t eepromPoll;
      uint16_t pageSize;
      uint16_t eepromSize;
      uint32_t flashSize;
    } param;
    struct ExtendedParameter {
      uint8_t commandSize;
      uint8_t eepromPageSize;
      uint8_t signalPagel;
      uint8_t signalBS2;
      uint8_t resetDisable;
    } extParam;

    uint8_t serialRead(void);
    uint8_t *serialReadBytes(uint16_t bytes);
    void serialWrite(uint8_t c);
    void serialWrite(const char *s);
    void emptyReply(void);
    void byteReply(uint8_t b);
    void wordReply(uint16_t w);
    void getVersion(void);
    void setParameters(void);
    void setExtendedParameters(void);
    void universal(void);
    uint16_t getFlashPage(void);
    uint16_t getEEpromPage(void);
    void programPage(void);
    bool writeFlash(uint16_t length);
    bool writeEEprom(uint16_t length);
    void readPage(void);
    void readFlash(uint16_t length);
    void readEEProm(uint16_t length);
    void readSignature(void);
};

extern STK500V1Class STK500V1;
