#include "Config.hpp"
#include <string.h>
#include "OSSLUtils.hpp"

/// @brief odstraní ze stringu prefix
/// @param pre prefix, který se má odstranit
/// @param str string, ze kterého se odstraňuje prefix
/// @return pointer na začátek dat bez prefixu
const char* rmprefix(const char* pre, const char* str) {
    int prelen = strlen(pre);
    if (strncmp(pre, str, prelen) == 0) {
        return str + prelen;
    }
    return NULL;
}

void PamConfig::configure(const int argc, const char** argv) {
    const char* arg;
    bool ttySet = false, ttySpeedSet = false;
    for (int i = 0; i < argc; i++) {
        arg = rmprefix("tty=", argv[i]);
        if (arg) {
            ttySet = true;
            ttyPath = arg;
        }

        arg = rmprefix("baudrate=", argv[i]);
        if (arg) {
            ttySpeedSet = true;
            ttySpeed = get_baud(atoi(arg));
            if (ttySpeed == -1) ttySpeedSet = false;    //invalid baud rate value
        }

        arg = rmprefix("parallel=", argv[i]);
        if (arg) parallelService = arg;

        arg = rmprefix("timeout=", argv[i]);
        if (arg) timeout = atoi(arg);
    }

    if (!ttySet || !ttySpeedSet) throw InvalidConfigurationException("TTY path not set");
}