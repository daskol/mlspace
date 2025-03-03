#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <unistd.h>

#include "base64.h"

#include <nlohmann/json.hpp>

// TODO(@daskol): Signal traps: sigchild, sigkill, sig...

namespace {

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

using SpecParser = std::variant<Uint64Parser, ChunkParser, SHA256SumParser>;

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

struct Job {
    std::string executable;
    std::vector<std::string> args;
    std::unordered_map<std::string, std::string> env;
    std::optional<std::filesystem::path> work_dir;

    bool shell_use;
    std::string shell;
    std::string image;

    static std::optional<Job> FromJSON(nlohmann::json const &json);
};

bool JsonPathInto(nlohmann::json const &json, std::string const &key,
                  std::optional<std::filesystem::path> &path) {
    if (!json.contains(key)) {
        return false;
    } else if (auto tmp = json.at(key); !tmp.is_string()) {
        return true;
    } else {
        path = std::move(tmp.template get<std::string>());
        return true;
    }
}

bool JsonStringInto(nlohmann::json const &json, std::string const &key,
                    std::string &val) {
    if (!json.contains(key)) {
        return false;
    } else if (auto tmp = json.at(key); !tmp.is_string()) {
        return false;
    } else {
        val = tmp.template get<std::string>();
        return true;
    }
}

bool JsonVectorInto(nlohmann::json const &json, std::string const &key,
                    std::vector<std::string> &val) {
    if (!json.contains(key)) {
        return false;
    } else if (auto tmp = json.at(key); !tmp.is_array()) {
        return false;
    } else {
        val = tmp.template get<std::vector<std::string>>();
        return true;
    }
}

bool JsonDictInto(nlohmann::json const &json, std::string const &key,
                  std::unordered_map<std::string, std::string> &val) {
    using T = std::unordered_map<std::string, std::string>;
    if (!json.contains(key)) {
        return false;
    } else if (auto tmp = json.at(key); !tmp.is_object()) {
        return false;
    } else {
        val = tmp.template get<T>();
        return true;
    }
}

std::optional<Job> Job::FromJSON(nlohmann::json const &json) {
    Job job;
    if (!JsonStringInto(json, "executable", job.executable)) {
        return std::nullopt;
    }

    if (!JsonVectorInto(json, "args", job.args)) {
        return std::nullopt;
    }

    if (!JsonDictInto(json, "env", job.env)) {
        return std::nullopt;
    }

    if (!JsonPathInto(json, "work_dir", job.work_dir)) {
        return std::nullopt;
    }

    return job;
}

int Exec(std::string const &exe, std::vector<char *> const &args,
         std::vector<char *> const &env) {
    if (int ret = execvpe(exe.data(), args.data(), env.data())) {
        printf("failed to launch: [%d] %s: %s\n", ret, strerrorname_np(errno),
               strerrordesc_np(errno));
        return 1;
    } else {
        return 0;
    }
}

int Spawn(Job job) {
    // Prepare subprocess command line arguments.
    std::vector<char *> args;
    args.reserve(job.args.size() + 2);
    args.push_back(job.executable.data());
    for (auto &arg : job.args) {
        args.push_back(arg.data());
    }
    args.push_back(nullptr);

    // Prepare subprocess environment variables.
    std::vector<std::string> env_owner;
    std::vector<char *> env;
    env_owner.reserve(job.env.size() + 1);
    env.reserve(job.env.size() + 1);
    for (auto &[k, v] : job.env) {
        // TODO(@daskol): Is it nessesary to join with '='?
        env_owner.push_back(k + '=' + v);
        env.push_back(env_owner.back().data());
    }

    // Add environment variables of parent process.
    for (auto ptr = environ; *ptr != nullptr; ++ptr) {
        std::string_view str(*ptr), key = str;
        auto offset = str.find_first_of('=');
        if (offset != str.npos) {
            key = str.substr(0, offset);
        }
        if (!job.env.contains(std::string{key})) {
            env.push_back(*ptr);
        }
    }

    // Array of environ veriables is NULL-terminated.
    env.push_back(nullptr);

    std::error_code ec;
    std::filesystem::path curr_dir;
    if (job.work_dir) {
        curr_dir = std::filesystem::current_path();
        std::filesystem::current_path(*job.work_dir, ec);
        if (ec) {
            printf("failed to change work dir: %s\n", ec.message().data());
            return 1;
        }
    }

    int ret = Exec(job.executable, args, env);

    if (job.work_dir) {
        std::filesystem::current_path(curr_dir, ec);
        if (ec) {
            printf("failed to restore work dir: %s\n", ec.message().data());
        }
    }
    return ret;
}

int Run(std::vector<std::string_view> const &args) {
    Spec spec;
    if (auto res = Spec::FromArgs(args)) {
        spec = std::move(*res);
    } else {
        printf("failed to parse command line\n");
        return 1;
    }

    printf("--opt-version=%zu\n", spec.version);
    printf("--opt-num-chunks=%zu\n", spec.num_chunks);
    printf("--opt-sha256sum=%s\n", spec.sha256sum.data());
    for (auto ix = 0; ix != spec.chunks.size(); ++ix) {
        printf("--opt-chunk-%d=%s\n", ix, spec.chunks[ix].data());
    }

    mlspace::Base64 base64;
    auto decoded_chunk = base64.Decode(std::string{spec.chunks[0]});
    printf("decoded: %s\n", decoded_chunk->data());

    auto json = nlohmann::json::parse(*decoded_chunk, nullptr, false);
    if (json.is_discarded()) {
        printf("failed to parse json\n");
        return 1;
    }

    auto job = Job::FromJSON(json);
    if (!job) {
        printf("failed to parse json to job\n");
        return 1;
    }
    printf("executable: %s\n", job->executable.data());
    printf("args: [");
    for (auto const &arg : job->args) {
        printf(" %s", arg.data());
    }
    printf(" ]\n");

    printf("env: [");
    for (auto const &[k, v] : job->env) {
        printf(" %s=%s", k.data(), v.data());
    }
    printf(" ]\n");

    return Spawn(*job);
}

} // namespace

int main(int argc, char *argv[]) {
    return Run({argv, argv + argc});
}
