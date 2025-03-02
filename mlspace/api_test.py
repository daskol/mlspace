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

from typing import Iterator

import pytest

from mlspace.api import GatewayV2, HealthParams
from mlspace.testing import MockServer, spawn_mock_server


@pytest.fixture(scope='session')
def gwapi() -> Iterator[MockServer]:
    server, thread = spawn_mock_server()
    try:
        yield server
    except Exception:
        pass
    finally:
        server.stop()
        thread.join()


class TestHealthParams:

    def test_init(self):
        hp = HealthParams(30, 'delete', ['notify'])
        assert hp.log_period == 30
        assert hp.action == 'delete'
        assert hp.sub_actions == ['notify']

    def test_validate_log_period(self):
        with pytest.raises(ValueError):
            _ = HealthParams(10, action='delete')

    def test_validate_actions(self):
        with pytest.raises(ValueError):
            _ = HealthParams(20)


class TestGatewayV2:

    KWARGS = {
        'api_key': 'API_KEY',
        'access_token': 'JWT',
        'workspace_id': '00000000-0000-0000-0000-000000000000',
    }

    def test_ping(self, gwapi: MockServer):
        cli = GatewayV2(**self.KWARGS, endpoint=gwapi.endpoint)
        assert cli.ping()
        assert cli.ping()

    def test_job_run(self, gwapi: MockServer):
        cli = GatewayV2(**self.KWARGS, endpoint=gwapi.endpoint)
        job_name = cli.job_run(
            script='/home/user/.cache/mlspace/launch',
            base_image='cr.ai.cloud.ru/8ff21ec0-666d-4950/job-example',
            instance_type='v100.1gpu',
        )
        assert isinstance(job_name, str)
        assert job_name.startswith('')
