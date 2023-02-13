#include "utils.h"

int base64LUT[256];
const char base64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

//F0 6D 65 6F 77
byte APDU[] = {
    0x00, /* CLA */
    0xA4, /* INS */
    0x04, /* P1  */
    0x00, /* P2  */
    0x05, /* Length of AID  */
    0xF0, 0x6D, 0x65, 0x6F, 0x77,   /* AID */
    0x00  /* Le  */
};
byte lenAPDU = sizeof(APDU);

//CRC16/XMODEM
uint16_t CRC16(byte *bfr, int len) {
    static const uint16_t CRC16POLYNOMIAL = 0x1021;
    uint16_t crc = 0;

    for (int i = 0; i < len; i++) {
        crc ^= bfr[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ CRC16POLYNOMIAL;
            else crc <<= 1;
        }
    }

    return crc;
}

int base64_encode(byte *data, int dataLen, byte *out) {
    int val = 0, valb = -6;
    int outIter = 0;

    for (int i = 0; i < dataLen; i++) {
        val = (val << 8) + data[i];
        valb += 8;
        while (valb >= 0) {
            out[outIter++] = base64chars[(val >> valb) & 0x3F];
            valb -= 6;
        }
    }
    if (valb > -6) out[outIter++] = base64chars[((val << 8) >> (valb + 8)) & 0x3F];
    while (outIter % 4) out[outIter++] = '=';
    return outIter;
}

int base64_decode(byte *data, int dataLen, byte *out) {
    int val = 0, valb = -8;
    int outIter = 0;

    for (int i = 0; i < dataLen; i++) {
        byte c = data[i];
        if (base64LUT[c] == -1) break;
        val = (val << 6) + base64LUT[c];
        valb += 6;
        if (valb >= 0) {
            out[outIter++] = char((val >> valb) & 0xFF);
            valb -= 8;
        }
    }

    return outIter;
}

void base64LUT_init() {
    for (int i = 0; i < 256; i++) base64LUT[i] = -1;
    for (int i = 0; i < 64; i++) base64LUT[base64chars[i]] = i;
}



decoded::decoded() {
    msg = 0;
    len = -1;
}

decoded::decoded(byte* b64, uint16_t b64len) {
    msg = (byte*)malloc(b64len);
    if (msg == 0) len = -1;
    else len = base64_decode(b64, b64len, msg);
}

decoded::decoded(decoded&& other) {
    this->~decoded();
    msg = other.msg;
    len = other.len;

    other.msg = 0;
    other.len = -1;
}

decoded::~decoded() {
    if (len >= 0) free(msg);
    len = -1;
}

decoded &decoded::operator=(decoded &&other) {
    this->~decoded();
    msg = other.msg;
    len = other.len;

    other.msg = 0;
    other.len = -1;
}

decoded::operator bool() {
    return len != -1;
}