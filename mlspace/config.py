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

import warnings
from dataclasses import dataclass, field
from os import environ, getenv
from pathlib import Path
from typing import Callable

__all__ = ('PREFIX', 'config')

PREFIX: str = 'MLSPACE_'


def secret(name: str) -> str | None:
    var = f'{PREFIX}{name.upper()}'
    if (val := getenv(var)) is None:
        return None
    del environ[var]
    return val


def secret_fn(name: str) -> Callable[..., str | None]:
    return lambda: secret(name)


def path_fn(name: str) -> Callable[..., Path | None]:
    def fn() -> Path | None:
        for search_path in search_paths:
            path = (root_dir / search_path).resolve()
            if path.exists():
                return path
        warnings.warn(('Binary `launch` is not found at its expected location '
                       f'at {path}.'), RuntimeWarning)
        return None

    search_paths = ('launch', '../../../bin/launch')
    root_dir = Path(__file__).parent
    return fn


@dataclass(slots=True)
class SecretConfig:

    _access_token: str | None = field(default_factory=secret_fn('api_key'))

    _api_key: str | None = field(default_factory=secret_fn('api_key'))

    _workspace_id: str | None = field(default_factory=secret_fn('api_key'))

    @property
    def access_token(self) -> str:
        return self._property_safe('access_token')

    @property
    def api_key(self) -> str:
        return self._property_safe('api_key')

    @property
    def workspace_id(self) -> str:
        return self._property_safe('workspace_id')

    def _property_safe(self, var: str) -> str:
        if (val := getattr(self, f'_{var}', None)) is None:
            raise RuntimeError(f'Secret `{var}` is not specified.')
        return val


@dataclass(slots=True)
class Config(SecretConfig):

    launch_bin: Path | None = field(default_factory=path_fn('launch_bin'))


config = Config()  # TODO(@daskol): Not thread-safe.
