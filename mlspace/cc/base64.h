// Copyright 2025 Daniel Bershatsky
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
