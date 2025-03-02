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

import base64
import json
from pathlib import Path

from mlspace.launch import Job, Spec, launch


class TestSpec:

    def test_from_job(self):
        job = Job(executable=Path('/usr/bin/env'), args=['-i'],
                  env={'VAR': 'VAL'})
        spec = Spec.from_job(job)
        assert spec.version == 0
        assert len(spec.chunks) == 1

        flags = spec.to_flags_dict()
        assert flags != {}
        assert flags['spec-version'] == '0'
        assert flags['spec-num-chunks'] == '1'
        assert flags['spec-chunk-0'] != ''

        chunk = flags['spec-chunk-0']
        payload = base64.b64decode(chunk)
        obj = json.loads(payload)
        assert isinstance(obj, dict)
        assert obj['executable'] == str(job.executable)
        assert obj['args'] == job.args
        assert obj['env'] == job.env


def test_launch():
    command = ['python', '-m', 'mylib', 'train', 'config/example.toml']
    with launch(None, command) as job:
        assert job is not None  # Dummy assertion.
