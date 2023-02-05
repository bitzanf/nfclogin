#include <PN532_HSU.h>
#include "pamcomm.h"
#include "apdu.h"

#define RESPBFR_LEN UINT8_MAX

byte msgbfr[2048], temp[1024];
byte ttyID, nfcID = 1;
uint16_t msgLen, tempLen;

uint8_t responseLength = RESPBFR_LEN;
uint8_t response[RESPBFR_LEN];

PN532_HSU pn532hsu(Serial1);
PN532 nfc(pn532hsu);

void sendACKorNAK(bool success) {
    Serial.write(success ? 0x06 : 0x15);
    Serial.write(ttyID);
}

bool checkCRC(decoded &dm) {
    const uint16_t localcrc = CRC16(dm.msg, dm.len);
    const uint16_t msgcrc = (uint16_t)msgbfr[msgLen-3] << 8 | msgbfr[msgLen-2];

    return localcrc == msgcrc;
}

bool nfcSendAPDU(byte *apdu, byte apduLen) {
    //phone detected
    if (nfc.inListPassiveTarget()) {
        responseLength = RESPBFR_LEN;
        //phone communicating
        if (nfc.inDataExchange(apdu, apduLen, response, &responseLength)) {
            int msglen = base64_encode(response, responseLength, msgbfr+6); // 6 = delka hlavicky
            
            //nadhera...
            msghdr *hdr = (msghdr*)msgbfr;
            msgftr *ftr = (msgftr*)(msgbfr+6+msglen);
            hdr->init();
            ftr->init();

            hdr->payload_len.u16(msglen);
            hdr->id = nfcID++;
            ftr->crc16.u16(CRC16(response, responseLength));

            return true;
        }
    }

    return false;
}

void nfcAuth() {
    msghdr *hdr = (msghdr*)msgbfr;
    msgftr *ftr = (msgftr*)(msgbfr + 6 + hdr->payload_len.u16());
    tempLen = base64_decode(msgbfr+6, hdr->payload_len.u16(), temp);
    uint16_t crc = CRC16(temp, tempLen);
    if (crc != ftr->crc16.u16()) {
        sendACKorNAK(false);
        return;
    }
    sendACKorNAK(true);

    //odesleme SELECT APDU, dostaneme zpet fingerprint telefonu
    if (!nfcSendAPDU(loginAPDU, sizeof(loginAPDU))) return;
    Serial.write(msgbfr, msgLen+10);    //10 bytu overhead
    Serial.read(); Serial.read();   // ACK

    
    //odesleme telefonu fingerprint pocitace
    responseLength = RESPBFR_LEN;
    nfc.inDataExchange(temp, tempLen, response, &responseLength);

    //nacteme packet s autentifikacnimi daty
    while (Serial.available() == 0);
    Serial.readBytes(msgbfr, 6);
    Serial.readBytes(msgbfr+6, hdr->payload_len.u16() + 4);

    tempLen = base64_decode(msgbfr+6, hdr->payload_len.u16(), temp);
    crc = CRC16(temp, tempLen);
    if (crc != ftr->crc16.u16()) {
        sendACKorNAK(false);
        return;
    }
    sendACKorNAK(true);

    //prevezmeme pretransformovana data
    responseLength = RESPBFR_LEN;
    nfc.inDataExchange(temp, tempLen, response, &responseLength);
    if (responseLength > 0) {   //pokud se zarizeni v telefonu nenalezlo, data se ignoruji
        msgLen = base64_encode(response, responseLength, msgbfr+6);
        hdr->id = nfcID++;
        hdr->payload_len.u16(msgLen);
        ftr->crc16.u16(CRC16(response, responseLength));
    }
    Serial.write(msgbfr, msgLen+10);
    Serial.read(); Serial.read();   // ACK
}

void nfcRegister() {
    msghdr *hdr = (msghdr*)msgbfr;
    msgftr *ftr = (msgftr*)(msgbfr + 6 + hdr->payload_len.u16());
    tempLen = base64_decode(msgbfr+6, hdr->payload_len.u16(), temp);
    uint16_t crc = CRC16(temp, tempLen);
    if (crc != ftr->crc16.u16()) {
        sendACKorNAK(false);
        return;
    }
    sendACKorNAK(true);

    //odesleme SELECT APDU, dostaneme zpet fingerprint telefonu s jeho klicem
    if (!nfcSendAPDU(registerAPDU, sizeof(registerAPDU))) return;
    Serial.write(msgbfr, msgLen+10);    //10 bytu overhead
    Serial.read(); Serial.read();   // ACK

    
    //odesleme telefonu fingerprint pocitace
    responseLength = RESPBFR_LEN;
    nfc.inDataExchange(temp, tempLen, response, &responseLength);
}

bool ledState = false;
void ledControl() {
    decoded dm{msgbfr+6, msgLen-10};
    if (!checkCRC(dm)) {
        sendACKorNAK(false);
        return;
    }
    sendACKorNAK(true);

    //prozatim
    ledState = !ledState;
    digitalWrite(13, ledState);
}

/* MSG[0] = AutoRFCA; MSG[1] = ON/OFF */
void radioControl() {
    decoded dm{msgbfr+6, msgLen-10};
    if (!checkCRC(dm)) {
        sendACKorNAK(false);
        return;
    }
    sendACKorNAK(true);

    if (dm.len == 2){
        nfc.setRFField(dm.msg[0], dm.msg[1]);
    }
}


void processMessage() {
    Serial.readBytes(msgbfr+1, 5);
    const uint16_t len = (uint16_t)msgbfr[3] << 8 | msgbfr[4];
    msgLen = 10 + len; // header + content
    ttyID = msgbfr[1];

    if (msgbfr[5] == 0x02) {
        Serial.readBytes(msgbfr+6, len + 4);
        switch (msgbfr[2]) {
            case 1:
                nfcAuth();
                break;
            case 2:
                nfcRegister();
                break;
            case 3:
                ledControl();
                break;
            case 4:
                radioControl();
                break;
        }
    }
    else sendACKorNAK(false);
}
