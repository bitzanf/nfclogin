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

/// @brief spočítá CRC16/XMODEM dat v oblasti [begin; end)
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

/// @brief nastaví parametry fd pro sériovou linku
/// @param fd otevřený file descriptor pro sériovou linku
/// @param baudrate rychlost (b/s)
/// @param vtime timeout
void setupTTY(int fd, speed_t baudrate, cc_t vtime);

/// @brief linková vrstva, obstarává komunikaci s Arduinem a telefonem
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
    /// @brief ID příchozího paketu
    uint8_t inPktID = 0;

    /// @brief ID odchozího paketu
    uint8_t outPktID = 0;

    /// @brief typ paketu
    PacketType inPktType, outPktType;

    /// @brief je API připravené komunikovat?
    bool recvAvailable = true, sendAvailable = true;
    
    std::vector<uint8_t> rawData;

    /// @brief otevřený file descriptor pro komunikaci
    int fd;

    /// @brief musíme ho po ukončení zavřít?
    bool ownsFD;

public:
    NFCAdapter(const char* ttyPath, speed_t baudrate, cc_t vtime);
    NFCAdapter(int fd);
    ~NFCAdapter();

    /// @brief @b veřejné @b API; odeslání zprávy
    /// @tparam It vstupní iterátor
    /// @param begin začátek dat
    /// @param end konec dat
    /// @param type typ paketu
    /// @return úspěšnost odeslání
    template <std::input_iterator It>
    requires is_byte_iterator<It>
    bool sendMessage(const It begin, const It end, PacketType type) {
        if (type == PacketType::PKT_NONE) return false;
        
        //překopíruje data do interního bufferu a odešle
        outPktType = type;
        rawData.clear();
        std::copy(begin, end, std::back_inserter(rawData));
        return transmit();
    }

    /// @brief @b veřejné @b API; odeslání zprávy
    /// @tparam T typ kontejneru se zprávou
    /// @param cont kontejner se zprávou
    /// @param type typ paketu
    /// @return úspěšnost odeslání
    template <typename T, template<typename> class Container>
    requires (std::is_convertible_v<T, uint8_t>)
    inline bool sendMessage(const Container<T>& cont, PacketType type) {
        //delegujeme odeslání na funkci s iterátory
        return sendMessage(cont.begin(), cont.end(), type);
    }

    /// @brief @b veřejné @b API; přijetí zprávy
    /// @tparam It výstupní iterátor
    /// @param outIterator iterátor, do kterého se zkopíruje přijatá zpráva
    /// @return typ přijatého paketu
    template <std::output_iterator<uint8_t> It>
    PacketType getResponse(It outIterator) {
        //přijmeme zprávu a vykopírujeme obsah
        if (receive()) {
            std::copy(rawData.begin(), rawData.end(), outIterator);
            return (PacketType)inPktType;
        } else return PacketType::PKT_NONE;
    }

    /// @brief @b veřejné @b API; přijetí zprávy
    /// @tparam Container typ výstupního kontejneru
    /// @param container kontejner, do kter=eho se pRěkopíruje zpráva
    /// @return typ přijatého paketu
    template <class Container>
    inline PacketType getResponse(Container& container) {
        //opět delegujeme přijetí na funkci s iterátorem
        return getResponse(std::back_inserter(container));
    }

    /// @brief vyzkouší přítomnost Arduina
    /// @return true, pokud přišel pong
    bool ping();

    int maxSendRetryCount = 4;
    int maxReceiveRetryCount = 4;
    int maxReadsDumped = 8;

private:
    /// @brief odeslání hlavičky paketu
    /// @param len délka dat zprávy
    void sendHeader(uint16_t len);

    /// @brief odeslání patičky a kontrolního součtu
    void sendChecksum();

    /// @brief odešle odpověď Arduinu
    /// @param success rozhoduje, zda se odešle ACK nebo NAK
    void sendACKorNAK(bool success);

    /// @brief vyčká na ACK
    /// @return true, pokud přišlo validní potvrzení
    bool waitForACK();

    /// @brief zpracování přijaté zprávy
    /// @return true, opkud se zpráva přijala úspěšně
    bool processMSG();

    /// @brief interní funkce na odeslání
    /// @return úspěch
    bool transmit();

    /// @brief interní funkce na příjem
    /// @return úspěch
    bool receive();

    /// @brief načítá data z sériové linky
    /// @tparam It typ výstupního iterátoru
    /// @param it výstupní iterátor
    /// @param n počet načítaných bytů
    /// @return podařilo se data načíst?
    template <std::output_iterator<uint8_t> It>
    bool getNBytes(It it, int n) {
        std::vector<uint8_t> bfr(n);
        int alreadyRead = 0, nowRead;

        //postupně načítá data až do n počtu bytů
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