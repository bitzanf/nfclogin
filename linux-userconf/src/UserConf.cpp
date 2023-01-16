#include "UserConf.hpp"
#include "fmt/format.h"
#include <iostream>
#include <fstream>
#include <openssl/rsa.h>
#include <openssl/pem.h>

using namespace osslUtils;
using namespace std;

SQLite3Error::SQLite3Error(const char *msg, sqlite3 *db) : runtime_error(fmt::format("{} - {}", msg, sqlite3_errmsg(db))) {}

UserConf::UserConf() {
    try {
        privateKey = loadPEMKey(configPath + "private.pem", false);
        publicKey = loadPEMKey(configPath + "public.pem", true);
    } catch (system_error &e) {
        if (e.code() == errc::no_such_file_or_directory) {
            makeKeyPair();
        } else throw e;
    }

    fingerprint = makeFingerprint(publicKey);
    ifstream fPubKey(configPath + "public.pem");
    sPublicKey = string(istreambuf_iterator<char>(fPubKey), istreambuf_iterator<char>());

    int sqlEC = sqlite3_open((configPath + "clients.db").c_str(), &db);
    if (sqlEC) throw SQLite3Error("Error opening database", db);

    sqlEC = sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS clients (\n"
        "    fingerprint TEXT,\n"
        "    pubkey TEXT,\n"
        "    login TEXT,\n"
        "    registered INTEGER\n"
        ");",
        NULL, NULL, NULL
    ); if (sqlEC) throw SQLite3Error("SQL Error", db);

    sqlEC = sqlite3_prepare_v2(db,
        "SELECT EXISTS (SELECT 1 FROM clients WHERE fingerprint = ?);",
        -1, &sqlExists, NULL
    ); if (sqlEC) throw SQLite3Error("SQL Error", db);

    sqlEC = sqlite3_prepare_v2(db,
        "DELETE FROM clients WHERE fingerprint = ?;",
        -1, &sqlDelete, NULL
    ); if (sqlEC) throw SQLite3Error("SQL Error", db);

    sqlEC = sqlite3_prepare_v2(db,
        "INSERT INTO clients (fingerprint, pubkey, login, registered) VALUES (?, ?, ?, unixepoch());",
        -1, &sqlInsert, NULL
    ); if (sqlEC) throw SQLite3Error("SQL Error", db);

    sqlEC = sqlite3_prepare_v2(db,
        "SELECT fingerprint, login, DATETIME(registered, 'unixepoch', 'localtime') FROM clients;",
        -1, &sqlSelect, NULL
    ); if (sqlEC) throw SQLite3Error("SQL Error", db);
}

UserConf::~UserConf() {
    int sqlEC;
    if (sqlExists) {
        sqlEC = sqlite3_finalize(sqlExists);
        if (sqlEC != SQLITE_OK) cerr << fmt::format("Error finalizing 'exists' statement : {}\n", sqlite3_errmsg(db));
    }
    sqlExists = NULL;

    if (sqlInsert) {
        sqlEC = sqlite3_finalize(sqlInsert);
        if (sqlEC != SQLITE_OK) cerr << fmt::format("Error finalizing 'insert' statement : {}\n", sqlite3_errmsg(db));
    }
    sqlInsert = NULL;

    if (sqlDelete) {
        sqlEC = sqlite3_finalize(sqlDelete);
        if (sqlEC != SQLITE_OK) cerr << fmt::format("Error finalizing 'delete' statement : {}\n", sqlite3_errmsg(db));
    }
    sqlDelete = NULL;

    if (sqlSelect) {
        sqlEC = sqlite3_finalize(sqlSelect);
        if (sqlEC != SQLITE_OK) cerr << fmt::format("Error finalizing 'delete' statement : {}\n", sqlite3_errmsg(db));
    }
    sqlSelect = NULL;

    if (db) {
        sqlEC = sqlite3_close(db);
        //if (sqlEC) throw runtime_error(fmt::format("Error closing database {} : {}", sqlEC, sqlite3_errmsg(db)));
        if (sqlEC) cerr << fmt::format("Error closing database : {}\n", sqlite3_errmsg(db));
        else db = NULL;
    }

    if (privateKey) EVP_PKEY_free(privateKey);
    if (publicKey) EVP_PKEY_free(publicKey);
    privateKey = NULL;
    publicKey = NULL;
}

bool UserConf::newDevice(const string &devFP, const string &devPK) {
    sqlite3_bind_text(sqlExists, 1, devFP.c_str(), devFP.length(), NULL);
    int rc = sqlite3_step(sqlExists);
    if (rc == SQLITE_ROW) {
        if (sqlite3_column_int(sqlExists, 0)) {
            sqlite3_clear_bindings(sqlExists);
            sqlite3_reset(sqlExists);
            return false;
        }
        
        sqlite3_bind_text(sqlInsert, 1, devFP.c_str(), devFP.length(), NULL);
        sqlite3_bind_text(sqlInsert, 2, devPK.c_str(), devPK.length(), NULL);
        sqlite3_bind_text(sqlInsert, 3, getlogin(), -1, NULL);
        if(sqlite3_step(sqlInsert) != SQLITE_DONE) throw SQLite3Error("Error inserting new device", db);
        sqlite3_clear_bindings(sqlInsert);
        sqlite3_reset(sqlInsert);
        return true;
    }

    sqlite3_clear_bindings(sqlExists);
    sqlite3_reset(sqlExists);
    return false;
}

void UserConf::deleteDevice(const std::string &devFP) {
    sqlite3_bind_text(sqlDelete, 1, devFP.c_str(), devFP.length(), NULL);
    int rc = sqlite3_step(sqlDelete);
    if (rc != SQLITE_DONE) throw SQLite3Error("Error deleting a device", db);
    sqlite3_clear_bindings(sqlDelete);
    sqlite3_reset(sqlDelete);
}

void UserConf::makeKeyPair() {
    ap::PKEYCTX ctx = ap::PKEYCTX(EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL));
    if (!ctx) throw OpenSSLError("Error creating key context");
    
    if (EVP_PKEY_keygen_init(ctx.get()) <= 0) throw OpenSSLError("EVP_PKEY_keygen_init() failed");
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), keyBits) <= 0) throw OpenSSLError("Error setting key bits");
    if (EVP_PKEY_CTX_set_rsa_keygen_primes(ctx.get(), 2) <= 0) throw OpenSSLError("Error setting key primes");
    if (EVP_PKEY_generate(ctx.get(), &privateKey) <= 0) throw OpenSSLError("Error generating private key");

    ap::FILE pubf = ap::FILE(fopen((configPath + "public.pem").c_str(), "wb"));
    if (!pubf) throw system_error(errno, generic_category(), "Error opening public key file for writing");
    if (PEM_write_PUBKEY(pubf.get(), privateKey) <= 0) throw OpenSSLError("Error writing public key");

    ap::FILE privf = ap::FILE(fopen((configPath + "private.pem").c_str(), "wb"));
    if (!privf) throw system_error(errno, generic_category(), "Error opening private key file for writing");
    if (PEM_write_PrivateKey(privf.get(), privateKey, NULL, NULL, 0, NULL, NULL) <= 0) throw OpenSSLError("Error writing private key");

    //nechutný, ale nenašel jsem tutoriál, jak to udělat lépe :(
    fflush(pubf.get());
    publicKey = loadPEMKey(configPath + "public.pem", true);
}

//https://github.com/openssl/openssl/blob/master/demos/pkey/EVP_PKEY_RSA_keygen.c

UserConf::DevIter UserConf::DevIter::operator++() {
    if (sql) {
        int rc = sqlite3_step(sql);
        if (rc == SQLITE_DONE) sql = nullptr;
    }

    return *this;
}

UserConf::DevInfo UserConf::DevIter::operator*() {
    if (sql) return DevInfo{
        .fingerprint = string((const char*)sqlite3_column_text(sql, 0)),
        .login       = string((const char*)sqlite3_column_text(sql, 1)),
        .registered  = string((const char*)sqlite3_column_text(sql, 2))
    };
    else return DevInfo{};
}

UserConf::DevIter UserConf::DevList::begin() {
    sqlite3_reset(uc.sqlSelect);
    int rc = sqlite3_step(uc.sqlSelect);
    if (rc == SQLITE_ROW) return DevIter{uc.sqlSelect};
    else return DevIter{};
}
