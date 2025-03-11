![Linting and testing][on-push]

[on-push]: https://github.com/daskol/mlspace/actions/workflows/on-push.yml/badge.svg

# MLSpace

*Alternative workload management in MLSpace at Cloud.RU (aka SberCloud).*

## Rationale

- Only `flags` (i.e. short `-o` or long `--option` with `-`).
- Overcomplicated API (wtf is `pytorch` and `pytorch2`?).
- Significant difference with `mpirun` and/or `slurm` but semantics the same.
- Unclear "launch protocol" (i.e. `sshd` in userspace, directory layout, etc).
- Option `-R` in `ssh` does not work. Consequently, `rsync` does not work too!
- Nothing has been changing in three years.

## Usage

```bash
python -m mlspace -e VAR=VAL -- env
```

```bash
python -m mlspace.testing -H localhost -p 8080
```

## Building

```bash
cmake -S . -B build -G 'Ninja Multi-Config' -DCMAKE_CXX_COMPILER=clang++
cmake --build build --config Release
```

```bash
python -m build -nvw
```

```bash
cibuildwheel --only cp313-manylinux_x86_64
```

## Specification

### Protocol

1. Preparation.
    1. Serialize job launch parameters to JSON.
    2. Encode JSON to base64.
    3. Splite base64-encoded JSON on chunks of 64kB.
2. Allocation (via Gateway v2 public API)
    1. Specify `launch` binary as `script` parameter of `type=binary` job.
    2. Enumerate all base64-encoded chunks to with `--spec-part-#` options and
       combine them to `flags` associate array.
    3. Add to `flags` array `--spec-version` and `--spec-num-parts` options.
    4. Add `--spec-sha256` for checksum verification.
    5. Submit job on execution.
3. Launching (vai `launch` binary).
    1. Process command line arguments and restore original base64-encoded JSON.
    2. Decode JSON to job spec.
    3. Validate job spec.
    4. Run target binary with `execvpe`.
        1. Change working directory.
        2. Update environment.

### Container Image

1. Create non-privileged user `user`.
    1. It must have ids `1000:1000` with home at `/home/user`.
    2. The following directories must exist and be owned by `user:user`:
       `/home/jovyan`, `/home/user`, `/tmp/.jupyter`, and `/tmp/.jupyter_data`,
2. Set up `sshd` in userspace.
    1. No `PAM`.
    2. Allow `ssh-rsa` keys.
    3. PID file must be in `/run/sshd/sshd.pid`
    4. Everything must be owned by `user.
3. Set up `hpc-x>=2.18` (`hpc-x>=2.21` on `ubuntu:24.04`).
    1. Install directory must be `/opt/hpcx`.
    2. Configure environment variables `LD_LIBRARY_PATH`, `PATH`, and
       `OPAL_PREFIX`.

## References

+ [Gateway V1 public API: Specification.][1].
+ [Gateway V2 public API: Specification.][2].
+ [Gateway V2 public API: OpenAI Specification][3].

[1]: https://api.ai.cloud.ru/public/v1/redoc
[2]: https://api.ai.cloud.ru/public/v2/redoc
[3]: https://api.aicloud.sbercloud.ru/public/v2/openapi.json
