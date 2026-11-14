#pragma once
#include "core/util.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <span>
#include <string>
#include <type_traits>

namespace filesys
{
    template <typename T>
    T swap_bytes(T v)
    {
        std::array<byte, sizeof(T)> b;
        std::memcpy(b.data(), &v, sizeof(T));
        std::reverse(b.begin(), b.end());
        std::memcpy(&v, b.data(), sizeof(T));
        return v;
    }

    class ByteReader
    {
    public:
        explicit ByteReader(std::span<const byte> data, bool bigEndian = false) : data(data), bigEndian(bigEndian) {}

        template <typename T>
        T read()
        {
            static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>, "read scalars, not structs");
            T v = peek<T>(pos);
            pos += sizeof(T);
            return v;
        }

        template <typename T>
        std::vector<T> read_list(size_t count)
        {
            if (remaining() < sizeof(T) * count)
                throw_error("read past end of data");
            std::vector<T> out(count);
            for (size_t i = 0; i < count; ++i)
                out[i] = read<T>();
            return out;
        }

        template <typename T>
        const T* read_array(size_t count)
        {
            static_assert(sizeof(T) == 1, "raw pointer reads skip byte swapping, use read_list");
            if (remaining() < sizeof(T) * count)
                throw_error("read past end of data");
            const T* ptr = reinterpret_cast<const T*>(data.data() + pos);
            pos += sizeof(T) * count;
            return ptr;
        }
        
        template <typename T>
        T peek(size_t at) const
        {
            if (at > data.size() || data.size() - at < sizeof(T))
                throw_error("read past end of data");
            T v;
            std::memcpy(&v, data.data() + at, sizeof(T));
            return bigEndian && sizeof(T) > 1 ? swap_bytes(v) : v;
        }

        // wii u bfres offsets are relative to the field itself, 0 = null
        size_t read_rel_offset()
        {
            const size_t at = pos;
            const int32_t off = read<int32_t>();
            return off == 0 ? 0 : size_t(int64_t(at) + off);
        }

        std::vector<size_t> read_offset_array(size_t count) {
            std::vector<std::size_t> out(count);
            for (size_t i = 0; i < count; ++i)
                out[i] = read_rel_offset();
            return out;
        }

        std::string string_at(size_t at) const
        {
            if (at >= data.size())
                throw_error("string offset out of range");
            const auto* begin = reinterpret_cast<const char*>(data.data() + at);
            return {begin, strnlen(begin, data.size() - at)};
        }

        std::string read_string_offset()
        {
            const size_t offset = read_rel_offset();
            return offset == 0 ? std::string{} : string_at(offset);
        }

        std::vector<std::string> read_string_list(size_t count)
        {
            std::vector<std::string> out(count);
            for (size_t i = 0; i < count; ++i)
                out[i] = read_string_offset();
            return out;
        } 

        math::vec2 read_vec2()
        {
            return { read<float>(), read<float>() };
        }
        math::vec3 read_vec3() { return {read<float>(), read<float>(), read<float>()}; }
        math::quat read_quat() { return {read<float>(), read<float>(), read<float>(), read<float>()}; }
        math::vec4 read_vec4() { return {read<float>(), read<float>(), read<float>(), read<float>()}; }
        math::mat4 read_mat4()
        {
            math::mat4 m = math::mat4::identity();
            for (int i = 0; i < 4; ++i)
            {
                m[i] = read_vec4();
            }
            return m;
        }

        math::mat4 read_mat3()
        {
            math::mat4 m = math::mat4::identity();
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 4; ++col)
                    m[col][row] = read<float>();
            return m;
        }



        bool magic(const char (&m)[5])
        {
            if (remaining() < 4 || std::memcmp(data.data() + pos, m, 4) != 0)
                return false;
            pos += 4;
            return true;
        }

        std::span<const byte> slice(size_t offset, size_t size) const
        {
            if (offset > data.size() || data.size() - offset < size)
                throw_error("slice out of range");
            return data.subspan(offset, size);
        }

        void seek(size_t at)
        {
            if (at > data.size())
                throw_error("seek out of range");
            pos = at;
        }

        void skip(size_t n) { seek(pos + n); }
        size_t position() const { return pos; }
        size_t remaining() const { return data.size() - pos; }
        size_t size() const { return data.size(); }

        bool bigEndian;

    private:
        std::span<const byte> data;
        size_t pos = 0;
    };
}