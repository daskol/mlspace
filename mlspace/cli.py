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

"""A simple alternative for launching jobs in MLSpace at Cloud.RU."""

import json
import logging
import sys
from argparse import ArgumentParser, FileType, Namespace
from pathlib import Path
from sys import stderr

LOGGING_LEVELS = {
    'debug': logging.DEBUG,
    'info': logging.INFO,
    'warn': logging.WARNING,
    'error': logging.ERROR,
}

LOGGING_FMT = '%(levelname)s %(asctime)s %(module)s %(message)s'

LOGGING_DATEFMT = '%m-%d %H:%M:%S'  # No year.

logger: logging.Logger


class Formatter(logging.Formatter):

    def __init__(self, *args, is_tty=False, **kwargs):
        super().__init__(*args, **kwargs)
        self.is_tty = is_tty

    def format(self, record):
        ch = record.levelname[:1]
        if self.is_tty:
            match ch:
                case 'W':
                    ch = f'\033[33m{ch}\033[0m'
                case 'E' | 'F':
                    ch = f'\033[31m{ch}\033[0m'
        record.levelname = ch
        return super().format(record)


def launch(ns: Namespace) -> int:
    if (image := ns.image) is None and not ns.local:
        logger.error('no container image is specified')
        return 1

    command: list[str] = ns.command
    command_str = ' '.join(f'"{x}"' if ' ' in x else x for x in command)
    logger.info('job command: %s', command_str)

    env_list: list[str] = ns.env
    env: dict[str, str] = {}
    for ent in sorted(env_list):
        match len(res := ent.split('=')):
            case 1:
                var, val = res[0], ''
            case 2:
                var, val = res

        if var in env:
            logger.warning('envvar %s occurs multiple types', var)
        env[var] = val

    if env:
        logger.info('environment variables are %s',
                    json.dumps(env, ensure_ascii=False, indent=2))
    else:
        logger.info('no auxiliary environment variables specified')

    if (launch_bin := ns.launch_bin) is not None:
        if not launch_bin.exists():
            logger.warning(
                'specified `launch` binary does not exist locally: %s',
                launch_bin)

    # Import all related subpackages as late as possible for better UX.
    from mlspace.launch import launch
    with launch(image, command, env, run_local=ns.local) as job:
        if ns.detach:
            job.detach
            return 0
        return 1


def main() -> int:
    # Parse command line arguments. If no subcommand were run then show usage
    # and exit. We assume that only main parser (super command) has valid value
    # in func attribute.
    ns: Namespace = parser.parse_args()

    # Set up basic logging configuration.
    if (stream := ns.log_output) is None:
        stream = stderr
    logging.basicConfig(format=LOGGING_FMT, datefmt=LOGGING_DATEFMT,
                        level=logging.WARNING, stream=stream)

    # Configure application-level logger.
    log_level = LOGGING_LEVELS[ns.log_level]
    formatter = Formatter(fmt=LOGGING_FMT, datefmt=LOGGING_DATEFMT,
                          is_tty=stream.isatty())

    console_handler = logging.StreamHandler(stream)
    console_handler.setFormatter(formatter)
    console_handler.setLevel(log_level)

    global logger
    logger = logging.getLogger('mlspace')
    logger.setLevel(log_level)
    logger.addHandler(console_handler)
    logger.propagate = False

    if (ret := launch(ns)) is not None:
        sys.exit(ret)


parser = ArgumentParser(description=__doc__)
parser.add_argument('command', nargs='+', help='command to run')

parser.add_argument(
    '-l', '--local', default=False, action='store_true',
    help='run job locally (useful for testing)')
parser.add_argument('-n', '--dry-run', default=False, action='store_true')

g_job = parser.add_argument_group('job options')
g_job.add_argument(
    '-d', '--detach', default=False, action='store_true',
    help='do not want until job is finished')
g_job.add_argument(
    '-e', '--env', default=[], action='append',
    help='environment variable in form of `VAR[=VAL]`')
g_job.add_argument(
    '-i', '--image', default=None, help='container image where to spawn job')
g_job.add_argument(
    '-s', '--shell', default=False, nargs='?',
    help='spawn a job with a shell and what shell')
g_job.add_argument(
    '-w', '--work-dir', type=Path, help='override job working directory')
g_job.add_argument(
    '--launch-bin', type=Path, metavar='PATH',
    help='override path to `launch` binary to use to spawn job')

g_log = parser.add_argument_group('logging options')
g_log.add_argument(
    '--log-level', default='info', choices=sorted(LOGGING_LEVELS.keys()),
    help='set logger verbosity level')
g_log.add_argument(
    '--log-output', default=stderr, metavar='FILENAME', type=FileType('w'),
    help='set output file or stderr (-) for logging')

g_api = parser.add_argument_group('mlspace api options')
g_api.add_argument('--endpoint', default=None, help='not implemented')
