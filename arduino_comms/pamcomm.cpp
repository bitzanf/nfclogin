#include <PN532_HSU.h>
#include "pamcomm.h"

#define RESPBFR_LEN UINT8_MAX

byte msgbfr[2048];
byte ttyID, nfcID = 1;
uint16_t msgLen;

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
            int msglen = base64_encode(response, responseLength-2, msgbfr+6); // 6 = delka hlavicky; 2 = delka SELECT_OK_SW
            
            //nadhera...
            msghdr *hdr = (msghdr*)msgbfr;
            msgftr *ftr = (msgftr*)(msgbfr+6+msglen);
            hdr->init();
            ftr->init();

            hdr->payload_len.u16(msglen);
            hdr->id = nfcID++;
            ftr->crc16.u16(CRC16(response, responseLength-2));
            
            msgLen = msglen + 10;

            return true;
        }
    }

    return false;
}

void nfcAuth() {
    msghdr *hdr = (msghdr*)msgbfr;
    msgftr *ftr = (msgftr*)(msgbfr + 6 + hdr->payload_len.u16());
    decoded msg{msgbfr+6, hdr->payload_len.u16()};
    uint16_t crc = CRC16(msg.msg, msg.len);
    if (crc != ftr->crc16.u16()) {
        sendACKorNAK(false);
        return;
    }
    sendACKorNAK(true);

    //odesleme SELECT APDU, dostaneme zpet fingerprint telefonu + OK
    if (!nfcSendAPDU(APDU, lenAPDU)) return;
    Serial.write(msgbfr, msgLen+10);    //10 bytu overhead
    Serial.read(); Serial.read();   // ACK

    
    //odesleme telefonu fingerprint pocitace
    responseLength = RESPBFR_LEN;
    nfc.inDataExchange(msg.msg, msg.len, response, &responseLength);

    //nacteme packet s autentifikacnimi daty
    while (Serial.available() == 0);
    Serial.readBytes(msgbfr, 6);
    Serial.readBytes(msgbfr+6, hdr->payload_len.u16() + 4);

    msg = decoded{msgbfr+6, hdr->payload_len.u16()};
    crc = CRC16(msg.msg, msg.len);
    if (crc != ftr->crc16.u16()) {
        sendACKorNAK(false);
        return;
    }
    sendACKorNAK(true);

    //prevezmeme pretransformovana data
    responseLength = RESPBFR_LEN;
    nfc.inDataExchange(msg.msg, msg.len, response, &responseLength);
    if (responseLength > 2) {   //pokud se zarizeni v telefonu nenalezlo, data se ignoruji (vrati se UNKNOWN_CMD_SW)
        msgLen = base64_encode(response, responseLength-2, msgbfr+6);
        hdr->id = nfcID++;
        hdr->payload_len.u16(msgLen);

        ftr = (msgftr*)(msgbfr + 6 + msgLen);
        ftr->init();
        ftr->crc16.u16(CRC16(response, responseLength-2));
    }
    Serial.write(msgbfr, msgLen);
    Serial.read(); Serial.read();   // ACK
}

void nfcRegister() {
    digitalWrite(LED_BUILTIN, HIGH);
    msghdr *hdr = (msghdr*)msgbfr;
    msgftr *ftr = (msgftr*)(msgbfr + 6 + hdr->payload_len.u16());
    decoded msg{msgbfr+6, hdr->payload_len.u16()};
    uint16_t crc = CRC16(msg.msg, msg.len);
    if (crc != ftr->crc16.u16()) {
        //chybový stav, 1 bliknutí
        //nesedí kontrolní součet
        sendACKorNAK(false);
        delay(300);
        digitalWrite(LED_BUILTIN, LOW);
        delay(200);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN, LOW);
        return;
    }
    sendACKorNAK(true);

    //odesleme SELECT APDU, dostaneme zpet fingerprint telefonu s jeho klicem
    if (!nfcSendAPDU(APDU, lenAPDU)) {
        //chybový stav, 2 bliknutí
        //chyba v komunikaci (telefon neodpovídá)
        delay(300);
        digitalWrite(LED_BUILTIN, LOW);
        delay(200);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN, LOW);
        delay(200);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN, LOW);
        return;
    }
    Serial.write(msgbfr, msgLen+10);    //10 bytu overhead
    Serial.read(); Serial.read();   // ACK

    
    //odesleme telefonu fingerprint pocitace
    responseLength = RESPBFR_LEN;
    if (!nfc.inDataExchange(msg.msg, msg.len, response, &responseLength)) {
        //chybový stav, 3 bliknutí
        //chyba v komunikaci (telefon neodpovídá)
        delay(300);
        digitalWrite(LED_BUILTIN, LOW);
        delay(200);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN, LOW);
        delay(200);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN, LOW);
        delay(200);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
    }
    digitalWrite(LED_BUILTIN, LOW);
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
