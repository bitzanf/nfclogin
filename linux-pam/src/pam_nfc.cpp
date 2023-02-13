#include <stdlib.h>
#include <string.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <pwd.h>
#include <unistd.h>
#include <pthread.h>

#include <iostream>
#include <chrono>
#include <functional>

#include "Config.hpp"
#include "projectVersion.h"
#include "Comms.hpp"
#include "CryptoLoginManager.hpp"
#include "base64.hpp"

#define ever (;;)

using std::string;
using std::vector;
using std::runtime_error;
using std::back_inserter;
using namespace std::chrono;

/// @brief struktura na uložení výsledků autentifikační operace
struct ResultInfo {
    int result;
    bool finished = false;
    bool valid = false;
    string username;
};

/// @brief struktura na uložení parametrů pro vlákna
struct ThreadArgs {
    PamConfig *conf;
    ResultInfo *res;
    const pam_conv *conv;
    std::function<void(PamConfig*, ResultInfo*, const pam_conv*)> f;

    inline void operator()() {
        f(conf, res, conv);
    }
};

/// @brief obalovací funkce pro pthread, jinak po zavolání pthread_cancel přijde SIGABRT
/// @param arg ThreadArgs*
/// @return 
void *thread_safety_f(void *arg) {
    try {
        auto &args = *reinterpret_cast<ThreadArgs*>(arg);
        args();
        return NULL;
    } catch (...) {
        throw;
    }
}

/// @brief nalezne pozici oddělovače dat ('|')
/// @param data 
/// @return iterátor ukazující na oddělovač
auto getSplitIter(vector<uint8_t>& data) {
    return std::find_if(
        data.begin(),
        data.end(),
        [](uint8_t val) { return val == '|'; }
    );
}

/// @brief zajišťuje ověření přihlášení 
/// @param fp fingerprint zařízení
/// @return PAM status přihlášení
int doAuth(NFCAdapter& nfc, CryptoLoginManager& mngr, const string& fp) {
    try {
        vector<uint8_t> msg, response;
        bool r;

        //nejdříve vyhledáme zařízení v databázi a vyzvedneme náhodná zašifrovaná data
        auto dev = mngr.getDevice(fp);
        auto secret = dev.getSecret();

        //sestavíme zprávu
        msg.assign({'a', 'u', 't', 'h', '|'});
        msg.insert(msg.end(), secret.begin(), secret.end());

        //a odešleme ji
        if (!nfc.sendMessage(msg, NFCAdapter::PacketType::DATAPACKET)) return PAM_AUTH_ERR;
        auto pt = nfc.getResponse(response);
        
        //poté stačí zkontrolovat přijatá data
        if (pt == NFCAdapter::PacketType::DATAPACKET) {
            if (dev.checkSecret(vector<uint8_t>(getSplitIter(response)+1, response.end()))) return PAM_SUCCESS;
            else return PAM_PERM_DENIED;
        } else return PAM_AUTH_ERR;
    } catch (CryptoLoginManager::Device::DeviceNotFound) {
        return PAM_PERM_DENIED;
    }
}

//nevyužitá PAM funkce
PAM_EXTERN "C" int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	puts("Setcred");
    return PAM_SUCCESS;
}

//nevyužitá PAM funkce
PAM_EXTERN "C" int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	puts("Acct mgmt");
	return PAM_SUCCESS;
}

/// @brief zajišťuje NFC autentifikaci
void pam_nfc_service(PamConfig* conf, ResultInfo* res, const pam_conv* conv) {
    try {
        CryptoLoginManager mngr;
        NFCAdapter nfc(conf->ttyPath.c_str(), conf->ttySpeed, conf->timeout);
        vector<uint8_t> response;

        //sestavíme zprávu pro zjištění otisku zařízení
        string fp = "fp|" + mngr.getFingerprint();

        if (!nfc.ping()) {
            res->result = PAM_IGNORE;
            res->finished = true;
            return;
        }

        //počítadlo na timeout (20s)
        auto start = steady_clock::now();
        for ever {
            //každou sekundu se pošle zpráva a zkusí se, zda na ni přijde odpověď
            auto loopStart = steady_clock::now();
            if (!nfc.sendMessage(fp.begin(), fp.end(), NFCAdapter::PacketType::DATAPACKET)) {
                std::cerr << "failed sending message" << std::endl;
                res->result = PAM_AUTH_ERR;
                res->finished = true;
                return;
                //throw runtime_error{"error sending message"}; //nechapu proc, ale tohle se proste nechyti
            }

            auto pt = nfc.getResponse(response);

            //pokud dostaneme odpověď, pokusíme se o autentifikaci
            if (pt == NFCAdapter::PacketType::DATAPACKET) {
                res->result = doAuth(nfc, mngr, string(getSplitIter(response)+1, response.end()));
                res->finished = true;
                return;
            }

            auto now = steady_clock::now();
            if (duration_cast<seconds>(now - start).count() > 20) {
                res->result = PAM_IGNORE;
                res->finished = true;
                return;
            }

            //vysílá se 1x za sekundu
            auto remainder = 1e6 - duration_cast<microseconds>(now - loopStart).count();
            if (remainder > 0) usleep(remainder);
        }
    } catch (std::exception &e) {
        std::cerr << " PAM_NFC ERROR: " << e.what() << std::endl;
        res->result = PAM_AUTH_ERR;
        res->finished = true;
    }
}

/// @brief zajišťuje spuštění paralelní služby
/// @param conv PAM konverzační funkce (na dotazování na heslo apod.)
void pam_parallel_service(PamConfig* conf, ResultInfo* res, const pam_conv* conv) {
    pam_handle_t* pamh;
    char* username;

    //inicializace PAM
    int rc = pam_start(
        conf->parallelService.value().c_str(),
        res->username.length() > 0 ? res->username.c_str() : NULL,
        conv, &pamh
    );

    //a pokus o autentifikaci
    if (rc == PAM_SUCCESS) {
        res->result = pam_authenticate(pamh, 0);
        pam_get_item(pamh, PAM_USER, (const void**)&username);
        res->username = string(username);
        res->finished = true;
    }
}

/// @brief PAM funkce pro autentifikaci
PAM_EXTERN "C" int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    try {
        PamConfig conf{ argc, argv };
        pthread_t tasks[2];
        ResultInfo results[2];
        ThreadArgs args[2];
        int result;
        int nValid = 0;

        //naplníme informace o uživatelském jméně
        char* username;
        if (pam_get_item(pamh, PAM_USER, (const void**)&username) == PAM_SUCCESS) {
            for (auto &r : results) r.username = string(username);
        }

        //poté naplníme konverzační funkce a inicializujeme parametry
        const pam_conv* conv;
        pam_get_item(pamh, PAM_CONV, (const void**)&conv);
        for (auto &arg : args) {
            arg.conf = &conf;
            arg.conv = conv;
        }

        //NFC služba, spustíme vlákno
        results[0].valid = true;
        args[0].res = &results[0];
        args[0].f = &pam_nfc_service;
        pthread_create(&tasks[0], NULL, thread_safety_f, &args[0]);
        
        //paralelní služba, spustíme jen, pokud máme
        if (conf.parallelService.has_value()) {
            results[1].valid = true;
            args[1].res = &results[1];
            args[1].f = &pam_parallel_service;
            pthread_create(&tasks[1], NULL, thread_safety_f, &args[1]);
        } else {
            tasks[1] = 0;
        }

        //spočítáme validní (=běžící) služby
        for (auto& res : results) if (res.valid) nValid++;

        for ever {
            if (nValid == 0) {
                result = PAM_IGNORE;
                goto finish;    //asi jediné legitimní užití goto v C :)
            }

            for (auto& res : results) {
                //pokud vlákno už doběhlo, podíváme se, co nám vrátilo
                if (res.valid && res.finished) {
                    if (res.result == PAM_IGNORE) {
                        nValid--;
                        res.valid = false;
                    } else {
                        //máme hotovo; konec
                        result = res.result;
                        goto finish;
                    }
                }
            }

            usleep(5e5);
        }

        //pozabíjení všech vláken
        finish:
        for (auto& t : tasks) {
            if (t && !pthread_tryjoin_np(t, NULL)) {
                pthread_cancel(t);
            }
        }
        return result;
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return PAM_AUTH_ERR;
    }
}