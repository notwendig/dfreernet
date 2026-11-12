#pragma once
#include "pe_image.hpp"
#include <limits>
namespace rnet {
// MSVC/MFC 32-bit archive primitives used by the observed RND serializers.
class ArchiveReader {
  const Bytes &bytes_;
  std::size_t position_ = 0;
  void need(std::size_t n) const {
    if (position_ > bytes_.size() || n > bytes_.size() - position_)
      throw std::runtime_error("Truncated archive at " +
                               std::to_string(position_));
  }
  std::uint32_t string_length(bool &unicode) {
    auto n = u8();
    if (n < 255)
      return n;
    auto w = u16();
    if (w == 0xfffe) {
      if (unicode)
        throw std::runtime_error("Repeated Unicode marker");
      unicode = true;
      return string_length(unicode);
    }
    return w < 0xffff ? w : u32();
  }

public:
  explicit ArchiveReader(const Bytes &b, std::size_t p = 0)
      : bytes_(b), position_(p) {
    need(0);
  }
  std::size_t position() const { return position_; }
  void skip(std::size_t n) {
    need(n);
    position_ += n;
  }
  std::uint8_t u8() {
    need(1);
    return bytes_[position_++];
  }
  bool boolean() {
    auto b = u8();
    if (b > 1)
      throw std::runtime_error("Invalid archive Boolean at " +
                               std::to_string(position_ - 1));
    return b != 0;
  }
  std::uint16_t u16() {
    auto a = u8();
    return std::uint16_t(a) | (std::uint16_t(u8()) << 8);
  }
  std::uint32_t u32() {
    auto a = u16();
    return std::uint32_t(a) | (std::uint32_t(u16()) << 16);
  }
  std::uint32_t count() {
    auto n = u16();
    auto result = n < 65535 ? n : u32();
    if (result > 10000000)
      throw std::runtime_error("Archive count exceeds limit");
    return result;
  }
  std::u16string string() {
    bool unicode = false;
    auto n = string_length(unicode);
    if (n > 8 * 1024 * 1024)
      throw std::runtime_error("Archive string exceeds limit");
    need(std::size_t(n) * (unicode ? 2 : 1));
    std::u16string s;
    s.reserve(n);
    static constexpr char16_t cp1252[32] = {
        0x20ac, 0x0081, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
        0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008d, 0x017d, 0x008f,
        0x0090, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
        0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0x009d, 0x017e, 0x0178};
    for (std::uint32_t i = 0; i < n; ++i) {
      auto c = unicode ? u16() : u8();
      s.push_back(!unicode && c >= 128 && c < 160 ? cp1252[c - 128]
                                                  : char16_t(c));
    }
    return s;
  }
};
} // namespace rnet
