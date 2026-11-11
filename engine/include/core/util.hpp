#pragma once

#include <cmath>
#include <stdexcept>

#include <cstdint>
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using uin = unsigned int;
using dbl = double;
using flt = float;
using dblc = const double;
using fltc = const float;


#define cdbl (double)
#define cflt (float)
#define var auto

#define def_void typedef void*

#define ret(v) const { return v; }

#define throw_error(msg) throw std::runtime_error(msg)
#define has(e, f) ((e & f) == f)
