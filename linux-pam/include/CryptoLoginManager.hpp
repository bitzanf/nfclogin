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
    /// @brief struktura obsahující informace o konkrétním zařízení
    class Device {
        std::string deviceFingerprint;
        std::string login;
        std::array<uint8_t, 64> secret;

        EVP_PKEY *devPubKey;
        CryptoLoginManager &clmg;

    public:
        class DeviceNotFound : public std::exception {};

        /// @brief načte z databáze informace o zařízení
        /// @param dbRow SQL dotaz s navázaným otiskem
        Device(CryptoLoginManager &_clmg, sqlite3_stmt *dbRow);
        
        ~Device();
        std::string timeRegistered;

        /// @brief zašifruje nějaká (již vytvořená) náhodná data veřejným klíčem protistrany
        /// @return zašifrovaná data
        std::vector<uint8_t> getSecret();

        /// @brief rozšifruje data svým soukromým klíčem a ověří
        /// @param _secret přijatá zašifrovaná data
        /// @return true pokud data odpovídají původním
        bool checkSecret(const std::vector<uint8_t> &_secret);
        
        /// @brief vytvoří náhodná data pro ověřování přihlášení
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

    /// @brief výběr zařízení podle otisku
    Device getDevice(const std::string& devFP);

    /// @brief otisk tohoto počítače
    const std::string& getFingerprint() { return fingerprint; }

private:
    /// @brief cesta ke složce s konfigurací, může být pouze pro čtení
    const std::string configPath = "/etc/pam-nfc/";
    
    /// @brief otisk veřejného klíče, slouží k unikátní identifikaci počítače
    ///        (SHA1 hash, string "%02x" hodnot parametru klíče N)
    std::string fingerprint;

    /// @brief databáze zařízení
    sqlite3 *db = NULL;

    /// @brief dotaz na výběr zařízení
    sqlite3_stmt *select = NULL;

    EVP_PKEY *privateKeyPair = NULL;
};

#endif