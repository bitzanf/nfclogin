#include "OSSLUtils.hpp"
#include "fmt/format.h"
#include <openssl/err.h>
#include <openssl/core_names.h>
#include <vector>
#include <termios.h>

using std::unique_ptr;

namespace osslUtils {

OpenSSLError::OpenSSLError(const char *msg) : runtime_error(fmt::format("{} - {}", msg, getOpenSSLError())) {}

EVP_PKEY* loadPEMKey(const std::string& file, bool isPublic) {
    EVP_PKEY *key;
    int keyType;

    if (isPublic) keyType = EVP_PKEY_PUBLIC_KEY;
    else keyType = EVP_PKEY_KEYPAIR;

    ap::FILE f = ap::FILE(fopen(file.c_str(), "r"));
    if (!f) throw std::system_error(errno, std::generic_category(), "Error opening key file");

    ap::ODCTX dctx = ap::ODCTX(OSSL_DECODER_CTX_new_for_pkey(&key, "PEM", NULL, "RSA", keyType, NULL, NULL));
    if (!dctx) throw OpenSSLError("Error creating decoder context");
    if (!OSSL_DECODER_from_fp(dctx.get(), f.get())) throw OpenSSLError("Error decoding key");
    
    return key;
}

std::string getOpenSSLError() {
    BIO *bio = BIO_new(BIO_s_mem());
    ERR_print_errors(bio);
    char *buf;
    size_t len = BIO_get_mem_data(bio, &buf);
    std::string ret(buf, len);
    BIO_free(bio);
    return ret;
}

std::string makeFingerprint(EVP_PKEY *pubkey) {
    BIGNUM *_bn = NULL;
    if (EVP_PKEY_get_bn_param(pubkey, OSSL_PKEY_PARAM_RSA_N, &_bn) != 1) throw OpenSSLError("GetParam failed");
    ap::BGNM n(_bn);

    const EVP_MD *algorithm = EVP_sha1();
    unsigned int hashLength = EVP_MD_get_size(algorithm);
    
    std::vector<uint8_t> hash(hashLength);
    ap::MDCTX ctx = ap::MDCTX(EVP_MD_CTX_new());

    std::vector<uint8_t> data(BN_num_bytes(n.get()));
    if (BN_bn2bin(n.get(), data.data()) != data.size()) throw OpenSSLError("bn2bin size mismatch");

    if (EVP_DigestInit(ctx.get(), algorithm) != 1) throw OpenSSLError("DigestInit failed");
    if (EVP_DigestUpdate(ctx.get(), data.data(), data.size()) != 1) throw OpenSSLError("DigestUpdate failed");
    if (EVP_DigestFinal(ctx.get(), hash.data(), &hashLength) != 1) throw OpenSSLError("DigestFinal failed");

    return fmt::format("{:02x}", fmt::join(hash, ""));
}

};


int get_baud(int baud) {
    switch (baud) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    case 460800:
        return B460800;
    case 500000:
        return B500000;
    case 576000:
        return B576000;
    case 921600:
        return B921600;
    case 1000000:
        return B1000000;
    case 1152000:
        return B1152000;
    case 1500000:
        return B1500000;
    case 2000000:
        return B2000000;
    case 2500000:
        return B2500000;
    case 3000000:
        return B3000000;
    case 3500000:
        return B3500000;
    case 4000000:
        return B4000000;
    default: 
        return -1;
    }
}