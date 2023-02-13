#ifndef _OSSL_UTILS_HPP
#define _OSSL_UTILS_HPP

#include <string>
#include <stdexcept>
#include <openssl/evp.h>
#include <openssl/decoder.h>
#include <openssl/encoder.h>
#include <memory>

namespace osslUtils {
    class OpenSSLError : public std::runtime_error {
    public:
        OpenSSLError(const char* msg);
    };

    /// @brief autopointers
    namespace ap {
        struct ODCTX_FREE {
            void operator()(OSSL_DECODER_CTX *ctx) { OSSL_DECODER_CTX_free(ctx); }
        };
        struct OECTX_FREE {
            void operator()(OSSL_ENCODER_CTX *ctx) { OSSL_ENCODER_CTX_free(ctx); }
        };
        struct PKEYCTX_FREE {
            void operator()(EVP_PKEY_CTX *ctx) { EVP_PKEY_CTX_free(ctx); }
        };
        struct MDCTX_FREE {
            void operator()(EVP_MD_CTX *ctx) { EVP_MD_CTX_free(ctx); }
        };
        struct FILE_FREE {
            void operator()(::FILE* fp) { fclose(fp); }
        };
        struct BN_FREE {
            void operator()(BIGNUM *bn) { BN_free(bn); }
        };
        using ODCTX   = std::unique_ptr<OSSL_DECODER_CTX, ODCTX_FREE>;
        using OECTX   = std::unique_ptr<OSSL_ENCODER_CTX, OECTX_FREE>;
        using PKEYCTX = std::unique_ptr<EVP_PKEY_CTX    , PKEYCTX_FREE>;
        using FILE    = std::unique_ptr<FILE            , FILE_FREE>;
        using MDCTX   = std::unique_ptr<EVP_MD_CTX      , MDCTX_FREE>;
        using BGNM    = std::unique_ptr<BIGNUM          , BN_FREE>;
    }

    /// @brief spočítá otisk veřejného klíče
    /// @return SHA1 hash, string "%02x" hodnot parametru klíče N
    std::string makeFingerprint(EVP_PKEY *pubkey);

    /// @brief načte klíč formátu PEM ze souboru
    /// @param file cesta k souboru
    /// @param isPublic je načítaný klíč veřejný?
    /// @return načtený klíč
    EVP_PKEY* loadPEMKey(const std::string& file, bool isPublic);

    /// @return chybový string z OpenSSL
    std::string getOpenSSLError();
}

/// @brief nechutná hrůza, vrací baud rate (rychlost sériového portu) jako definovanou hodnotu z číselné hodnoty
/// @param baud číselná hodnota rychlosti (např. 9600)
/// @return definovaná hodnota B* (z termios.h) odpovídající číslu (např. B9600)
int get_baud(int baud);

#endif