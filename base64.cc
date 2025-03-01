#include "base64.h"

#include <iterator>

namespace mlspace {

Base64::Base64(void) {
    bits.fill(unknown_mask);
    for (auto it = 0; it != abc.size(); ++it) {
        char ch = abc[it];
        bits[ch] = it;
    }
    bits[padding] = padding_mask;
}

template <std::forward_iterator Inp,
          std::output_iterator<std::iter_reference_t<Inp>> Out>
inline bool DecodeQuad(std::array<uint8_t, 256> const &map, Inp begin, Inp end,
                       Out curr) {
    int32_t len = 0;
    uint32_t buf = 0;
    for (auto it = begin; it != end; ++it) {
        if (auto bits = map[*it]; bits == Base64::unknown_mask) {
            return false;
        } else if (bits == Base64::padding_mask) {
            break;
        } else {
            buf = (buf << 6) | bits;
            len += 6;
        }
        // Value `(len | 0b11000) >> 3` is an index of output byte.
        if (len & 0b11000) {
            *curr++ = (buf >> (len & 0b111)) & 0xff;
        }
    }
    // Number of output bytes.
    return static_cast<bool>(len & 0b11000);
}

std::optional<std::string> Base64::Decode(std::string const &s) {
    // Single character can not encode a byte.
    if (s.size() % 4 == 1) {
        return std::nullopt;
    }

    size_t length = 3 * s.size() / 4;
    std::string str;
    str.reserve(length);

    // Handle main part of sequence consisting of full quadruples.
    auto out = std::back_inserter(str);
    for (auto it = s.begin(), end = it + 4 * (length / 3); it != end; it += 4) {
        if (!DecodeQuad(bits, it, it + 4, out)) {
            return std::nullopt;
        }
    }

    // Handle tail without padding.
    if (auto rem = length % 3; rem > 0) {
        if (!DecodeQuad(bits, s.begin() + length - rem, s.end(), out)) {
            return std::nullopt;
        }
    }

    return str;
}

template <std::output_iterator<char> Out>
inline void EncodeQuad(std::string_view abc, uint32_t buf, Out out) {
    *out++ = abc[(buf >> 18) & 0b11'1111];
    *out++ = abc[(buf >> 12) & 0b11'1111];
    *out++ = abc[(buf >> 6) & 0b11'1111];
    *out++ = abc[(buf >> 0) & 0b11'1111];
}

std::string Base64::Encode(std::string const &s) {
    size_t length = 4 * ((s.size() + 2) / 3);
    std::string str;
    str.reserve(length);
    auto out = std::back_inserter(str);

    uint32_t buf = 0;
    auto it = s.begin();
    while (it != s.begin() + 3 * (s.size() / 3)) {
        buf = static_cast<uint32_t>(*(it++)) << 16;
        buf |= static_cast<uint32_t>(*(it++)) << 8;
        buf |= static_cast<uint32_t>(*(it++)) << 0;
        EncodeQuad(abc, buf, out);
    }

    if (it != s.end()) {
        // At this point, we have one or two input charcter(s).
        buf = static_cast<uint32_t>(*(it++)) << 16;
        *out++ = abc[(buf >> 18) & 0b11'1111];

        // There are two options for two characters in the middle.
        if (it == s.end()) {
            *out++ = abc[(buf >> 12) & 0b11'1111];
            *out++ = '=';
        } else {
            buf |= *(it++) << 8;
            *out++ = abc[(buf >> 12) & 0b11'1111];
            *out++ = abc[(buf >> 6) & 0b11'1111];
        }

        // The last char is always padding.
        *out++ = '=';
    }

    return str;
}

} // namespace mlspace
