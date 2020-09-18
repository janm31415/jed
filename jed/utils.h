#pragma once

#include <string>
#include <stdint.h>

std::string get_file_in_executable_path(const std::string& filename);

uint16_t ascii_to_utf16(unsigned char ch);