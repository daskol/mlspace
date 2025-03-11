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

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include <mlspace/cc/base64.h>
#include <mlspace/cc/cli.h>
#include <mlspace/cc/job.h>

// TODO(@daskol): Signal traps: sigchild, sigkill, sig...

using mlspace::Job;
using mlspace::Spec;

namespace {

int Exec(std::string const &exe, std::vector<char *> const &args,
         std::vector<char *> const &env) {
    if (pid_t pid = fork(); pid == 0) {
        if (int ret = execvpe(exe.data(), args.data(), env.data())) {
            printf("failed to launch: [%d] %s\n", ret, strerror(errno));
            return 1;
        } else {
            return 0;
        }
    } else if (pid == -1) {
        printf("failed to fork: %s\n", strerror(errno));
        return 1;
    } else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            printf("failed to wait job command: %s\n", strerror(errno));
            return 1;
        }

        printf("job command completed with exit code %d\n",
               WEXITSTATUS(status));
        return 0;
    }
}

// Spawn spawns a new process and executes in user-specified command.
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

    auto job = Job::FromJSON(*decoded_chunk);
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
