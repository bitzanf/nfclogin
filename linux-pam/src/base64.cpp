#include "base64.hpp"
#include <array>

using namespace std;

const char base64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static struct base64LUT_t {
    base64LUT_t() {
        for (int i = 0; i < 64; i++) _base64LUT[base64chars[i]] = i;
        for (int i = 64; i < 256; i++) _base64LUT[i] = -1;
    }

    constexpr int& operator[](int i) { return _base64LUT[i]; }
    
    std::array<int, 256> _base64LUT;
} base64LUT;

string base64_encode(const vector<uint8_t> &in) {
    string out;

    int val = 0, valb = -6;
    for (uint8_t c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(base64chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(base64chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

vector<uint8_t> base64_decode(const string &in) {
    vector<uint8_t> out;

    int val = 0, valb = -8;
    for (uint8_t c : in) {
        if (base64LUT[c] == -1) break;
        val = (val << 6) + base64LUT[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    return out;
}