//
// Created by Arunas on 04/07/2024.
//

#ifndef MY_PLUGINS_UTILS_HPP
#define MY_PLUGINS_UTILS_HPP

#include <iostream>
#include <optional>
// Use (void) to silence unused warnings.

#define STRING(s) #s

std::string slurp(std::ifstream &in);

std::optional<std::string> read_file(const char *filename);

void write_file(const char *filename, const char *data, size_t size);

#endif //MY_PLUGINS_UTILS_HPP
