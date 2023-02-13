#ifndef _PAM_CONFIG_HPP
#define _PAM_CONFIG_HPP

#include <optional>
#include <string>
#include <stdexcept>
#include <termios.h>

class InvalidConfigurationException : public std::logic_error {
public:
    template <typename String>
    InvalidConfigurationException(String msg) : std::logic_error(msg) {};
};

/*
 * konfigurace PAM modulu
 * validní možnosti:
 *  - tty=<cesta>
 *  - baudrate=<rychlost sériové linky>
 *  - parallel=<název PAM služby, která se spustí souběžně s NFC přihlašováním>
 *  - timeout=<násobky 0,1s; maximální doba čekání na odpověď po TTY>
*/
struct PamConfig {
    PamConfig() {};
    PamConfig(const int argc, const char** argv) { configure(argc, argv); };

    void configure(const int argc, const char** argv);

    std::optional<std::string> parallelService;
    std::string ttyPath;
    speed_t ttySpeed = B0;

    /// @brief násobky 0,1s !
    int timeout = 0;
};

#endif