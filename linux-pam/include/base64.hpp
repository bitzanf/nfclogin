#ifndef _BASE64_HPP
#define _BASE64_HPP

#include <string>
#include <vector>

std::string base64_encode(const std::vector<uint8_t> &in);
std::vector<uint8_t> base64_decode(const std::string &in);

#endif