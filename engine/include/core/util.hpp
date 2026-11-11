#pragma once

#include <cmath>
#include <stdexcept>
#include <string>
#include <cstddef>

#include <cstdint>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using uin = unsigned int;
using dbl = double;
using flt = float;
using dblc = const double;
using fltc = const float;
using byte = unsigned char;


#define cdbl (double)
#define cflt (float)
#define var auto

#define def_void typedef void*

#define ret(v) const { return v; }

#define throw_error(msg) throw std::runtime_error(msg)
#define has(e, f) ((e & f) == f)

#define class_def(n, ...) \
public: \
    explicit n(__VA_ARGS__); \
    ~n() = default; \
    n(const n&) = delete; \
    n& operator=(const n&) = delete; \

#define class_def_no_destructor(n, ...) \
public: \
    explicit n(__VA_ARGS__); \
    ~n(); \
    n(const n&) = delete; \
    n& operator=(const n&) = delete; \

#define virt_class_def(n, ...)                                                                                              \
public:                                                                                                                \
    n(__VA_ARGS__) {}                                                                                     \
    virtual ~n() {}                                                                                                    \
    n(const n&) = delete;                                                                                              \
    n& operator=(const n&) = delete; \

#define vcontains(s, v) (std::find(s.begin(), s.end(), v) != s.end())

namespace util {

    inline std::string guid()
    {
        static std::string buf;
        static u32 counter = 0;
        buf = "gui" + std::to_string(counter++);

        std::hash<std::string> hasher;

        buf += std::to_string(hasher(buf));

        return buf;
    }

}