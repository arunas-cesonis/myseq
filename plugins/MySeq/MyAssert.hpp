//
// Created by Arunas on 14/07/2024.
//

#ifndef MY_PLUGINS_MYASSERT_H
#define MY_PLUGINS_MYASSERT_H

#include <cpptrace/cpptrace.hpp>

#define assert(cond) { if (!(cond)) { fprintf(stderr, "Assertion failed at %s line %d\n", __FILE__, __LINE__); cpptrace::generate_trace().print();  abort(); } }

#endif //MY_PLUGINS_MYASSERT_H
