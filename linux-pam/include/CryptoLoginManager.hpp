#ifndef _CRYPTO_LOGIN_MANAGER_HPP
#define _CRYPTO_LOGIN_MANAGER_HPP

#include <string>
#include <array>
#include <memory>
#include <vector>
#include <sqlite3.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/decoder.h>

//OpenSSL >= 3.0.0
#define OPENSSL_API_COMPAT 30000
#define OPENSSL_NO_DEPRECATED

class CryptoLoginManager {
public:
    class Device {
        std::string deviceFingerprint;
        std::string login;
        std::array<uint8_t, 128> secret;

        EVP_PKEY *devPubKey;
        CryptoLoginManager &clmg;

    public:
        class DeviceNotFound : public std::exception {};

        Device(CryptoLoginManager &_clmg, sqlite3_stmt *dbRow);
        ~Device();
        std::string timeRegistered;

        std::vector<uint8_t> getSecret();
        bool checkSecret(const std::vector<uint8_t> &_secret);
        
        inline void regenSecret() {
            RAND_priv_bytes(secret.data(), secret.size());
        };
    };

    class SQLite3Error : public std::runtime_error {
    public:
        SQLite3Error(const char* msg, sqlite3 *db);
    };

    CryptoLoginManager();
    ~CryptoLoginManager();

    /// @brief gets device by its fingerprint
    Device getDevice(const std::string& devFP);

    /// @brief gets the fingerprint of this pc
    const std::string& getFingerprint() { return fingerprint; }

private:
    /// @brief path to configuration folder, can be read-only
    const std::string configPath = "/etc/pam-nfc/";
    
    /// @brief public key fingerprint, used for unique identification of the pc
    ///        (SHA1 hash, string of %02x values)
    std::string fingerprint;

    sqlite3 *db = NULL;
    sqlite3_stmt *select = NULL;
    EVP_PKEY *privateKey = NULL;
    EVP_PKEY *publicKey = NULL;
};

#endif