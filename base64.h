#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mlspace {

struct Base64 {
public:
    static constexpr std::string_view abc =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    static constexpr char padding = '=';

    static constexpr uint8_t padding_mask = 0xfe;

    static constexpr uint8_t unknown_mask = 0xff;

public:
    std::array<uint8_t, 256> bits; // TODO(@daskol): Make private?
                                   //
public:
    Base64(void);

    std::optional<std::string> Decode(std::string const &str);

    std::string Encode(std::string const &str);
};

} // namespace mlspace
