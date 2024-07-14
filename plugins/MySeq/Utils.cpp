//
// Created by Arunas on 15/07/2024.
//

#include <sstream>
#include <fstream>
#include "Utils.hpp"

std::string slurp(std::ifstream &in) {
    std::ostringstream sstr;
    sstr << in.rdbuf();
    return sstr.str();
}

std::optional<std::string> read_file(const char *filename) {
    std::ifstream in(filename);
    if (in.good()) {
        return {slurp(in)};
    } else {
        return {};
    }
}

void write_file(const char *filename, const char *data, size_t size) {
    std::ofstream out(filename);
    out.write(data, size);
}