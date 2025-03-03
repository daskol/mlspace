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
from dataclasses import dataclass, field
from datetime import datetime
from http.client import HTTPConnection, HTTPSConnection
from typing import Literal
from urllib.parse import urlparse

from mlspace import config

__all__ = ('HealthParams', 'GatewayV2', 'PriorityClass')

API_SPEC_URL = 'https://api.ai.cloud.ru/public/v2/redoc'

PriorityClass = Literal['low', 'medium', 'high']

JobType = Literal['binary', 'horovod', 'pytorch', 'pytorch2',
                  'pytorch_elastic', 'spark', 'nogpu', 'binary_exp']


@dataclass
class Job:

    # TODO(@daskol): May be it is `JobStatus` or `JobInfo`?

    name: str

    status: Literal['Completed', 'Failed', 'Pending', 'Running']

    created_at: datetime


@dataclass
class HealthParams:
    """Configuration parameters for monitoring stuck jobs."""

    log_period: int

    action: Literal['delete', 'restart'] | None = None

    sub_actions: list[str] = field(default_factory=list)

    def __post_init__(self):
        if not (20 <= self.log_period <= 720):
            raise ValueError('Value of `log_period` must be in range '
                             f'[20, 720] (see {API_SPEC_URL}).')
        if self.action is None and len(self.sub_actions) == 0:
            raise ValueError('At least `action` or `sub_actions` must be '
                             f'specified (see {API_SPEC_URL}).')


class GatewayV2:
    f"""Partially bound MLSpace Public API v2.

    [1]: {API_SPEC_URL}
    """

    ENDPOINT = 'https://api.ai.cloud.ru'

    def __init__(self, api_key: str | None = None,
                 access_token: str | None = None,
                 workspace_id: str | None = None,
                 endpoint: str | None = None):
        # TODO(@daskol): Populate from module-level configuration object.
        self.access_token = access_token or config.access_token
        self.api_key = api_key or config.access_token
        self.workspace_id = workspace_id or config.access_token
        self.endpoint: str = endpoint or GatewayV2.ENDPOINT

        self.url = urlparse(self.endpoint)
        if self.url.hostname is None:
            raise ValueError(
                f'Endpoint does not have hostname: {self.endpoint}.')

        # TODO(@daskol): No retries out of the box.
        match self.url.scheme:
            case 'http':
                self.conn = HTTPConnection(self.url.hostname, self.url.port)
            case 'https':
                self.conn = HTTPSConnection(self.url.hostname, self.url.port)
            case _:
                raise ValueError(f'Unknown endpoint scheme: {self.endpoint}.')

        self.headers = {
            'authorization': f'Bearer {self.access_token}',
            'accept': 'application/json',
            'x-api-key': self.api_key,
            'x-workspace-id': self.workspace_id,
        }

    def ping(self) -> bool:
        """Test availablility of a remote or local mock server."""
        self.conn.request('GET', '/ping', None, self.headers)
        return self.conn.getresponse().status == 200

    def job_kill(self):
        pass

    def job_list(self):
        pass

    def job_logs(self):
        pass

    def job_run(
        self,
        script: str,
        base_image: str,
        instance_type: str,
        region: str | None = None,
        type_: str = 'binary',
        n_workers: int = 1,
        process_per_worker: int | None = None,
        job_desc: str | None = None,
        internet: bool | None = None,
        max_retry: int | None = None,
        priority_class: PriorityClass | None = None,
        health_params: HealthParams | None = None,
        flags: dict[str, str] = {},
        env_variables: dict[str, str] = {},
        **kwargs,
    ) -> str:
        """Run training jobs on various configurations of resources with useful
        options such as self-healing and monitoring.

        NOTE API spec says that `process_per_worker` default value is `default`
        but remote service responds with bad request (404).
        """
        params = {**locals()}
        params.pop('self')
        params.pop('kwargs')
        params['type'] = params.pop('type_')
        params.update(kwargs)
        filtered_params = {}
        for k, v in params.items():
            if v is None:
                continue
            if isinstance(v, dict | list | tuple) and len(v) == 0:
                continue
            filtered_params[k] = v
        body = json.dumps(filtered_params, ensure_ascii=False)

        self.conn.request('POST', '/public/v2/jobs', body, self.headers)
        res = self.conn.getresponse()
        if res.status != 200:
            payload = json.load(res)
            code = payload.get('error_code', 'no code')
            desc = payload.get('error_message', 'no description')
            raise RuntimeError(
                f'Request failed with status {res.status} ({res.reason}): '
                f'[{code}] {desc}.')

        payload = json.load(res)
        return payload['job_name']

    def job_status(self):
        pass
