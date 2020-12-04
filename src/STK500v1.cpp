#include <Arduino.h>
#include "STK500v1.h"
#include "command.h"
#include "isp.h"

void STK500V1Class::process(char cmd) {
  switch (cmd) {
    case Cmnd_STK_GET_SYNC:
      emptyReply();
      break;

    case Cmnd_STK_GET_SIGN_ON:
      if (serialRead() == Sync_CRC_EOP) {
        serialWrite(Resp_STK_INSYNC);
        serialWrite(STK_SIGN_ON_MESSAGE);
        serialWrite(Resp_STK_OK);
      }
      else
        serialWrite(Resp_STK_NOSYNC);
    break;

    case Cmnd_STK_GET_PARAMETER:
      getVersion();
      break;

    case Cmnd_STK_SET_DEVICE:
      setParameters();
      emptyReply();
      break;

    case Cmnd_STK_SET_DEVICE_EXT:
      serialReadBytes(5);   // Not used
      emptyReply();
      break;

    case Cmnd_STK_ENTER_PROGMODE:
      if (!programMode)
        ISP.enterProgramMode();
      programMode = true;
      emptyReply();
      break;

    case Cmnd_STK_LEAVE_PROGMODE:
      ISP.exitProgramMode();
      programMode = false;
      emptyReply();
      break;

    case Cmnd_STK_LOAD_ADDRESS:
      curWordAddress = serialRead() | (uint16_t(serialRead()) << 8);
      emptyReply();
      break;

    case Cmnd_STK_UNIVERSAL:
      universal();
      break;

    case Cmnd_STK_PROG_PAGE:
      programPage();
      break;

    case Cmnd_STK_READ_PAGE:
      readPage();
      break;

    case Cmnd_STK_READ_SIGN:
      readSignature();
      break;

    case Sync_CRC_EOP:
      // expecting a command, not Sync_CRC_EOP
      // this is how we can get back in sync
      serialWrite(Resp_STK_NOSYNC);
      break;

    default:
      // anything else we will return Resp_STK_UNKNOWN
      serialWrite((serialRead() == Sync_CRC_EOP) ? Resp_STK_UNKNOWN : Resp_STK_NOSYNC);
      break;
  }
}

bool STK500V1Class::isProgramMode() {
  return programMode;
}

uint8_t STK500V1Class::serialRead() {
  while(!Serial.available())
    yield();

  return Serial.read();
}

uint8_t * STK500V1Class::serialReadBytes(uint16_t bytes) {
  uint8_t *buffer = (uint8_t *)malloc(bytes);
  if (buffer != nullptr) {
    for (uint32_t i = 0; i < bytes; ++i) {
      buffer[i] = serialRead();
    }
  }

  return buffer;
}

inline void STK500V1Class::serialWrite(uint8_t b) {
  Serial.print((char)b);
}

inline void STK500V1Class::serialWrite(const char *s) {
  Serial.print(s);
}

void STK500V1Class::emptyReply() {
  if (serialRead() == Sync_CRC_EOP) {
    serialWrite(Resp_STK_INSYNC);
    serialWrite(Resp_STK_OK);
  }
  else
    serialWrite(Resp_STK_NOSYNC);
  }

void STK500V1Class::byteReply(uint8_t b) {
  if (serialRead() == Sync_CRC_EOP) {
    serialWrite(Resp_STK_INSYNC);
    serialWrite(b);
    serialWrite(Resp_STK_OK);
  }
  else
    serialWrite(Resp_STK_NOSYNC);
}

void STK500V1Class::wordReply(uint16_t w) {
  if (serialRead() == Sync_CRC_EOP) {
    serialWrite(Resp_STK_INSYNC);
    serialWrite(w);
    serialWrite(w >> 8);
    serialWrite(Resp_STK_OK);
  }
  else
    serialWrite(Resp_STK_NOSYNC);
}

void STK500V1Class::getVersion() {
  switch (serialRead()) {
    case Parm_STK_HW_VER:
      byteReply(HWVER);
      break;

    case Parm_STK_SW_MAJOR:
      byteReply(SWMAJ);
      break;

    case Parm_STK_SW_MINOR:
      byteReply(SWMIN);
      break;

    case Parm_STK_PROGMODE:
      byteReply('S');   // serial programmer
      break;

    default:
      byteReply(0x00);
      break;
  }
}

void STK500V1Class::setParameters() {
  uint8_t * paramPtr = (uint8_t *)&param;
  for (uint8_t i = 0; i < sizeof(param); ++i, ++paramPtr)
    *paramPtr = serialRead();
  // Fix big endian values
  // 16 bits
  param.eepromPoll = (param.eepromPoll >> 8) | (param.eepromPoll << 8);
  param.pageSize   = (param.pageSize >> 8) | (param.pageSize << 8);
  param.eepromSize = (param.eepromSize >> 8) | (param.eepromSize << 8);
  // 32 bits
  param.flashSize = (param.flashSize >> 24) |
                    ((param.flashSize & 0x00FF0000) >> 16) |
                    ((param.flashSize & 0x0000FF00) << 8) |
                    (param.flashSize << 24);
}

void STK500V1Class::universal() {
  uint8_t b = ISP.rawCommand(serialRead(), serialRead(), serialRead(), serialRead());
  byteReply(b);
}

uint16_t STK500V1Class::getFlashPage() {
  switch (param.pageSize) {
    case 32:
    case 64:
    case 128:
    case 256:
      return curWordAddress & ~((param.pageSize >> 1) - 1);
    default:
      return curWordAddress;
  }
}

uint16_t STK500V1Class::getEEpromPage() {
  return curWordAddress & ~(param.pageSize - 1);
}

void STK500V1Class::programPage() {
  uint16_t length = (uint16_t(serialRead()) << 8) | serialRead();
  if (length > 256)
    serialWrite(Resp_STK_FAILED);
  else {
    // flash memory @here, (length) bytes
    uint8_t memType = serialRead();
    switch (memType) {
      case 'F':
        writeFlash(length);
        break;

      case 'E':
        writeEEprom(length);
        break;

      default:
        serialWrite(Resp_STK_FAILED);
        break;
    }
  }
}

void STK500V1Class::writeFlash(uint16_t length) {
  uint8_t *buffer = serialReadBytes(length);
  if (buffer == nullptr) {
    serialWrite(Resp_STK_FAILED);
    return;
  }

  if (serialRead() != Sync_CRC_EOP)
    serialWrite(Resp_STK_NOSYNC);
  else {
    serialWrite(Resp_STK_INSYNC);

    uint16_t prevPage = getFlashPage();
    for (uint16_t i = 0; i < length; i += 2, ++curWordAddress) {
      uint16_t curPage = getFlashPage();
      if (curPage != prevPage) {
        ISP.commitFlash(prevPage);
        prevPage = curPage;
      }
      ISP.writeFlash(curWordAddress, (uint16_t(buffer[i+1]) << 8) | buffer[i]);
    }
    ISP.commitFlash(prevPage);

    serialWrite(Resp_STK_OK);
  }
  free (buffer);
}

void STK500V1Class::writeEEprom(uint16_t length) {
  uint8_t *buffer = serialReadBytes(length);
  if (buffer == nullptr) {
    serialWrite(Resp_STK_FAILED);
    return;
  }

  if (length > param.eepromSize)
    serialWrite(Resp_STK_FAILED);
  else {
    if (serialRead() != Sync_CRC_EOP)
      serialWrite(Resp_STK_NOSYNC);
    else {
      serialWrite(Resp_STK_INSYNC);

      uint32_t addr = curWordAddress << 1;    // here is a word address, get the byte address
      uint16_t prevPage = getEEpromPage();
      for (uint16_t i = 0; i < length; ++i, ++addr) {
        uint16_t curPage = getEEpromPage();
        if (curPage != prevPage) {
          ISP.commitEEprom(prevPage);
          prevPage = curPage;
        }
        ISP.writeEEprom(addr, buffer[i]);
      }
      ISP.commitEEprom(prevPage);

      serialWrite(Resp_STK_OK);
    }
  }
  free (buffer);
}

void STK500V1Class::readPage() {
  uint16_t length = (uint16_t(serialRead()) << 8) | serialRead();
  if (length > 256)
    serialWrite(Resp_STK_FAILED);
  else {
    uint8_t memType = serialRead();
    if (serialRead() != Sync_CRC_EOP)
      serialWrite(Resp_STK_NOSYNC);
    else {
      switch (memType) {
        case 'F':
          readFlash(length);
          break;

        case 'E':
          readEEProm(length);
          break;

        default:
          serialWrite(Resp_STK_FAILED);
          break;
      }
    }
  }
}

void STK500V1Class::readFlash(uint16_t length) {
  serialWrite(Resp_STK_INSYNC);
  for (uint16_t i = 0; i < length; i += 2, ++curWordAddress) {
    uint16_t w = ISP.readFlash(curWordAddress);
    serialWrite(w);
    serialWrite(w >> 8);
  }
  serialWrite(Resp_STK_OK);
}

void STK500V1Class::readEEProm(uint16_t length) {
  serialWrite(Resp_STK_INSYNC);
  // here again we have a word address
  uint32_t addr = curWordAddress << 1;
  for (uint16_t i = 0; i < length; ++i, ++addr) {
    uint8_t b = ISP.readEEprom(addr);
    serialWrite(b);
  }
  serialWrite(Resp_STK_OK);
}

void STK500V1Class::readSignature() {
  if (serialRead() != Sync_CRC_EOP)
    serialWrite(Resp_STK_NOSYNC);
  else {
    serialWrite(Resp_STK_INSYNC);
    uint32_t signature = ISP.readSignature();
    serialWrite(signature >> 16);
    serialWrite(signature >> 8);
    serialWrite(signature);
    serialWrite(Resp_STK_OK);
  }
}

STK500V1Class STK500V1;
