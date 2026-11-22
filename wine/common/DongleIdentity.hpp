#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace dongle_recovered {

// USB EEPROM words 0x80..0x8b, read by GetAccessLevelFromDongle.
// Word 6 is the access level: Therapist=0, Dealer=1, OEM=2, Factory=3.
enum class DongleProfile : std::uint16_t { Dealer = 1, OEM = 2, Factory = 3 };

constexpr std::optional<DongleProfile> parse_dongle_profile(std::string_view name) {
    if (name == "Dealer") return DongleProfile::Dealer;
    if (name == "OEM") return DongleProfile::OEM;
    if (name == "Factory") return DongleProfile::Factory;
    return std::nullopt;
}
constexpr unsigned kIdentityBase = 0x80;
using DongleIdentity = std::array<std::uint16_t, 12>;

constexpr std::uint16_t identity_crc(const DongleIdentity& words) {
    std::uint16_t crc = 0xffff;
    const auto byte = [&crc](std::uint8_t value) constexpr {
        crc ^= static_cast<std::uint16_t>(value) << 8;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    };
    const auto word = [&byte](std::uint16_t value) constexpr {
        byte(static_cast<std::uint8_t>(value));
        byte(static_cast<std::uint8_t>(value >> 8));
    };
    // DongleInterface wraps the little-endian words in these two markers,
    // treating the final EEPROM checksum word as zero during calculation.
    word(0x2d5a);
    for (unsigned i = 0; i < words.size(); ++i)
        word(i + 1 == words.size() ? 0 : words[i]);
    word(0x39c2);
    return crc;
}

constexpr DongleIdentity dongle_identity(DongleProfile profile) {
    DongleIdentity words{0x000c, 0x0101, 0x05b0, 0xb014, 0, 0,
                         static_cast<std::uint16_t>(profile), 0, 0, 0, 0, 0};
    words.back() = identity_crc(words);
    return words;
}

} // namespace dongle_recovered
