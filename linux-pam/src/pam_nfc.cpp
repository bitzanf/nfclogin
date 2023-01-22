#include <stdlib.h>
#include <string.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <pwd.h>
#include <unistd.h>

#include <iostream>
#include <thread>
#include <chrono>
#include <functional>

#include "Config.hpp"
#include "projectVersion.h"
#include "Comms.hpp"
#include "CryptoLoginManager.hpp"
#include "base64.hpp"

#define ever (;;)

using std::thread;
using std::string;
using std::vector;
using std::runtime_error;
using std::back_inserter;
using namespace std::chrono;

struct ResultInfo {
    int result;
    bool finished = false;
    bool valid = false;
    string username;
};

auto getSplitIter(vector<uint8_t>& data) {
    return std::find_if(
        data.begin(),
        data.end(),
        [](uint8_t val) { return val == '|'; }
    );
}

int doAuth(NFCAdapter& nfc, CryptoLoginManager& mngr, const string& fp) {
    vector<uint8_t> msg, response;
    auto dev = mngr.getDevice(fp);
    auto secret = dev.getSecret();
    bool r;

    msg.assign({'a', 'u', 't', 'h', '|'});
    msg.insert(msg.end(), secret.begin(), secret.end());


    if (!nfc.sendMessage(msg, NFCAdapter::PacketType::DATAPACKET)) return PAM_AUTH_ERR;
    auto pt = nfc.getResponse(response);
    
    if (pt == NFCAdapter::PacketType::DATAPACKET) {
        if (dev.checkSecret(vector<uint8_t>(getSplitIter(response)+1, response.end()))) return PAM_SUCCESS;
        else return PAM_PERM_DENIED;
    } else return PAM_AUTH_ERR;
}

PAM_EXTERN "C" int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	puts("Setcred");
    return PAM_SUCCESS;
}

PAM_EXTERN "C" int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	puts("Acct mgmt");
	return PAM_SUCCESS;
}

void pam_nfc_service(PamConfig* conf, ResultInfo* res) {
    try {
        CryptoLoginManager mngr;
        NFCAdapter nfc(conf->ttyPath.c_str(), conf->ttySpeed, conf->timeout);
        string fp = "fp|" + mngr.getFingerprint();
        vector<uint8_t> response;

        if (!nfc.ping()) {
            res->result = PAM_IGNORE;
            res->finished = true;
            return;
        }

        auto start = steady_clock::now();
        for ever {
            auto loopStart = steady_clock::now();
            if (!nfc.sendMessage(fp.begin(), fp.end(), NFCAdapter::PacketType::DATAPACKET)) {
                res->result = PAM_AUTH_ERR;
                res->finished = true;
                return;
            }

            auto pt = nfc.getResponse(response);

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
        return;
    }
}

void pam_parallel_service(PamConfig* conf, ResultInfo* res, const pam_conv* conv) {
    pam_handle_t* pamh;
    char* username;
    int rc = pam_start(conf->parallelService.value().c_str(), res->username.length() > 0 ? res->username.c_str() : NULL, conv, &pamh);

    if (rc == PAM_SUCCESS) {
        res->result = pam_authenticate(pamh, 0);
        pam_get_item(pamh, PAM_USER, (const void**)&username);
        res->username = string(username);
        res->finished = true;
    }
}

PAM_EXTERN "C" int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    try {
        PamConfig conf{ argc, argv };
        thread tasks[2];
        ResultInfo results[2];
        int result;
        int nValid = 0;

        char* username;
        if (pam_get_item(pamh, PAM_USER, (const void**)&username) == PAM_SUCCESS) {
            for (auto &r : results) r.username = string(username);
        }

        results[0].valid = true;
        tasks[0] = thread(&pam_nfc_service, &conf, &results[0]);
        
        if (conf.parallelService.has_value()) {
            const pam_conv* conv;
            pam_get_item(pamh, PAM_CONV, (const void**)&conv);
            results[1].valid = true;
            tasks[1] = thread(&pam_parallel_service, &conf, &results[1], conv);
        }

        for (auto& res : results) if (res.valid) nValid++;

        for ever {
            if (nValid == 0) {
                result = PAM_IGNORE;
                goto finish;
            }

            for (auto& res : results) {
                if (res.valid && res.finished) {
                    if (res.result == PAM_IGNORE) {
                        nValid--;
                        res.valid = false;
                    } else {
                        result = res.result;
                        goto finish;
                    }
                }
            }
        }

        finish:
        for (auto& t : tasks) pthread_cancel(t.native_handle());
        return result;
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return PAM_AUTH_ERR;
    }
}