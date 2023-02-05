#ifndef _PAMCOMM_H_
#define _PAMCOMM_H_

#include <PN532.h>
#include "utils.h"

extern PN532 nfc;

extern bool ledState;
extern byte msgbfr[2048];
extern byte ttyID, nfcID;
extern uint16_t msgLen;

void sendACKorNAK(bool success);
bool checkCRC(decoded &dm);
void nfcAuth();
void nfcRegister();
void ledControl();
void radioControl();
void processMessage();

#endif