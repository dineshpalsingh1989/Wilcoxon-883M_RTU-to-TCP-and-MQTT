"""Run the real portable protocol core and Arduino-facing RTU adapter with fake I/O."""
import os,sys,subprocess,tempfile,shutil
from pathlib import Path
lib=Path(__file__).resolve().parents[1]
repo=next((p for p in lib.parents if (p/'.tools/native-compiler/ziglang/zig.exe').exists()),None)
if os.environ.get('CXX'):compiler=[os.environ['CXX']]
elif shutil.which('g++'):compiler=['g++']
elif shutil.which('clang++'):compiler=['clang++']
elif repo:compiler=[str(repo/'.tools/native-compiler/ziglang/zig.exe'),'c++']
else:sys.exit('Install a native C++ compiler or set CXX.')
extra=['-nostdlib++','-fno-exceptions','-fno-rtti','-DOWNMODBUS_NO_CXX_RUNTIME'] if 'zig' in Path(compiler[0]).name else []
with tempfile.TemporaryDirectory(prefix='ila-modbus-') as tmp:
 for source in sorted((lib/'tests').glob('*_tests.cpp')):
  exe=Path(tmp)/(source.stem+('.exe' if os.name=='nt' else ''))
  defines=['-DARDUINO_ARCH_ESP32'] if source.stem=='adapter_tests' else []
  subprocess.run(compiler+extra+['-std=c++11','-Wall','-Wextra','-Werror','-I'+str(lib/'tests/stubs'),'-I'+str(lib/'src')]+defines+[str(source),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
