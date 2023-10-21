#pragma once
#include <fstream>
#include <string>
#include <string_view>

enum class Endianness {
    Little,
    Big
};

class BinReader {
  public:
    BinReader(const std::string& name, Endianness endianness);
    ~BinReader();

    template <typename T>
    BinReader& operator>>(const T& other) {
        auto ptr = (char*)&other;
        auto size = sizeof(other);
        m_stream.read(ptr, size);
        if (m_endianness == Endianness::Big && size > 1) {
            std::reverse(ptr, ptr + size);
        }
        return *this;
    }

    template <typename T>
    T read() {
        T val;
        *this >> val;
        return val;
    }
    void read(uint8_t* buffer, size_t len);
    void skip(size_t len);
    void seek(size_t pos);
    size_t pos();
    size_t find(const std::string_view str);
    size_t length();
    void setEndianness(Endianness e);

  private:
    std::ifstream m_stream;
    Endianness m_endianness;
    size_t m_length;
};