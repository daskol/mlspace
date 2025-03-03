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

#include "job.h"

#include <nlohmann/json.hpp>

namespace mlspace {

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

std::optional<Job> Job::FromJSON(std::string const &str) {
    auto json = nlohmann::json::parse(str, nullptr, false);
    if (json.is_discarded()) {
        printf("failed to parse json\n");
        return std::nullopt;
    }

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

} // namespace mlspace
