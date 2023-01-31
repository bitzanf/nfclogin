#include "CryptoLoginManager.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include "fmt/format.h"
#include <openssl/err.h>
#include "OSSLUtils.hpp"

using namespace std;
using namespace osslUtils;

CryptoLoginManager::CryptoLoginManager() {
    int sqlEC = sqlite3_open_v2((configPath + "clients.db").c_str(), &db, SQLITE_OPEN_READONLY, NULL);
    if (sqlEC) {
        throw SQLite3Error("Error opening database", db);
    }

    privateKey = loadPEMKey(configPath + "private.pem", false);
    publicKey = loadPEMKey(configPath + "public.pem", true);
    fingerprint = makeFingerprint(publicKey);

    if (sqlite3_prepare_v2(db,
        "SELECT fingerprint, pubkey, login, datetime(registered, 'unixepoch', 'localtime') as registered "
        "FROM clients WHERE fingerprint = ? LIMIT 1;",
    -1, &select, NULL) != SQLITE_OK) throw SQLite3Error("Error compileng statement", db);
}

CryptoLoginManager::~CryptoLoginManager() {
    if (select) sqlite3_finalize(select);
    select = NULL;

    if (db) {
        int sqlEC = sqlite3_close(db);
        //if (sqlEC) throw runtime_error(fmt::format("Error closing database {} : {}", sqlEC, sqlite3_errmsg(db)));
        if (sqlEC) cerr << fmt::format("Error closing database {} : {}", sqlEC, sqlite3_errmsg(db));
        else db = NULL;
    }

    if (privateKey) EVP_PKEY_free(privateKey);
    if (publicKey) EVP_PKEY_free(publicKey);
    privateKey = NULL;
    publicKey = NULL;
}

CryptoLoginManager::Device CryptoLoginManager::getDevice(const std::string& devFP) {
    sqlite3_reset(select);
    sqlite3_clear_bindings(select);

    if (sqlite3_bind_text(select, 1, devFP.c_str(), devFP.size(), SQLITE_STATIC) != SQLITE_OK) throw SQLite3Error("Bind failed", db);
    return Device(*this, select);
}

CryptoLoginManager::Device::Device(CryptoLoginManager &_clmg, sqlite3_stmt *dbRow) : clmg(_clmg) {
    regenSecret();
    int rc = sqlite3_step(dbRow);
    if (rc == SQLITE_ROW) {
        const unsigned char* column;

        column = sqlite3_column_text(dbRow, 0);
        deviceFingerprint = (const char*)column;

        column = sqlite3_column_text(dbRow, 1);
        size_t keyStrLen = strlen((const char*)column);

        ap::ODCTX dctx = ap::ODCTX(OSSL_DECODER_CTX_new_for_pkey(&devPubKey, "PEM", NULL, "RSA", EVP_PKEY_PUBLIC_KEY, NULL, NULL));
        if (!dctx) throw OpenSSLError("Error creating decoder context");
        if (!OSSL_DECODER_from_data(dctx.get(), &column, &keyStrLen)) throw OpenSSLError("Error decoding key");

        column = sqlite3_column_text(dbRow, 2);
        login = (const char*)column;

        column = sqlite3_column_text(dbRow, 3);
        timeRegistered = (const char*)column;
    } else if (rc == SQLITE_DONE) throw DeviceNotFound();
    else throw SQLite3Error("Error fetching database row", sqlite3_db_handle(dbRow));
}

CryptoLoginManager::Device::~Device() {
    if (devPubKey) EVP_PKEY_free(devPubKey);
    devPubKey = NULL;
}

vector<uint8_t> CryptoLoginManager::Device::getSecret() {
    vector<uint8_t> out;
    size_t outLen;
    ap::PKEYCTX ctx = ap::PKEYCTX(EVP_PKEY_CTX_new_from_pkey(NULL, devPubKey, NULL));

    if (!ctx) throw OpenSSLError("Error creating EVP context");
    if (EVP_PKEY_encrypt_init(ctx.get()) <= 0) throw OpenSSLError("Error initializing encryption");
    if (EVP_PKEY_encrypt(ctx.get(), NULL, &outLen, secret.data(), secret.size()) <= 0) throw OpenSSLError("Error (null) encrypting");
    out.resize(outLen);
    if (EVP_PKEY_encrypt(ctx.get(), out.data(), &outLen, secret.data(), secret.size()) <= 0) throw OpenSSLError("Error encrypting");
    
    return out;
}

bool CryptoLoginManager::Device::checkSecret(const std::vector<uint8_t> &_secret) {
    vector<uint8_t> decrypted;
    size_t decryptedLen;
    ap::PKEYCTX ctx = ap::PKEYCTX(EVP_PKEY_CTX_new_from_pkey(NULL, clmg.privateKey, NULL));

    if (!ctx) throw OpenSSLError("Error creating EVP context");
    if (EVP_PKEY_decrypt_init(ctx.get()) <= 0) throw OpenSSLError("Error initializing decryption");
    if (EVP_PKEY_decrypt(ctx.get(), NULL, &decryptedLen, _secret.data(), _secret.size()) <= 0) throw OpenSSLError("Error (null) decrypting");
    decrypted.resize(decryptedLen);
    if (EVP_PKEY_decrypt(ctx.get(), decrypted.data(), &decryptedLen, _secret.data(), _secret.size()) <= 0) throw OpenSSLError("Error decrypting");
    decrypted.resize(decryptedLen);
    
    return !memcmp(decrypted.data(), secret.data(), min(decrypted.size(), secret.size()));
}



CryptoLoginManager::SQLite3Error::SQLite3Error(const char *msg, sqlite3 *db) : runtime_error(fmt::format("{} - {}", msg, sqlite3_errmsg(db))) {}