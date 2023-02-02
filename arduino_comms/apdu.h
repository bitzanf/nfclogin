#ifndef _APDU_H_
#define _APDU_H_

#include <Arduino.h>

//private const string LOGIN_AID	= "F9 4C 75 6D 70 69 6B";
//private const string REGISTER_AID = "FA 4C 75 6D 70 69 6B";

uint8_t loginAPDU[] = {
    0x00, /* CLA */
    0xA4, /* INS */
    0x04, /* P1  */
    0x00, /* P2  */
    0x07, /* Length of AID  */
    0xF9, 0x4C, 0x75, 0x6D, 0x70, 0x69, 0x6B,   /* AID */
    0x00  /* Le  */
};

uint8_t registerAPDU[] = {
    0x00, /* CLA */
    0xA4, /* INS */
    0x04, /* P1  */
    0x00, /* P2  */
    0x07, /* Length of AID  */
    0xFA, 0x4C, 0x75, 0x6D, 0x70, 0x69, 0x6B,   /* AID */
    0x00  /* Le  */
};

#endif