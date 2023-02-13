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
    //naćteme soukromý klíč (a pokud není, vytvoříme ho)
    try {
        keypair = loadPEMKey(configPath + "private.pem", false);
    } catch (system_error &e) {
        if (e.code() == errc::no_such_file_or_directory) {
            system(("mkdir -p " + configPath).c_str());   //hnus, ja vim...
            makeKeyPair();
        } else throw e;
    }

    //vytvoříme veřejný klíč
    makePublicDER();
    fingerprint = makeFingerprint(keypair);

    //a nachystáme databázi
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

    //existuje zařízení?
    sqlEC = sqlite3_prepare_v2(db,
        "SELECT EXISTS (SELECT 1 FROM clients WHERE fingerprint = ?);",
        -1, &sqlExists, NULL
    ); if (sqlEC) throw SQLite3Error("SQL Error", db);

    //smazání určitého zařízení
    sqlEC = sqlite3_prepare_v2(db,
        "DELETE FROM clients WHERE fingerprint = ?;",
        -1, &sqlDelete, NULL
    ); if (sqlEC) throw SQLite3Error("SQL Error", db);

    //nové zařízení
    sqlEC = sqlite3_prepare_v2(db,
        "INSERT INTO clients (fingerprint, pubkey, login, registered) VALUES (?, ?, ?, unixepoch());",
        -1, &sqlInsert, NULL
    ); if (sqlEC) throw SQLite3Error("SQL Error", db);

    //získání informací o zařízeních (pro výpis)
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

    if (keypair) EVP_PKEY_free(keypair);
    keypair = NULL;

    if (publicKeyDER.data()) OPENSSL_free(publicKeyDER.data());
    publicKeyDER = {(uint8_t*)nullptr, 0};
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

void UserConf::deleteDevice(const string &devFP) {
    sqlite3_bind_text(sqlDelete, 1, devFP.c_str(), devFP.length(), NULL);
    int rc = sqlite3_step(sqlDelete);
    if (rc != SQLITE_DONE) throw SQLite3Error("Error deleting a device", db);
    sqlite3_clear_bindings(sqlDelete);
    sqlite3_reset(sqlDelete);
}

void UserConf::makeKeyPair() {
    keypair = EVP_RSA_gen(keyBits);
    if (!keypair) throw OpenSSLError("Error creating keypair");
    ap::FILE privf = ap::FILE(fopen((configPath + "private.pem").c_str(), "wb"));
    if (!privf) throw system_error(errno, generic_category(), "Error opening private key file for writing");
    if (PEM_write_PrivateKey(privf.get(), keypair, NULL, NULL, 0, NULL, NULL) <= 0) throw OpenSSLError("Error writing private key");
}

void UserConf::makePublicDER() {
    uint8_t *pubkey = nullptr;
    int pkLen = i2d_PublicKey(keypair, &pubkey);

    if (pkLen <= 0) throw OpenSSLError("Error generating public key DER");

    publicKeyDER = {pubkey, pkLen};
}

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
