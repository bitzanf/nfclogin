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

template <typename It>
requires is_byte_iterator<It>
uint16_t CRC16(const It begin, const It end) {
    constexpr uint16_t CRC16POLYNOMIAL = 0x1021;
    uint16_t crc = 0;

    for (It i = begin; i < end; i++) {
        crc ^= *i << 8;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000) crc = (crc << 1) ^ CRC16POLYNOMIAL;
            else crc <<= 1;
        }
    }

    return crc;
}

template <typename Predicate, typename... Args>
requires std::invocable<Predicate, Args...>
bool await(unsigned int timeout_ms, Predicate pred, Args... args) {
    unsigned int delay = 0;
    while (!pred(args...) && delay < timeout_ms) {
        usleep(1e5);
        delay += 100;
    }
    
    return delay < timeout_ms;
}



NFCAdapter::NFCAdapter(const char* ttyPath, speed_t baudrate) : ttyAdapter(ttyPath, baudrate) {
    launchThread();
}

NFCAdapter::NFCAdapter(int fd) : ttyAdapter(fd) {
    launchThread();
}

NFCAdapter::~NFCAdapter() {
    commThread.request_stop();
    commThread.join();
}

void NFCAdapter::launchThread() {
    commThread = std::jthread(std::bind(&NFCAdapter::commThreadProc, this, std::placeholders::_1));
}

void NFCAdapter::commThreadProc(std::stop_token stop) {
    try {
        for ever {
            if (stop.stop_requested()) return;

            if (!sendAvailable) {
                sendHeader();
                std::string b64data = base64_encode(outBfr);
                ttyAdapter.send((uint8_t*)b64data.data(), b64data.size());
                sendChecksum();
                if (!waitForACK()) {
                    if (sendRetry++ < maxSendRetryCount) throw runtime_error{"Too many errors sending message"};
                    else continue;
                };
                outPktType = (uint8_t)PacketType::PKT_NONE;
                sendAvailable = true;
            }

            bool msgReady;
            //await(std::bind(&NFCAdapter::commThreadCheckMSG, this, std::placeholders::_1), &msgReady);
            commThreadCheckMSG(&msgReady);
            if (msgReady) processMSG();

            usleep(1e5);
        }
    } catch (std::exception &e) {
        std::cerr << " COMM THREAD ERROR: " << e.what() << std::endl;
        exit(-1);
    }
}

bool NFCAdapter::commThreadCheckMSG(bool *ready) {
    uint8_t byte;
    if (ttyAdapter.recv(&byte)) {
        //SOH
        if (byte == 0x01) {
            *ready = true;
            return true;
        }
        //PING
        if (byte == 0x05) {
            ttyAdapter.send(0x06);
            return false;
        }
    }
    return false;
}

void NFCAdapter::sendHeader() {
    ttyAdapter.send(0x01);  //SOH
    ttyAdapter.send(outPktID);
    ttyAdapter.send(outPktType);
    ttyAdapter.send((outBfr.size() >> 8) & 0xFF);
    ttyAdapter.send(outBfr.size() & 0xFF);
    ttyAdapter.send(0x02);  //STX
}

void NFCAdapter::sendChecksum() {
    uint16_t crc = CRC16(outBfr.begin(), outBfr.end());
    
    ttyAdapter.send(0x03);  //ETX
    ttyAdapter.send((crc >> 8) & 0xFF);
    ttyAdapter.send(crc & 0xFF);
    ttyAdapter.send(0x04);  //EOT
}

void NFCAdapter::sendACKorNAK(bool success) {
    ttyAdapter.send(success ? 0x06 : 0x15);
    ttyAdapter.send(inPktID);
}

bool NFCAdapter::ping() {
    return true;
}

bool NFCAdapter::waitForACK() {
    uint8_t byte, ID;
    int iters = 0;
    try {
        do {
            byte = getU8(1000);
            ID = getU8(1000);
            iters++;
            usleep(1e5);
        } while (byte != 0x06 && byte != 0x15 && iters < maxReadsDumped);
    } catch (runtime_error) {
        return false;
    }

    return byte == 0x06 && ID == outPktID;
}

bool NFCAdapter::processMSG() {
    std::string b64data;

    uint8_t ID = getU8();
    inPktType = getU8();
    uint16_t LEN = getU16();
    if (ID == inPktID + 1) {
        //STX
        if (getU8() == 0x02) {
            for (int i = 0; i < LEN; i++) {
                auto byte = getU8();
                if (byte == 0x03) throw runtime_error("Incorrect payload length");
                b64data.push_back(byte);
            }
            //ETX
            if (getU8() == 0x03) {
                uint16_t msgcrc = getU16();
                //EOT
                if (getU8() == 0x04) {
                    inBfr = base64_decode(b64data);
                    uint16_t localcrc = CRC16(inBfr.begin(), inBfr.end());
                    if (localcrc == msgcrc) {
                        inPktID++;
                        recvAvailable = true;
                        recvRetry = 0;
                        sendACKorNAK(true);
                        return true;
                    }
                }
            }
        }
    }

    if (recvRetry++ < maxReceiveRetryCount) sendACKorNAK(false);
    else throw runtime_error{"Bad message format"};
    return false;
}

uint8_t NFCAdapter::getU8(unsigned int timeout_ms) {
    static auto getter = std::bind(&TTYAdapter::recv, &ttyAdapter, std::placeholders::_1);

    uint8_t out;
    bool success = await(timeout_ms, getter, &out);
    if (!success) throw runtime_error{"Timed out waiting for response"};

    return out;
}




NFCAdapter::TTYAdapter::TTYAdapter(const char *path, speed_t baudrate) {
    fd = open(path, O_RDWR);
    if (fd == -1) throw SYSERR("Error opening TTY");
    setup(baudrate);
    //int flags = fcntl(fd, F_GETFL, 0);
    //fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    ownsFD = true;
}

NFCAdapter::TTYAdapter::TTYAdapter(int _fd) {
    fd = _fd;
    ownsFD = false;
}

NFCAdapter::TTYAdapter::~TTYAdapter() {
    if (fd != -1 && ownsFD) close(fd);
    fd = -1;
}

bool NFCAdapter::TTYAdapter::recv(uint8_t* val) {
    uint8_t byte;
    int n = read(fd, &byte, 1);
    if (n == -1 && errno != EAGAIN) throw RECVERR;
    if (n > 0) {
        *val = byte;
        return true;
    } else return false;
}

void NFCAdapter::TTYAdapter::send(uint8_t *data, size_t len) {
    if (write(fd, data, len) == -1) throw SENDERR;
}

void NFCAdapter::TTYAdapter::setup(speed_t baudrate) {
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

    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN] = 0;
    cfsetspeed(&tty, baudrate);

    if (tcsetattr(fd, TCSANOW, &tty)) throw SYSERR("Error setting TTY attributes");
}