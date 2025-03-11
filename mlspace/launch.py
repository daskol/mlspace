# Copyright 2025 Daniel Bershatsky
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import json
import logging
import sys
from base64 import b64encode
from contextlib import contextmanager
from copy import deepcopy
from dataclasses import dataclass, field, fields
from os import PathLike
from pathlib import Path
from subprocess import Popen
from typing import Any, ClassVar, Iterator, cast
from uuid import uuid4

if sys.version_info >= (3, 11):
    from typing import Self
else:
    from typing_extensions import Self

from mlspace import config

logger = logging.getLogger(__name__)


class Runner:

    def join(self, job: 'Job'):
        raise NotImplementedError

    def launch(self, job: 'Job', launch_bin: Path):
        raise NotImplementedError

    def detach(self, job: 'Job'):
        raise NotImplementedError


class LocalRunner(Runner):
    """Local runner execute `launch` binary directly on local system."""

    def __init__(self) -> None:
        super().__init__()
        self.procs: dict[str, Popen] = {}

    def join(self, job: 'Job'):
        if (job_id := job._id) is None:
            raise RuntimeError('Job has no assigned identifier.')
        proc = self.procs[job_id]
        code = proc.wait()
        logger.info('locally spawned job finished: retcode=%d', code)

    def launch(self, job: 'Job', launch_bin: Path):
        # Encode job spec as chunked base64-encoded JSON.
        flags = Spec.from_job(job).to_flags_dict()
        command = [str(launch_bin.resolve())]
        for k, v in flags.items():
            if len(k) == 1:
                flag = f'-{k}'
            else:
                flag = f'--{k}'
            command += [flag, v]

        # TODO(@daskol): Find proper way to detach child process.
        job_id = str(uuid4())
        job._id = job_id
        self.procs[job_id] = Popen(command, start_new_session=True)

    def detach(self, job: 'Job'):
        if (job_id := job._id) is None:
            raise RuntimeError('Job has no assigned identifier.')
        self.procs.pop(job_id)


class MLSpaceRunner(Runner):
    """Job runner based on top of MLSpace Gateway v2 public API."""

    def __init__(self, region: str, **kwargs) -> None:
        super().__init__()

        self.region = region

        # Load all implementation lazily.
        from mlspace.api import Gateway
        self.gwapi = Gateway(**kwargs)

    def join(self, job: 'Job'):
        if (_job_id := job._id) is None:
            raise RuntimeError('Job has no assigned identifier.')
        raise NotImplementedError

    def launch(self, job: 'Job', launch_bin: Path):
        # TODO(@daskol): Find proper way to detach child process.
        if (base_image := job.image) is None:
            raise ValueError('Job image is not specified.')
        job._id = self.gwapi.job_run(
            script=str(launch_bin),
            base_image=base_image,
            instance_type='v100.1gpu',  # TODO(@daskol): Hardcoded.
            region=self.region,
            flags=Spec.from_job(job).to_flags_dict(),
        )

    def detach(self, job: 'Job'):
        if (_job_id := job._id) is None:
            raise RuntimeError('Job has no assigned identifier.')
        raise NotImplementedError


@dataclass
class Spec:
    """Intermediate wire representation for a :class:`Job`."""

    chunks: tuple[str, ...]

    version: int = 0

    MAX_ARG_STRLEN: ClassVar[int] = 65535  # Actual is `32 * PAGE_SIZE`.

    MAX_ARG_STRINGS: ClassVar[int] = 0x7FFFFFFF

    @classmethod
    def from_job(cls, job: 'Job') -> Self:
        chunks = []
        encoded_json = job.to_base64().decode('ascii')
        for i in range(len(encoded_json) // Spec.MAX_ARG_STRLEN + 1):
            begin = i * Spec.MAX_ARG_STRLEN
            end = begin + Spec.MAX_ARG_STRLEN
            chunks.append(encoded_json[begin:end])
        return cls(tuple(chunks))

    def to_flags_dict(self) -> dict[str, str]:
        flags = {
            'spec-version': f'{self.version}',
            'spec-num-chunks': f'{len(self.chunks)}',
        }
        for i, chunk in enumerate(self.chunks):
            flags[f'spec-chunk-{i}'] = chunk
        return flags


@dataclass
class Job:
    """Internal job representation."""

    executable: PathLike

    args: list[str] = field(default_factory=list)

    env: dict[str, str] = field(default_factory=dict)

    work_dir: PathLike | None = None

    shell: bool | PathLike = False

    image: str | None = None

    _runner: Runner = field(default_factory=LocalRunner)

    _id: str | None = None

    def to_base64(self) -> bytes:
        value = self.to_json().encode('utf-8')
        return b64encode(value)

    def to_dict(self) -> dict[str, Any]:
        obj: dict[str, Any] = {}
        for field in fields(self):  # noqa: F402
            if field.name.startswith('_'):
                continue
            obj[field.name] = getattr(self, field.name)
        obj = deepcopy(obj)
        obj['executable'] = str(self.executable)  # TODO(@daskol): Cast?
        return obj

    def to_json(self) -> str:
        # Produce as compact as possible JSON.
        return json.dumps(self.to_dict(), ensure_ascii=False, indent=None,
                          separators=(',', ':'))

    def detach(self):
        raise NotImplementedError

    def join(self):
        self._runner.join(self)

    def kill(self):
        raise NotImplementedError

    def launch(self, launch_bin: PathLike | None = None):
        if (launch_bin := launch_bin or config.launch_bin) is None:
            raise RuntimeError('No `launch_bin` found.')
        self._runner.launch(self, cast(Path, launch_bin))  # Not None!


@contextmanager
def launch(image: str | None, command: list[str], env: dict[str, str] = {},
           region: str | None = None, launch_bin: PathLike | None = None,
           run_local=False, **kwargs) -> Iterator[Job]:
    """Conctext manager for lauching and waiting jobs.

    Args:
      image: Container image in which job is spawned. If `image` is set to
      `None`, then job is launched locally (useful for dry runing and testing).
      command: Command to execute with its all arguments.
      env: Environment variables to add to job context before launching (see
      `man 3 execvpe` for details).
      region: Cluster where to spawn job.
    """
    if len(command) == 0:
        raise RuntimeError(f'No command to run; command is empty: {command}.')

    executable = Path(command[0])
    args_ = command[1:]

    _runner: Runner
    if run_local:
        _runner = LocalRunner()
    else:
        if region is None:
            raise RuntimeError(
                'Region must be non-empty for non-local executor.')
        _runner = MLSpaceRunner(region)

    job = Job(executable, args_, env, image=image, _runner=_runner, **kwargs)
    job.launch(launch_bin)
    try:
        yield job
    except Exception:
        pass
    finally:
        job.join()
