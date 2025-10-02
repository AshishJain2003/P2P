#ifndef SHA1_HPP
#define SHA1_HPP
#include <cstdint>
#include <string>
#include <vector>

std::string sha1(const std::string &string);
std::string sha1_from_data(const char* data, size_t len);

#endif