#ifndef _UTILS_H
#define _UTILS_H

#include <Arduino.h>

uint16_t CRC16(byte *bfr, int len);

int base64_encode(byte *data, int dataLen, byte *out);
int base64_decode(byte *data, int dataLen, byte *out);
void base64LUT_init();

struct decoded {
    byte *msg;
    uint16_t len;

    decoded();
    decoded(byte* b64, uint16_t b64len);
    decoded(decoded&& other);
    ~decoded();
    operator bool();
};

/*
 * STRUKTURA ZPRÁVY
 *
 * <SOH> (uint8_t)ID (uint8_t)TYPE (uint16_t MSB first)PAYLOAD_LENGTH <STX>
 * (uint8_t[], BASE64)PAYLOAD
 * <ETX> (uint16_t MSB first)CRC16 <EOT>
 * 
 * ODPOVĚĎ
 * 
 * <ACK> nebo <NAK> (uint8_t)ID
 * 
 * PING
 * <ENQ> -> <ACK>
 */

struct msb_u16 {
    byte msb;
    byte lsb;
    inline uint16_t u16() { return (((uint16_t)msb) << 8) | lsb; }
    inline void u16(uint16_t v) { msb = (v >> 8) & 0xFF; lsb = v & 0xFF; }
};

struct msghdr {
    byte _soh;
    byte id;
    byte type;
    msb_u16 payload_len;
    byte _stx;
    inline void init() { _soh = 0x01; _stx = 0x02; }
};

struct msgftr {
    byte _etx;
    msb_u16 crc16;
    byte _eot;
    inline void init() { _etx = 0x03; _eot = 0x04; }
};

#endif