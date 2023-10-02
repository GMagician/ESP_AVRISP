#include <Arduino.h>
#include "STK500v1.h"
#include "command.h"
#include "isp.h"

void STK500V1Class::process(Stream* stream) {
  this->stream = stream;

  switch (streamRead()) {
    case Cmnd_STK_GET_SYNC:
      emptyReply();
      break;

    case Cmnd_STK_GET_SIGN_ON:
      if (streamRead() == Sync_CRC_EOP) {
        socketWrite(Resp_STK_INSYNC);
        streamWrite(STK_SIGN_ON_MESSAGE);
        socketWrite(Resp_STK_OK);
      }
      else
        socketWrite(Resp_STK_NOSYNC);
    break;

    case Cmnd_STK_GET_PARAMETER:
      getParameters();
      break;

    case Cmnd_STK_SET_DEVICE:
      setDeviceParameters();
      emptyReply();
      break;

    case Cmnd_STK_SET_DEVICE_EXT:
      setExtendedDeviceParameters();
      emptyReply();
      break;

    case Cmnd_STK_ENTER_PROGMODE:
      if (status != Status::ProgramMode)
        status = ISP.enterProgramMode() ? Status::Error : Status::ProgramMode;
      emptyReply();
      break;

    case Cmnd_STK_LEAVE_PROGMODE:
      if (status != Status::Error)
        status = Status::Done;
      ISP.exitProgramMode();
      emptyReply();
      break;

    case Cmnd_STK_LOAD_ADDRESS:
      curWordAddress = streamRead() | (uint16_t(streamRead()) << 8);
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
      socketWrite(Resp_STK_NOSYNC);
      break;

    default:
      socketWrite((streamRead() == Sync_CRC_EOP) ? Resp_STK_UNKNOWN : Resp_STK_NOSYNC);
      break;
  }
}

STK500V1Class::Status STK500V1Class::getStatus(void) {
  return status;
}

uint8_t STK500V1Class::streamRead() {
  while(!stream->available())
    yield();

  return stream->read();
}

uint8_t * STK500V1Class::streamReadBytes(uint16_t bytes) {
  uint8_t *buffer = (uint8_t *)malloc(bytes);
  if (buffer != nullptr) {
    for (uint32_t i = 0; i < bytes; ++i) {
      buffer[i] = streamRead();
    }
  }

  return buffer;
}

inline void STK500V1Class::socketWrite(uint8_t b) {
  stream->print((char)b);
}

inline void STK500V1Class::streamWrite(const char *s) {
  stream->print(s);
}

void STK500V1Class::emptyReply() {
  if (streamRead() != Sync_CRC_EOP)
    socketWrite(Resp_STK_NOSYNC);
  else {
    socketWrite(Resp_STK_INSYNC);
    socketWrite(Resp_STK_OK);
  }
}

void STK500V1Class::byteReply(uint8_t b) {
  if (streamRead() != Sync_CRC_EOP)
    socketWrite(Resp_STK_NOSYNC);
  else {
    socketWrite(Resp_STK_INSYNC);
    socketWrite(b);
    socketWrite(Resp_STK_OK);
  }
}

void STK500V1Class::wordReply(uint16_t w) {
  if (streamRead() != Sync_CRC_EOP)
    socketWrite(Resp_STK_NOSYNC);
  else {
    socketWrite(Resp_STK_INSYNC);
    socketWrite(w);
    socketWrite(w >> 8);
    socketWrite(Resp_STK_OK);
  }
}

void STK500V1Class::getParameters() {
  switch (streamRead()) {
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

void STK500V1Class::setDeviceParameters() {
  uint8_t * paramPtr = (uint8_t *)&devParam;
  for (uint8_t i = 0; i < sizeof(devParam); ++i, ++paramPtr)
    *paramPtr = streamRead();
  // Fix big endian values
  // 16 bits
  devParam.eepromPoll = (devParam.eepromPoll >> 8) | (devParam.eepromPoll << 8);
  devParam.pageSize   = (devParam.pageSize >> 8) | (devParam.pageSize << 8);
  devParam.eepromSize = (devParam.eepromSize >> 8) | (devParam.eepromSize << 8);
  // 32 bits
  devParam.flashSize = (devParam.flashSize >> 24) |
                       ((devParam.flashSize & 0x00FF0000) >> 8) |
                       ((devParam.flashSize & 0x0000FF00) << 8) |
                       (devParam.flashSize << 24);
}

void STK500V1Class::setExtendedDeviceParameters() {
  uint8_t * extDevParamPtr = (uint8_t *)&extDevParam;
  for (uint8_t i = 0; i < sizeof(extDevParam); ++i, ++extDevParamPtr)
    *extDevParamPtr = streamRead();
}

void STK500V1Class::universal() {
  uint8_t b = ISP.rawCommand(streamRead(), streamRead(), streamRead(), streamRead());
  byteReply(b);
}

uint16_t STK500V1Class::getFlashPage() {
 switch (devParam.pageSize) {
    case 32:
    case 64:
    case 128:
    case 256:
      return curWordAddress & ~((devParam.pageSize >> 1) - 1);
    default:
      return curWordAddress;
  }
}

uint16_t STK500V1Class::getEEpromPage() {
  return (curWordAddress << 1) & ~((extDevParam.eepromPageSize >> 1) - 1);   // here is a word address, get the byte address
}

void STK500V1Class::programPage() {
  uint16_t length = (uint16_t(streamRead()) << 8) | streamRead();
  if (length > 256)
    socketWrite(Resp_STK_FAILED);
  else {
    // flash memory @here, (length) bytes
    uint8_t memType = streamRead();
    switch (memType) {
      case 'F':
        if (writeFlash(length))
          status = Status::Error;
        break;

      case 'E':
        if (writeEEprom(length))
          status = Status::Error;
        break;

      default:
        socketWrite(Resp_STK_FAILED);
        break;
    }
  }
}

bool STK500V1Class::writeFlash(uint16_t length) {
  uint8_t result;

  uint8_t *buffer = streamReadBytes(length);
  if (buffer == nullptr)
    result = Resp_STK_FAILED;
  else {
    if (streamRead() != Sync_CRC_EOP)
      result = Resp_STK_NOSYNC;
    else {
      socketWrite(Resp_STK_INSYNC);

      result = Resp_STK_OK;
      uint16_t prevPage = getFlashPage();
      for (uint16_t i = 0; i < length; i += 2, ++curWordAddress) {
        uint16_t curPage = getFlashPage();
        if (curPage != prevPage) {
          if (ISP.commitFlash(prevPage))
            result = Resp_STK_FAILED;
          prevPage = curPage;
        }
        ISP.writeFlash(curWordAddress, (uint16_t(buffer[i+1]) << 8) | buffer[i]);
      }
      if (ISP.commitFlash(prevPage))
        result = Resp_STK_FAILED;
    }
    free (buffer);
  }
  socketWrite(result);

  return result != Resp_STK_OK;
}

bool STK500V1Class::writeEEprom(uint16_t length) {
  uint8_t result;

  uint8_t *buffer = streamReadBytes(length);
  if (buffer == nullptr)
    result = Resp_STK_FAILED;
  else {
    if (length > devParam.eepromSize)
      result = Resp_STK_FAILED;
    else {
      if (streamRead() != Sync_CRC_EOP)
        result = Resp_STK_NOSYNC;
      else {
        socketWrite(Resp_STK_INSYNC);

        result = Resp_STK_OK;
        uint32_t addr = curWordAddress << 1;    // here is a word address, get the byte address
        uint16_t prevPage = getEEpromPage();
        for (uint16_t i = 0; i < length; ++i, ++addr) {
          uint16_t curPage = getEEpromPage();
          if (curPage != prevPage) {
            if (ISP.commitEEprom(prevPage))
              result = Resp_STK_FAILED;
            prevPage = curPage;
          }
          ISP.writeEEprom(addr, buffer[i]);
        }
        if (ISP.commitEEprom(prevPage))
          result = Resp_STK_FAILED;
      }
    }
    free (buffer);
  }
  socketWrite(result);

  return result != Resp_STK_OK;
}

void STK500V1Class::readPage() {
  uint16_t length = (uint16_t(streamRead()) << 8) | streamRead();
  if (length > 256)
    socketWrite(Resp_STK_FAILED);
  else {
    uint8_t memType = streamRead();
    if (streamRead() != Sync_CRC_EOP)
      socketWrite(Resp_STK_NOSYNC);
    else {
      switch (memType) {
        case 'F':
          readFlash(length);
          break;

        case 'E':
          readEEProm(length);
          break;

        default:
          socketWrite(Resp_STK_FAILED);
          break;
      }
    }
  }
}

void STK500V1Class::readFlash(uint16_t length) {
  socketWrite(Resp_STK_INSYNC);
  for (uint16_t i = 0; i < length; i += 2, ++curWordAddress) {
    uint16_t w = ISP.readFlash(curWordAddress);
    socketWrite(w);
    socketWrite(w >> 8);
  }
  socketWrite(Resp_STK_OK);
}

void STK500V1Class::readEEProm(uint16_t length) {
  socketWrite(Resp_STK_INSYNC);
  uint32_t addr = curWordAddress << 1;  // here again we have a word address
  for (uint16_t i = 0; i < length; ++i, ++addr) {
    uint8_t b = ISP.readEEprom(addr);
    socketWrite(b);
  }
  socketWrite(Resp_STK_OK);
}

void STK500V1Class::readSignature() {
  if (streamRead() != Sync_CRC_EOP)
    socketWrite(Resp_STK_NOSYNC);
  else {
    socketWrite(Resp_STK_INSYNC);
    uint32_t signature = ISP.readSignature();
    socketWrite(signature >> 16);
    socketWrite(signature >> 8);
    socketWrite(signature);
    socketWrite(Resp_STK_OK);
  }
}

STK500V1Class STK500V1;
