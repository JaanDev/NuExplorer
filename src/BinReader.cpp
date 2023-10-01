#include "BinReader.hpp"

#include <memory>
#include "logger.hpp"

BinReader::BinReader(const std::string& name, Endianness endianness) : m_endianness(endianness) {
    m_stream.open(name, std::ios::binary | std::ios::in);

    if (!m_stream) {
        logE("Failed to open a BinReader: {}", name);
        exit(1);
    }

    m_stream.seekg(0, std::ios::end);
    m_length = m_stream.tellg();
    m_stream.seekg(0);
}

BinReader::~BinReader() {
    m_stream.close();
}

void BinReader::read(uint8_t* buffer, size_t len) {
    m_stream.read((char*)buffer, len);
}

void BinReader::skip(size_t len) {
    m_stream.ignore(len);
}

void BinReader::seek(size_t pos) {
    m_stream.seekg(pos);
}

size_t BinReader::pos() {
    return m_stream.tellg();
}

size_t BinReader::find(const std::string_view str) {
    auto startPos = pos();
    size_t result = 0;
    constexpr size_t loadAtOnce = 1024 * 1024 * 8; // 8 mb
    size_t totalRead = 0;

    auto data = std::make_shared<uint8_t[]>(loadAtOnce);

    seek(0);

    while (true) {
        auto sizeRead = std::min(loadAtOnce, m_length - pos());
        if (sizeRead == 0)
            break;
        read(data.get(), sizeRead);
        auto sv = std::string_view((char*)data.get(), sizeRead);
        auto pos = sv.find(str);
        if (pos != std::string_view::npos) {
            return totalRead + pos;
        }
        totalRead += sizeRead;
    }

    seek(startPos);

    logE("Couldnt find in binary file", str);
    return 0;
}

size_t BinReader::length() {
    return m_length;
}
