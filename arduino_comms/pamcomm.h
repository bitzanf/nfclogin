#ifndef _PAMCOMM_H_
#define _PAMCOMM_H_

#include <PN532.h>
#include "utils.h"

/// @brief NFC adaptér
extern PN532 nfc;

/// @brief stav LED (kvůli ledControl())
extern bool ledState;

/// @brief buffer na příjem zpráv
extern byte msgbfr[2048];

/// @brief délka přijaté zprávy
extern uint16_t msgLen;

/// @brief ID zprávy
extern byte ttyID, nfcID;

/// @brief odpoví po sériové lince ACK nebo NAK a číslem ořichozího paketu
void sendACKorNAK(bool success);

/// @brief zkontroluje CRC u dekódované zprávy
/// @param dm zpráva
bool checkCRC(decoded &dm);

/// @brief obstarává autentifikační komunikaci
void nfcAuth();

/// @brief obstarává registrační komunikaci
void nfcRegister();

/// @brief obstarává ovládání LED na desce
void ledControl();

/// @brief obstarává vzdálené ovládání rádia
void radioControl();

/// @brief přijetí a rozřarení zprávy podle typu paketu
void processMessage();

#endif