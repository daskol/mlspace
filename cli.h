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

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string_view>
#include <vector>

namespace mlspace {

template <typename T, typename It>
concept Parser = std::forward_iterator<It> && requires(T obj, It it) {
    { obj.operator()(it, it) } -> std::same_as<int>;
};

struct Uint64Parser {
    std::string_view option;
    uint64_t &value;

    bool parsed = false;

    template <std::forward_iterator It> int operator()(It curr, It end) {
        if (!curr->starts_with(option)) {
            return 0;
        }
        return Parse(curr, end);
    }

    template <std::forward_iterator It> int Parse(It curr, It end) {
        std::string_view sv;
        auto advanced = 0;
        auto length = option.size();
        if (curr->size() == length) {
            if (++curr == end) {
                return 0;
            }
            sv = *curr;
            advanced = 1;
        } else if ((*curr)[length] == '=') {
            sv = curr->substr(length);
        } else {
            return 0;
        }

        auto [ptr, ec] = std::from_chars(sv.begin(), sv.end(), value);
        if (ptr != sv.end() || ec != std::errc()) {
            return 0;
        }

        parsed = true;
        return advanced + 1;
    };
};

static_assert(Parser<Uint64Parser, std::string_view::iterator>);

struct SHA256SumParser {
    std::string_view option;
    std::string_view &sha256sum;
    bool parsed = false;

    template <std::forward_iterator It> int operator()(It curr, It end) {
        if (!curr->starts_with(option)) {
            return 0;
        }
        return Parse(curr, end);
    }

    template <std::forward_iterator It> int Parse(It curr, It end) {
        if (auto length = option.size(); curr->size() == length) {
            if (++curr == end) {
                return false;
            }
            sha256sum = *curr;
            parsed = true;
            return 2;
        } else if ((*curr)[length] == '=') {
            sha256sum = curr->substr(length + 1);
            parsed = true;
            return 1;
        } else {
            return 0;
        }
    }
};

static_assert(Parser<SHA256SumParser, std::string_view::iterator>);

struct ChunkParser {
    std::string_view option;
    std::vector<std::string_view> &chunks;
    std::vector<std::pair<size_t, size_t>> indices;
    bool parsed = false;

    template <std::forward_iterator It> int operator()(It curr, It end) {
        if (!curr->starts_with(option)) {
            return 0;
        }
        return Parse(curr, end);
    }

    template <std::forward_iterator It> int Parse(It curr, It end) {
        auto advanced = 0;
        auto length = option.size();

        // We have two situations here: (a) '--spec-chunk-### <chunk>` and (b)
        // `--spec-chunk-###=<chunk>`.
        std::string_view sv;
        auto from = curr->begin() + length;
        auto to = curr->end();
        auto offset = curr->find_first_of('=');
        if (offset == curr->npos) {
            if (++curr == end) {
                return 0;
            }
            sv = *curr;
            advanced = 1;
        } else {
            sv = curr->substr(offset + 1);
            to = curr->begin() + offset;
        }

        size_t index;
        auto [rest, ec] = std::from_chars(from, to, index);
        if (rest != to || ec != std::errc()) {
            return 0;
        }
        chunks.push_back(sv);
        indices.emplace_back(index, indices.size());
        parsed = true;
        return advanced + 1;
    }

    bool Finalize(void) {
        assert(indices.size() == chunks.size() &&
               "number of chunks and their indices differ");

        // Calculate inverse permutation.
        std::sort(indices.begin(), indices.end());

        // Verify that indices list is contingues, i.e. [0, num_chunks).
        auto it = indices.begin();
        for (auto ix = 0; ix != indices.size(); ++ix, ++it) {
            if (ix != it->first) {
                return false;
            }
        }

        // Apply permutation to restore chunk order.
        for (it = indices.begin(); it != indices.end(); ++it) {
            if (it->second > it->first) {
                std::swap(chunks[it->first], chunks[it->second]);
            }
        }
        return true;
    }
};

static_assert(Parser<ChunkParser, std::string_view::iterator>);

/**
 * Spec represents chunked base64-encoded JSON in command line arguments.
 */
struct Spec {
    static constexpr std::string_view opt_version = "--spec-version";
    static constexpr std::string_view opt_sha256sum = "--spec-sha256sum";
    static constexpr std::string_view opt_num_chunks = "--spec-num-chunks";
    static constexpr std::string_view opt_chunk_ = "--spec-chunk-";

    size_t version = 0;
    size_t num_chunks = 0;
    std::vector<std::string_view> chunks;
    std::string_view sha256sum;

    static std::optional<Spec>
    FromArgs(std::vector<std::string_view> const &args);
};

} // namespace mlspace
