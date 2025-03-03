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

#include "cli.h"

#include <variant>

namespace mlspace {

using SpecParser = std::variant<Uint64Parser, ChunkParser, SHA256SumParser>;

std::optional<Spec> Spec::FromArgs(std::vector<std::string_view> const &args) {
    Spec spec;
    std::vector<SpecParser> parsers = {
        Uint64Parser(Spec::opt_version, spec.version),
        Uint64Parser(Spec::opt_num_chunks, spec.num_chunks),
        ChunkParser(Spec::opt_chunk_, spec.chunks),
        SHA256SumParser(Spec::opt_sha256sum, spec.sha256sum),
    };

    auto it = args.begin();
    while (++it != args.end()) {
        for (auto &parser : parsers) {
            int advanced = std::visit(
                [it, &args](auto &p) -> int { return p(it, args.end()); },
                parser);
            if (advanced > 0) {
                it += advanced - 1;
                break;
            }
        }
    }

    // Verify that required options has been parsed.
    for (auto it = parsers.begin() + 1; it != parsers.begin() + 3; ++it) {
        auto parsed =
            std::visit([](auto const &p) -> bool { return p.parsed; }, *it);
        if (!parsed) {
            printf("some required options are not parsed\n");
            return std::nullopt;
        }
    }

    // Restore order of chunks, i.e. `--spec-chunk-0`, `--spec-chunk-1`, ...,
    // `--spec-chunk-(n - 1)`.
    if (auto *parser = std::get_if<ChunkParser>(&parsers[2])) {
        if (spec.num_chunks != parser->chunks.size()) {
            printf("actual and expected number of chunks does not match\n");
            return std::nullopt;
        }
        if (!parser->Finalize()) {
            return std::nullopt;
        }
    }

    return spec;
}

} // namespace mlspace
