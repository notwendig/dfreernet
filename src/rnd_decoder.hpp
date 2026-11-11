#pragma once
#include "pe_image.hpp"
#include <array>
namespace rnet {
// File encryption only. No transport or DLL interface is implemented here.
class RndDecoder {
    std::array<std::uint32_t,18> p_{};
    std::array<std::uint32_t,1024> s_{};
    void initialize();
    std::uint32_t f(std::uint32_t x)const;
    void enc(std::uint32_t& l,std::uint32_t& r)const;
    void dec(std::uint32_t& l,std::uint32_t& r)const;
public:
#ifdef __linux__
    RndDecoder(); // Standard constants from bundled project resources.
#endif
    // Read the standard Blowfish pi constants from the installed original image.
    explicit RndDecoder(const PeImage& original);
    Bytes decode(const Bytes& encrypted)const;
    static std::array<std::uint8_t,31> file_key();
};
}
