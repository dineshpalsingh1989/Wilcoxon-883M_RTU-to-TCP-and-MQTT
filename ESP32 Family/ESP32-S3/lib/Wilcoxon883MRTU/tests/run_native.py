"""Build and execute the real C++ protocol core on the host; no Modbus libraries."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

lab = Path(__file__).resolve().parents[1]
repo = lab.parents[1]
local_zig = repo / '.tools/native-compiler/ziglang/zig.exe'
if os.environ.get('CXX'):
    compiler = [os.environ['CXX']]
elif shutil.which('g++'):
    compiler = ['g++']
elif shutil.which('clang++'):
    compiler = ['clang++']
elif local_zig.exists():
    compiler = [str(local_zig), 'c++']
else:
    raise SystemExit('Install a native C++ compiler and set CXX to its executable path.')
extra = ['-nostdlib++', '-fno-exceptions', '-fno-rtti', '-DOWNMODBUS_NO_CXX_RUNTIME'] if 'zig' in Path(compiler[0]).name else []
with tempfile.TemporaryDirectory(prefix='own-modbus-') as directory:
    for source in sorted((lab / 'tests').glob('*_tests.cpp')):
        executable = Path(directory) / (source.stem + ('.exe' if os.name == 'nt' else ''))
        subprocess.run(compiler + extra + ['-std=c++11', '-Wall', '-Wextra', '-Werror',
                       '-I' + str(lab / 'src'), str(source), '-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
