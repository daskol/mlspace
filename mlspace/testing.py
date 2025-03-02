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
from argparse import ArgumentParser, Namespace
from codecs import getwriter
from http.server import BaseHTTPRequestHandler, HTTPServer
from io import BytesIO
from random import randint
from threading import Thread
from typing import Any, Sequence
from uuid import uuid4

parser = ArgumentParser(description='Simple mock server for testing purposes.')
parser.add_argument('-H', '--host', default='localhost')
parser.add_argument('-p', '--port', type=int, default=8080)


def has_required_keys(obj: dict[str, Any], keys: Sequence[str]):
    for key in keys:
        if key not in obj:
            raise ValueError(f'Missing key {key} in request body.')


class RequestHandler(BaseHTTPRequestHandler):

    def __init__(self, *args, quiet=False, **kwargs):
        self.quiet = quiet
        super().__init__(*args, **kwargs)

    def log_message(self, format: str, *args):
        if not self.quiet:
            super().log_message(format, *args)

    def do_GET(self):
        if self.path == '/ping':
            self.send_response(200)
            self.end_headers()
        else:
            self.send_error(404)
        self.log_request

    def do_POST(self):
        try:
            if self.path == '/public/v2/jobs':
                self.handle_job_run()
            else:
                self.send_error(404)
        except ValueError:
            self.send_error(400)

    def handle_job_run(self):
        req = self.read_json()
        has_required_keys(req, ('script', 'base_image', 'instance_type'))
        res = {'job_name': f'lm-mpi-job-{uuid4()}'}
        self.send_json(res)

    def send_json(self, obj):
        buf = BytesIO()
        writer = getwriter('utf-8')(buf)
        json.dump(obj, writer, ensure_ascii=False)
        content_length = buf.tell()
        buf.seek(0)

        self.send_response(200)
        self.send_header('content-type', 'application/json')
        self.send_header('content-length', str(content_length))
        self.end_headers()
        self.wfile.write(buf.getbuffer())

    def read_body(self) -> bytes:
        if (value := self.headers.get('content-length')) is None:
            raise ValueError
        content_length = int(value)
        return self.rfile.read(content_length)

    def read_json(self) -> dict[str, Any]:
        body = self.read_body()
        try:
            return json.loads(body)
        except json.JSONDecodeError as e:
            raise ValueError from e


class MockServer:
    """Simple mock server for `mlspace.api.GatewayV2`.

    NOTE It is not completely thread-safe.
    """

    def __init__(self, host: str, port: int, quiet=False):
        self.host = host
        self.port = port
        self.endpoint = f'http://{self.host}:{self.port}'

        def fn(*args, **kwargs):
            return RequestHandler(*args, quiet=quiet, **kwargs)

        # TODO(@daskol): Are sockets managed inproperly in abnormal conditions?
        # TODO(@daskol): Reusable?
        self.server = HTTPServer((self.host, self.port), fn)

    def run(self):
        with self.server as httpd:
            httpd.serve_forever()

    def stop(self):
        self.server.shutdown()


def spawn_mock_server(host='localhost',
                      num_attempts=5) -> tuple[MockServer, Thread]:
    for _ in range(num_attempts):
        port = randint(4096, 65535)
        try:
            server = MockServer(host, port, quiet=True)
        except OSError:
            continue
        thread = Thread(target=server.run)
        thread.start()
        return server, thread
    else:
        raise RuntimeError('Failed to start mock server on random port in '
                           f'{num_attempts} attempts.')


def main() -> None:
    ns: Namespace = parser.parse_args()
    MockServer(ns.host, ns.port).run()


if __name__ == '__main__':
    main()
