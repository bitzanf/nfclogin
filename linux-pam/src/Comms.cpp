#include "Comms.hpp"
#include <fcntl.h>
#include <string.h>
#include <cerrno>
#include <chrono>
#include <functional>
#include <iostream>
#include "fmt/format.h"
#include "base64.hpp"

#define ever (;;)

using std::span;
using std::vector;
using std::runtime_error;
using std::system_error;

#define SYSERR(msg) system_error(errno, std::generic_category(), msg)
#define SENDERR SYSERR("Error sending data")
#define RECVERR SYSERR("Error reveiving data")
#define TIMEOUT runtime_error("Timed out waiting for response")

void setupTTY(int fd, speed_t baudrate, cc_t vtime) {
    if (fd < 0) return;

    termios tty;
    if (tcgetattr(fd, &tty)) throw SYSERR("Error getting TTY attributes");

    tty.c_cflag &= ~PARENB; //no parity
    tty.c_cflag &= ~CSTOPB; //1 stop bit
    tty.c_cflag &= ~CSIZE;  tty.c_cflag |= CS8; //8-bit bytes
    tty.c_cflag &= ~CRTSCTS;    //no hw flow control
    tty.c_cflag |= CREAD | CLOCAL;  //read & no modem
    
    tty.c_lflag &= ~ICANON;  //non-canonical mode
    tty.c_lflag &= ~(ECHO | ECHOE | ECHONL);    //no echo
    tty.c_lflag &= ~ISIG;   //no (exit) character interpretting

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); //no sw flow control
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); //no special recv handling

    tty.c_oflag &= ~OPOST;  //no special handling of output chars
    tty.c_oflag &= ~ONLCR;  //no \n -> \r\n

    tty.c_cc[VTIME] = vtime;
    tty.c_cc[VMIN] = 0;
    cfsetspeed(&tty, baudrate);

    if (tcsetattr(fd, TCSANOW, &tty)) throw SYSERR("Error setting TTY attributes");
}



NFCAdapter::NFCAdapter(const char* ttyPath, speed_t baudrate, cc_t vtime) {
    fd = open(ttyPath, O_RDWR | O_DSYNC);
    if (fd == -1) throw SYSERR("Error opening TTY");
    setupTTY(fd, baudrate, vtime);
    ownsFD = true;
}

NFCAdapter::NFCAdapter(int _fd) {
    fd = _fd;
    ownsFD = false;
}

NFCAdapter::~NFCAdapter() {
    if (ownsFD && fd >= 0) {
        close(fd);
        fd = -1;
    }
}

void NFCAdapter::sendHeader(uint16_t len) {
    uint8_t bfr[] {
        /* SOH */ 0x01,
        outPktID,
        (uint8_t)outPktType,
        (uint8_t)((len >> 8) & 0xFF),
        (uint8_t)(len & 0xFF),
        /* STX */ 0x02
    };

    if (write(fd, bfr, 6) == -1) throw SENDERR;
}

void NFCAdapter::sendChecksum() {
    uint16_t crc = CRC16(rawData.begin(), rawData.end());
    uint8_t bfr[] {
        /* ETX */ 0x03,
        (uint8_t)((crc >> 8) & 0xFF),
        (uint8_t)(crc & 0xFF),
        /* EOT */ 0x04
    };
    
    if (write(fd, bfr, 4) == -1) throw SENDERR;
}

void NFCAdapter::sendACKorNAK(bool success) {
    uint8_t ack = success ? 0x06 : 0x15;
    if (write(fd, &ack, 1) == -1) throw SENDERR;
}

bool NFCAdapter::ping() {
    uint8_t response;
    if (write(fd, "\x05", 1) == -1) throw SENDERR;
    if (!getNBytes(&response, 1)) return false;
    return response == 0x06;
}

bool NFCAdapter::waitForACK() {
    uint8_t bfr[2];
    if (getNBytes(bfr, 2)) {
        return bfr[0] == 0x06 && bfr[1] == outPktID;
    } else return false;
}

bool NFCAdapter::processMSG() {
    std::vector<uint8_t> bfr;
    auto inserter = std::back_inserter(bfr);
    uint16_t len;

    //na??teme hlavi??ku
    if (getNBytes(inserter, 5)) {
        //ID paketu
        if (bfr[0] == inPktID + 1) {
            //typ paketu
            inPktType = (PacketType)bfr[1];
            //d??lka dat
            len = ((uint16_t)bfr[2] << 8) | bfr[3];

            //slu??ebn?? byte hlavi??ky
            if (bfr[4] == 0x02) {
                bfr.clear();

                //na??ten?? dat
                if (getNBytes(inserter, len)) {
                    //data jsou base64, mus??me je dek??dovat
                    rawData = base64_decode(bfr.begin(), bfr.end());
                    uint16_t localCRC = CRC16(rawData.begin(), rawData.end());

                    //na??ten?? pati??ky
                    bfr.clear();
                    if (getNBytes(inserter, 4)) {
                        //slu??ebn?? byte pati??ky
                        if (bfr[0] == 0x03) {
                            uint16_t remoteCRC = ((uint16_t)bfr[1] << 8) | bfr[2];

                            //sed?? kontroln?? sou??et?
                            if (remoteCRC == localCRC && bfr[3] == 0x04) {
                                sendACKorNAK(true);
                                return true;
                            }
                        }
                    } else throw TIMEOUT;
                } else throw TIMEOUT;
            }
        }
    } else throw TIMEOUT;

    sendACKorNAK(false);
    return false;
}

bool NFCAdapter::transmit() {
    //zak??dujeme data do base64
    std::string base64 = base64_encode(rawData.begin(), rawData.end());
    bool success;
    int iters = 0;
    
    //pokud odes??l??n?? sel??e, n??kolikr??t opakujeme (a?? do maxSendRetryCount)
    outPktID++;
    do {
        //ode??leme hlavi??ku, data, pati??ku
        sendHeader(base64.length());
        if (write(fd, base64.data(), base64.length()) == -1) throw SENDERR;
        sendChecksum();
        success = waitForACK();
    } while (!success && iters++ < maxSendRetryCount);
    return success;
}

bool NFCAdapter::receive() {
    uint8_t mark;
    int iters = 0;
    int messageReads = 0;

    //n??kolikr??t na????t?? ??pln?? za????tek zpr??vy
    //zahod?? a?? maxReadsDumped na??ten??ch byt?? (kv??li synchronizaci na zna??ku SOH)
    while (iters++ < maxReadsDumped && messageReads < maxReceiveRetryCount) {
        if (getNBytes(&mark, 1)) {
            if (mark == 0x01) {
                //chytili jsme zna??ku za????tku zpr??vy, na??teme ji
                bool res = processMSG();
                if (!res) {
                    iters = 0;
                    messageReads++;
                    continue;
                }
                return res;
            } else continue;
        } else throw TIMEOUT;
    }

    return false;
}