//nechutný, ale tak co už...
//I really couldn't be bothered
#include "../../linux-pam/include/Comms.hpp"
#include "../../linux-pam/include/OSSLUtils.hpp"
#include "UserConf.hpp"
#include <iostream>
#include <functional>
#include <cxxabi.h>
#include <cstring>
#include <span>
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#include <openssl/pem.h>

using namespace osslUtils;
using std::runtime_error;
using std::find_if;
using std::string;
using std::vector;
using std::span;
using std::cout;

//autopointer pro C string
struct AP_MALLOC_FREE {
    void operator()(void* ptr) { free(ptr); };
};
using apCSTR = std::unique_ptr<char, AP_MALLOC_FREE>;

/// @brief převede interní název C++ typu na čitelnou reprezentaci
/// @param mangled typeid::name
/// @return čitelný název
inline apCSTR CXADemangle(const char* mangled) {
    int status;
    return apCSTR(abi::__cxa_demangle(mangled, 0, 0, &status));
}

/// @brief vyžádá si od uživatele přihlášení pomocí PAM
/// @return true při úspěšném přihlášení
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

/// @brief převede veřejný klíč z binární DER reprezentace na textovou (PEM)
/// @param der binární veřejný klíč
/// @return PEM string
string DER2PEM(span<uint8_t> der) {
    EVP_PKEY *devPubKey = nullptr;
    uint8_t *pDER = der.data();
    if (d2i_PublicKey(EVP_PKEY_RSA, &devPubKey, (const uint8_t**)&pDER, der.size()) == nullptr) throw OpenSSLError("Error decoding DER device key");

    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PUBKEY(bio, devPubKey);

    uint8_t *data;
    size_t len = BIO_get_mem_data(bio, &data);
    string pem{(char*)data, len};

    EVP_PKEY_free(devPubKey);
    BIO_free(bio);
    return pem;
}

/// @brief nápověda
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

/// @brief výpis všech zařízení
void List(UserConf& uc) {
    for (auto [fp, login, reg] : uc.list()) {
        cout << login << " [" << fp << "] (registered " << reg << ")\n";
    }
    cout << "(done)\n";
}

/// @brief registrace nového zařízení
void Register(UserConf& uc, NFCAdapter &nfc) {
    vector<uint8_t> temp;
    NFCAdapter::PacketType respType;
    if (!requireLogin()) {
        cout << "You have to be authenticated to register a new device\n";
        return;
    }

    //sestavíme zprávu na získání otisku zařízení
    auto &fp = uc.getFingerprint();
    auto &pk = uc.getPubKey();
    temp.assign(fp.begin(), fp.end());
    temp.push_back('|');
    temp.insert(temp.end(), pk.begin(), pk.end());

    //a odešleme
    nfc.sendMessage(temp, NFCAdapter::PacketType::REGISTER);
    temp.clear();

    //převezmeme odpověď s informacemi o protistraně
    respType = nfc.getResponse(temp);
    if (respType != NFCAdapter::PacketType::REGISTER) throw runtime_error{"Incorrect packet type"};
    
    //rozdělíme na otisk a klíč a registrujeme nové zařízení
    auto split = find_if(temp.begin(), temp.end(), [](uint8_t val){ return val == '|'; });
    bool res = uc.newDevice(string(temp.begin(), split), DER2PEM({(split+1), temp.end()}));

    if (res) cout << "Registration successful\n";
    else cout << "Registration failed\n";
}

/// @brief vymazání zařízení
/// @param fp otisk mazaného zařízení
///           (je-li NULL, dotáže se pomocí NFC)
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

    cout << "Removing device [" << sFP << "]\n";
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

    //načtení cesty k sériovému portu
    if (argv[1][0] == '-') {
        //rozdělení na ':'
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
        ttyPath.assign(argv[1]+1, sep);     //pomlčka na začátku...
        ttyBaud = get_baud(atoi(sep+1));    //už od přírody konci \0
        if (ttyBaud == -1) {
            cout << "Error: incorrect baud rate: " << sep + 1 << "\n\n";
            Usage();
            return 0;
        }
        optidx++;
    }

    UserConf uc;

    //načtení typu operace a provedení
    if (!strcmp("list", argv[optidx])) {
        cout << "Listing registered devices...\n";
        List(uc);
    } else {
        NFCAdapter nfc(ttyPath.c_str(), ttyBaud, 50 /* *0,1s => 5s */);
        if (!nfc.ping()) throw runtime_error{"error communicating with NFC adapter"};

        if (!strcmp("register", argv[optidx])) {
            cout << "Registering a new device...\n";
            Register(uc, nfc);
        } else if (!strcmp("remove", argv[optidx])) {
            cout << "Removing a device...\n";
            Remove(uc, nfc, argc > (optidx + 1) ? argv[optidx+1] : nullptr);
        } else {
            cout << "Error: unknown argument: " << argv[optidx] << "\n\n";
            Usage();
        }
    }

    return 0;
}

/// @brief obalovač se zachytáváním výjimek
int main(int argc, char** argv) {
    try {
        return main_u(argc, argv);
    } catch (std::exception &e) {
        int status;
        auto mangled = typeid(e).name();
        auto demangled = CXADemangle(mangled);
        //hezké vytišténí zachycené chyby
        std::cerr << "\x1b[31;1mE \x1b[37;1m" << (!demangled ? mangled : demangled.get()) << "\x1b[0m: " << e.what() << '\n';
        return -1;
    }
}