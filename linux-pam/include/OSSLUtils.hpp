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

    /// @brief computes the public key fingerprint
    /// @return SHA1 hash, string of %02x values
    std::string makeFingerprint(EVP_PKEY *pubkey);

    EVP_PKEY* loadPEMKey(const std::string& file, bool isPublic);
    std::string getOpenSSLError();
}

int get_baud(int baud);

#endif