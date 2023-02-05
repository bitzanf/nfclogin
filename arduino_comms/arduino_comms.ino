#include "pamcomm.h"
    
#define LED_PIN 13

void setup() {
    pinMode(LED_PIN, OUTPUT);
    base64LUT_init();

    Serial.begin(9600);
    nfc.begin();
    
    uint32_t versiondata = nfc.getFirmwareVersion();
    if (!versiondata) {
        while (true) {
            //error while initializing the NFC
            digitalWrite(LED_PIN, HIGH);
            delay(500);
            digitalWrite(LED_PIN, LOW);
            delay(500);
        }
    }

    // Set the max number of retry attempts to read from a card
    // This prevents us from waiting forever for a card, which is
    // the default behaviour of the PN532.
    nfc.setPassiveActivationRetries(0x10);
    
    // configure board to read RFID tags
    nfc.SAMConfig();

    while (!Serial);
    Serial.println("Initialized");
}

void loop2() {
    if (Serial.available() > 0) {
        int mark = Serial.read();
        switch (mark) {
            case 0x01:
                processMessage();
                break;
            case 0x05:
                Serial.write(0x06);
                break;
#ifdef DEBUG
            case 'p':
                Serial.write("(response)");
                break;
#endif
        }
    }

    delay(10);
}

byte apdu[] = {
    0x00, /* CLA */
    0xA4, /* INS */
    0x04, /* P1  */
    0x00, /* P2  */
    0x05, /* Length of AID  */
    0xF2, 0x22, 0x22, 0x22, 0x22,   /* AID */
    0x00  /* Le  */
};

void loop() {
    byte resp[256];
    byte resplen;
    bool led = false;

    if (Serial.available()) {
        int c = Serial.read();
        if (c == 'p') {
            Serial.write("pong\n");
        } else if (c == 's') {
            Serial.write("Waiting for card...\n");
            if (nfc.inListPassiveTarget()) {
                Serial.write("Success\n");
                resplen = 256;

                if(nfc.inDataExchange(apdu, sizeof(apdu), resp, &resplen)) {
                    Serial.write("Got response (");
                    Serial.print(resplen);
                    Serial.write(" bytes): ");
                    for (int i = 0; i < resplen; i++) {
                        Serial.print(resp[i], HEX);
                        Serial.write(' ');
                    }
                } else {
                    Serial.write("inDataExchange failed\n");
                }
            } else {
                Serial.write("Nothing found\n");
            }
        }
    }
    delay(50);
}