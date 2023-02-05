#ifndef _USERCONF_HPP
#define _USERCONF_HPP

//OpenSSL >= 3.0.0
#define OPENSSL_API_COMPAT 30000
#define OPENSSL_NO_DEPRECATED

#include <sqlite3.h>
#include <openssl/evp.h>
#include <string>
#include <span>
#include "../../linux-pam/include/OSSLUtils.hpp"

class SQLite3Error : public std::runtime_error {
public:
    SQLite3Error(const char* msg, sqlite3 *db);
};

class UserConf {
public:
    UserConf();
    ~UserConf();

    /// @return local device fingerprint
    inline const std::string& getFingerprint() { return fingerprint; };

    /// @return PEM local public key string
    inline const auto& getPubKey() { return publicKeyDER; };

    /// @brief registers a new device under the current user
    /// @param devFP fingerprint of the device
    /// @param devPK PEM public key of the device
    /// @return true on success, else false (most likely device already exists)
    bool newDevice(const std::string& devFP, const std::string& devPK);

    /// @brief unregisters a device (harmless if not registered)
    /// @param devFP fingerprint of the device
    void deleteDevice(const std::string& devFP);

    struct DevInfo {
        std::string fingerprint, login, registered;
    };

    class DevList;

    struct DevIter {
        DevIter() : sql(nullptr) {};
        DevIter operator++();
        DevInfo operator*();
        inline bool operator!=(const DevIter& other) { return sql != other.sql; };
        friend class UserConf::DevList;
    private:
        DevIter(sqlite3_stmt *_sql) : sql(_sql) {};
        sqlite3_stmt *sql;
    };

    class DevList {
    public:
        DevIter begin();
        inline DevIter end() { return DevIter(); };
        friend class UserConf;

    private:
        UserConf& uc;
        DevList(UserConf& _uc) : uc(_uc) {};
    };

    inline DevList list() { return DevList{*this}; };

private:
    /// @brief path to configuration folder, can be read-only
    const std::string configPath = "/etc/pam-nfc/";

    /// @brief number of bits of the OpenSSL key
    const unsigned int keyBits = 1024;

    /// @brief public key fingerprint, used for unique identification of the pc
    ///        (SHA1 hash, string of {:02x} values)
    std::string fingerprint;

    /// @brief DER representation of the public key
    std::span<uint8_t> publicKeyDER;

    sqlite3 *db;
    sqlite3_stmt *sqlInsert, *sqlDelete, *sqlExists, *sqlSelect;
    EVP_PKEY *keypair;

    void makeKeyPair();
    void makePublicDER();
};

#endif