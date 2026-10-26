"""Build and run the WilcoxonMqtt protocol tests on the host (simulated connection, no broker).

With --broker HOST[:PORT] [--user U --password P], also run the real source against that broker
over plain TCP from this computer (non-retained messages under wilcoxonmqtt/selftest/).
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

lab = Path(__file__).resolve().parents[1]
local_zig = next((parent / '.tools/native-compiler/ziglang/zig.exe' for parent in lab.parents
                  if (parent / '.tools/native-compiler/ziglang/zig.exe').exists()), None)
if os.environ.get('CXX'):
    compiler = [os.environ['CXX']]
elif shutil.which('g++'):
    compiler = ['g++']
elif shutil.which('clang++'):
    compiler = ['clang++']
elif local_zig:
    compiler = [str(local_zig), 'c++']
else:
    raise SystemExit('Install a native C++ compiler and set CXX to its executable path.')
extra = ['-nostdlib++', '-fno-exceptions', '-fno-rtti'] if 'zig' in Path(compiler[0]).name else []
parser = argparse.ArgumentParser()
parser.add_argument('--broker')
parser.add_argument('--user')
parser.add_argument('--password')
args = parser.parse_args()
flags = ['-std=c++11', '-Wall', '-Wextra', '-Werror', '-I' + str(lab / 'tests/stubs')]
with tempfile.TemporaryDirectory(prefix='wilcoxon-mqtt-') as directory:
    executable = Path(directory) / ('protocol_tests' + ('.exe' if os.name == 'nt' else ''))
    subprocess.run(compiler + extra + flags + [str(lab / 'tests/protocol_tests.cpp'), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
    if args.broker:
        live = Path(directory) / ('live_broker_test' + ('.exe' if os.name == 'nt' else ''))
        sockets = ['-lws2_32'] if os.name == 'nt' else []
        subprocess.run(compiler + extra + flags + [str(lab / 'tests/live_broker_test.cpp'), '-o', str(live)] + sockets, check=True)
        host, _, port = args.broker.partition(':')
        credentials = [args.user] + ([args.password] if args.password else []) if args.user else []
        subprocess.run([str(live), host, port or '1883'] + credentials, check=True)
