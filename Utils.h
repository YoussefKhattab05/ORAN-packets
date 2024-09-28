// These files include utility functions like intToHex and calculateCRC32.
#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <cstdint>
#include <vector>

std::string intToHex(uint32_t value, size_t width);
uint32_t calculateCRC32(const std::string& data);
std::string padTo4ByteAlignment(const std::string& packetData);

#endif
