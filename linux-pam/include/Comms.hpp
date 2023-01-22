#ifndef _COMMS_HPP
#define _COMMS_HPP

#include <stdint.h>
#include <termios.h>
#include <unistd.h>
#include <span>
#include <vector>
#include <thread>
#include <stdexcept>

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

template <typename It>
concept is_byte_iterator = requires(It it) {
    { *it } -> std::convertible_to<uint8_t>;
};

template <typename Predicate, typename... Args>
requires std::invocable<Predicate, Args...>
bool await(unsigned int timeout_ms, Predicate pred, Args&&... args) {
    unsigned int delay = 0;
    while (!pred(args...) && delay < timeout_ms) {
        usleep(1e5);
        delay += 100;
    }
    
    return delay < timeout_ms;
}

template <typename Predicate, typename... Args>
requires std::invocable<Predicate, Args...>
inline bool await(Predicate pred, Args... args) { return await(-1, pred, args...); };

//CRC16/XMODEM
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

void setupTTY(int fd, speed_t baudrate, cc_t vtime);


class NFCAdapter {
public:
    enum class PacketType : uint8_t {
        PKT_NONE = 0,
        DATAPACKET,
        REGISTER,
        LEDCONTROL,
        RADIOCONTROL
    };

private:
    uint8_t inPktID = 0;
    uint8_t outPktID = 0;
    PacketType inPktType, outPktType;

    int sendRetry = 0, recvRetry = 0;

    /// @brief is the API ready for operation?
    bool recvAvailable = true, sendAvailable = true;
    
    std::vector<uint8_t> rawData;

    int fd;
    bool ownsFD;

public:
    NFCAdapter(const char* ttyPath, speed_t baudrate, cc_t vtime);
    NFCAdapter(int fd);
    ~NFCAdapter();

    template <std::input_iterator It>
    requires is_byte_iterator<It>
    bool sendMessage(const It begin, const It end, PacketType type) {
        if (type == PacketType::PKT_NONE) return false;
        
        rawData.clear();
        std::copy(begin, end, std::back_inserter(rawData));
        return transmit();
    }

    template <typename T, template<typename> class Container>
    requires (std::is_convertible_v<T, uint8_t>)
    inline bool sendMessage(const Container<T>& cont, PacketType type) { return sendMessage(cont.begin(), cont.end(), type); }

    template <std::output_iterator<uint8_t> It>
    PacketType getResponse(It outIterator) {
        if (receive()) {
            std::copy(rawData.begin(), rawData.end(), outIterator);
            return (PacketType)inPktType;
        } else return PacketType::PKT_NONE;
    }

    template <class Container>
    inline PacketType getResponse(Container& container) { return getResponse(std::back_inserter(container)); };

    bool ping();

    int maxSendRetryCount = 4;
    int maxReceiveRetryCount = 4;
    int maxReadsDumped = 8;

private:
    bool commThreadCheckMSG();
    void sendHeader(uint16_t len);
    void sendChecksum();
    void sendACKorNAK(bool success);
    bool waitForACK();
    bool processMSG();

    bool transmit();
    bool receive();

    template <std::output_iterator<uint8_t> It>
    bool getNBytes(It it, int n) {
        std::vector<uint8_t> bfr(n);
        int alreadyRead = 0, nowRead;
        while (alreadyRead < n) {
            nowRead = read(fd, &bfr[alreadyRead], n - alreadyRead);
            if (nowRead == 0) return false;
            if (nowRead < 0) throw std::system_error(errno, std::generic_category(), "Error reveiving data");

            alreadyRead += nowRead;
        }

        std::copy(bfr.begin(), bfr.end(), it);
        return true;
    }
};

#endif