#ifndef _BASE64_HPP
#define _BASE64_HPP

#include <string>
#include <vector>
#include <array>

extern const char base64chars[];
struct base64LUT_t {
    base64LUT_t() {
        for (int i = 0; i < 256; i++) _base64LUT[i] = -1;
        for (int i = 0; i < 64; i++) _base64LUT[base64chars[i]] = i;
    }

    constexpr int operator[](int i) const { return _base64LUT[i]; }
    
private:
    std::array<int, 256> _base64LUT;
};
extern const base64LUT_t base64LUT;



template <std::input_iterator It>
std::string base64_encode(const It begin, const It end) {
    std::string out;

    int val = 0, valb = -6;
    for (It it = begin; it != end; it++) {
        val = (val << 8) + (*it);
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

template <std::input_iterator It>
std::vector<uint8_t> base64_decode(const It begin, const It end) {
    std::vector<uint8_t> out;

    int val = 0, valb = -8;
    for (It it = begin; it != end; it++) {
        if (base64LUT[*it] == -1) break;
        val = (val << 6) + base64LUT[*it];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    return out;
}

#endif