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
from base64 import b64encode
from contextlib import contextmanager
from dataclasses import asdict, dataclass, field
from os import PathLike
from pathlib import Path
from typing import Any, Iterator


@dataclass
class Job:

    executable: PathLike

    args: list[str] = field(default_factory=list)

    env: dict[str, str] = field(default_factory=dict)

    work_dir: PathLike | None = None

    shell: bool | PathLike = False

    image: str | None = None

    def to_base64(self) -> bytes:
        value = self.to_json().encode('utf-8')
        return b64encode(value)

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)

    def to_json(self) -> str:
        # Produce as compact as possible JSON.
        return json.dumps(self.to_dict(), ensure_ascii=False, indent=None,
                          separators=(',', ':'))

    def detach(self):
        ...

    def join(self):
        ...

    def kill(self):
        ...

    def launch(self):
        ...


@contextmanager
def launch(image: str | None, command: list[str], env: dict[str, str] = {},
           **kwargs) -> Iterator[Job]:
    if len(command) == 0:
        raise RuntimeError(f'No command to run; command is empty: {command}.')

    executable = Path(command[0])
    args_ = command[1:]
    job = Job(executable, args_, env, image=image, **kwargs)
    try:
        yield job
    except Exception:
        pass
    finally:
        job.join()
