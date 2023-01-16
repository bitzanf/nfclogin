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
bool await(unsigned int timeout_ms, Predicate pred, Args... args);

template <typename Predicate, typename... Args>
requires std::invocable<Predicate, Args...>
inline bool await(Predicate pred, Args... args) { return await(-1, pred, args...); };

template <typename It>
requires is_byte_iterator<It>
//CRC16/XMODEM
uint16_t CRC16(const It begin, const It end);


class NFCAdapter {
private:
    uint8_t inPktID = 0;
    uint8_t outPktID = 0;
    uint8_t inPktType, outPktType;

    int sendRetry = 0, recvRetry = 0;

    /// @brief is the API ready for operation?
    bool recvAvailable = true, sendAvailable = true;
    
    std::vector<uint8_t> inBfr, outBfr;
    std::jthread commThread;

public:
    enum class PacketType : uint8_t {
        PKT_NONE = 0,
        DATAPACKET,
        REGISTER,
        LEDCONTROL,
        RADIOCONTROL
    };

    NFCAdapter(const char* ttyPath, speed_t baudrate);
    NFCAdapter(int fd);
    ~NFCAdapter();

    template <std::input_iterator It>
    requires is_byte_iterator<It>
    bool sendMessage(const It begin, const It end, PacketType type) {
        if (type == PacketType::PKT_NONE) return false;
        if (sendAvailable) {
            std::copy(begin, end, std::back_inserter(outBfr));
            outPktType = (uint8_t)type;
            sendAvailable = false;
            return true;
        }
        return false;
    }

    template <typename T, template<typename> class Container>
    requires (std::is_convertible_v<T, uint8_t>)
    inline bool sendMessage(const Container<T>& cont, PacketType type) { return sendMessage(cont.begin(), cont.end(), type); }

    template <std::output_iterator<uint8_t> It>
    PacketType getResponse(It outIterator) {
        if (recvAvailable) {
            std::copy(inBfr.begin(), inBfr.end(), outIterator);
            recvAvailable = false;
            return (PacketType)inPktType;
        }
        return PacketType::PKT_NONE;
    }

    template <class Container>
    inline PacketType getResponse(Container& container) { return getResponse(std::back_inserter(container)); };

    bool ping();

    int maxSendRetryCount = 4;
    int maxReceiveRetryCount = 4;
    int maxReadsDumped = 8;

private:
    class TTYAdapter {
    public:
        TTYAdapter(const char* path, speed_t baudrate);
        TTYAdapter(int _fd);
        ~TTYAdapter();
        bool recv(uint8_t* val);
        void send(uint8_t* data, size_t len);
        inline void send(uint8_t data) { send(&data, 1); };
    private:
        void setup(speed_t baudrate);

        int fd = -1;
        bool ownsFD;
    };

    void launchThread();
    void commThreadProc(std::stop_token stop);
    bool commThreadCheckMSG(bool *ready);
    void sendHeader();
    void sendChecksum();
    void sendACKorNAK(bool success);
    bool waitForACK();
    bool processMSG();
    uint8_t getU8(unsigned int timeout_ms = -1);
    inline uint16_t getU16(unsigned int timeout_ms = -1) { return (uint16_t(getU8(timeout_ms)) << 8) | uint16_t(getU8(timeout_ms)); }

    TTYAdapter ttyAdapter;
};

#endif