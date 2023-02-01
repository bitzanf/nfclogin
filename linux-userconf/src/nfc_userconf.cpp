//nechutný, ale tak co už...
//I really couldn't be bothered
#include "../../linux-pam/include/Comms.hpp"
#include "../../linux-pam/include/OSSLUtils.hpp"
#include "UserConf.hpp"
#include <iostream>
#include <functional>
#include <cxxabi.h>
#include <cstring>
#include <security/pam_appl.h>
#include <security/pam_misc.h>

using namespace osslUtils;
using std::runtime_error;
using std::find_if;
using std::string;
using std::vector;
using std::cout;

struct AP_MALLOC_FREE {
    void operator()(void* ptr) { free(ptr); };
};
using apCSTR = std::unique_ptr<char, AP_MALLOC_FREE>;

inline apCSTR CXADemangle(const char* mangled) {
    int status;
    return apCSTR(abi::__cxa_demangle(mangled, 0, 0, &status));
}

bool requireLogin() {
    char *username = getlogin();
    pam_handle_t *pamh;
    pam_conv conv = { misc_conv, NULL };
    
    if (pam_start("pamnfc-userconf", username, &conv, &pamh) != PAM_SUCCESS) {
        std::cerr << "pamnfc-userconf PAM service not found, fallback to login...\n";
        if (pam_start("login", username, &conv, &pamh) != PAM_SUCCESS) {
            std::cerr << "PAM error\n";
            return false;
        }
    }

    return pam_authenticate(pamh, 0) == PAM_SUCCESS;
}

void Usage() {
    cout << "nfc_userconf [-<ttypath>:<baudrate>] <CMD> ...\n\n"
            "(some actions might require additional authentication)\n"
            "CMD:\n"
            "    list                   -- lists all registered devices\n"
            "    register               -- registers a new device\n"
            "    remove [fingerprint]   -- removes a device\n"
            "        if no fingerprint is specified, retrieves the fingerprint via NFC,\n"
            "        otherwise removes the specified fingerprint\n";
}

void List(UserConf& uc) {
    for (auto [fp, login, reg] : uc.list()) {
        cout << login << " [" << fp << "] (registered " << reg << ")\n";
    }
}

void Register(UserConf& uc, NFCAdapter &nfc) {
    vector<uint8_t> temp;
    NFCAdapter::PacketType respType;
    if (!requireLogin()) {
        cout << "You have to be authenticated to register a new device\n";
        return;
    }

    auto &fp = uc.getFingerprint();
    auto &pk = uc.getPubKey();
    temp.assign(fp.begin(), fp.end());
    temp.push_back('|');
    temp.insert(temp.end(), pk.begin(), pk.end());

    nfc.sendMessage(temp, NFCAdapter::PacketType::REGISTER);
    temp.clear();

    respType = nfc.getResponse(temp);
    if (respType != NFCAdapter::PacketType::REGISTER) throw runtime_error{"Incorrect packet type"};
    
    auto split = find_if(temp.begin(), temp.end(), [](uint8_t val){ return val == '|'; });
    bool res = uc.newDevice(string(temp.begin(), split), string(split+1, temp.end()));

    if (res) cout << "Registration successful\n";
    else cout << "Registration failed\n";
}

void Remove(UserConf& uc, NFCAdapter &nfc, char* fp) {
    string sFP;
    NFCAdapter::PacketType respType;
    if (!requireLogin()) {
        cout << "You have to be authenticated to remove device\n";
        return;
    }

    if (fp == nullptr) {
        const string localFP = "fp|" + uc.getFingerprint();

        nfc.sendMessage(localFP, NFCAdapter::PacketType::DATAPACKET);

        respType = nfc.getResponse(sFP);
        if (respType != NFCAdapter::PacketType::DATAPACKET) throw runtime_error{"Incorrect packet type"};

        sFP.erase(0, 3);
    } else sFP.assign(fp);

    uc.deleteDevice(sFP);
}

int main_u(int argc, char** argv) {
    string ttyPath = "/dev/ttyS0";
    speed_t ttyBaud = B9600;
    int optidx = 1;

    if (argc < 2) {
        Usage();
        return 0;
    }

    if (argv[1][0] == '-') {
        auto end = argv[1] + strlen(argv[1]);
        auto sep = find_if(
            argv[1], end,
            [](char c) { return c == ':'; }
        );
        if (sep == end) {
            cout << "Error: incorrect TTY path specification: " << argv[1] << "\n\n";
            Usage();
            return 0;
        }
        ttyPath.assign(argv[1]+1, sep);     //pomlcka na zacatku...
        ttyBaud = get_baud(atoi(sep+1));    //uz od prirody konci \0
        if (ttyBaud == -1) {
            cout << "Error: incorrect baud rate: " << sep + 1 << "\n\n";
            Usage();
            return 0;
        }
        optidx++;
    }

    UserConf uc;
    NFCAdapter nfc(ttyPath.c_str(), ttyBaud, 50 /* *0,1s => 5s */);

    if (!strcmp("list", argv[optidx])) {
        cout << "Listing registered devices...\n";
        List(uc);
    } else if (!strcmp("register", argv[optidx])) {
        cout << "Registering a new device...\n";
        Register(uc, nfc);
    } else if (!strcmp("remove", argv[optidx])) {
        cout << "Removing a device...\n";
        Remove(uc, nfc, argc > optidx ? argv[optidx+1] : nullptr);
    } else {
        cout << "Error: unknown argument: " << argv[optidx] << "\n\n";
        Usage();
    }

    return 0;
}

int main(int argc, char** argv) {
    try {
        return main_u(argc, argv);
    } catch (std::exception &e) {
        int status;
        auto mangled = typeid(e).name();
        auto demangled = CXADemangle(mangled);
        std::cerr << "\x1b[31;1mE \x1b[37;1m" << (!demangled ? mangled : demangled.get()) << "\x1b[0m: " << e.what() << '\n';
        return -1;
    }
}

//https://stackoverflow.com/questions/3649278/how-can-i-get-the-class-name-from-a-c-object