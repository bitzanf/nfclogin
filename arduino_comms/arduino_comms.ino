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
}

void loop() {
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