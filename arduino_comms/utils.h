#ifndef _UTILS_H
#define _UTILS_H

#include <Arduino.h>

/// @brief spočítá CRC16
/// @param bfr data
/// @param len délka dat
/// @return CRC16/XMODEM
uint16_t CRC16(byte *bfr, int len);

/// @param out pole, kam se uloží výstup
/// @return délka zakódovanyćh dat
int base64_encode(byte *data, int dataLen, byte *out);

/// @param out pole, kam se uloží výstup
/// @return délka dekódovaných dat
int base64_decode(byte *data, int dataLen, byte *out);

/// @brief inicializace base64 lookup tabulky
void base64LUT_init();

/// @brief APDU pro výběr služby v telefonu
extern byte APDU[];

/// @brief délka onoho APDU
extern byte lenAPDU;

/// @brief dynamicky alokovaná dekódovaná zpráva
struct decoded {
    /// @brief dekódovaná zpráva
    byte *msg;

    /// @brief délka zprávy
    uint16_t len;

    decoded();

    /// @brief dekóduje oblast v paměti
    /// @param b64 base64 string
    /// @param b64len délka base64 stringu
    decoded(byte* b64, uint16_t b64len);

    decoded(decoded&& other);
    ~decoded();
    decoded& operator=(decoded&& other);

    /// @brief je zpráva validní?
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

/// @brief rozdělí uint16_t na 2x uint8_t; MSB je dříve
struct msb_u16 {
    /// @brief most significant byte
    byte msb;

    /// @brief least significant byte
    byte lsb;

    /// @brief vrátí hodnotu jako uint16_t
    inline uint16_t u16() { return (((uint16_t)msb) << 8) | lsb; }

    /// @brief nastaví oba byty podle příslušné hodnoty
    inline void u16(uint16_t v) { msb = (v >> 8) & 0xFF; lsb = v & 0xFF; }
};

/// @brief hlavička zprávy, slouží k zjednodušení tvorby
struct msghdr {
    /// @brief start of header [interní]
    byte _soh;

    /// @brief ID zprávy
    byte id;

    /// @brief typ zprávy
    byte type;

    /// @brief délka dat
    msb_u16 payload_len;

    /// @brief start of text [interní]
    byte _stx;

    /// @brief inicializuje interní položky
    inline void init() { _soh = 0x01; _stx = 0x02; }
};

/// @brief patička zprávy
struct msgftr {
    /// @brief end of text [interní]
    byte _etx;

    /// @brief kontrolní součet
    msb_u16 crc16;

    /// @brief end of text [interní]
    byte _eot;

    /// @brief inicializuje interní položky
    inline void init() { _etx = 0x03; _eot = 0x04; }
};

#endif