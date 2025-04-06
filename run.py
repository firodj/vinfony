#!/usr/bin/env python
import os, sys
import subprocess
from dotenv import load_dotenv
load_dotenv()
EXT_PATH = os.environ.get('EXT_PATH')
current_path = os.path.dirname(__file__)

print(os.name, sys.platform)

target = "vinfony"
executable = "vinfonyd"
build_name = "build-ninja"
build_mode = "Debug"
generator_name = "Ninja Multi-Config"
bin_path = os.path.join(current_path, "bin")
build_path = os.path.join(current_path, build_name)
runtime_path = bin_path
generator_ext = ""
if os.name == "nt":
  build_name = "build-msvc"
  # https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2017%202022.html
  generator_name = "Visual Studio 17 2022"
  generator_ext = "-A x64"

passargs = []
args = sys.argv[1:]
if len(args) >= 1:
  for i in range(0, len(args)):
    if args[i] == '--':
      passargs = args[i+1:]
      args = args[:i]
      break

  if len(args) >= 1:
    cmd = args[0]
    match cmd:
## -- commands
      case "test":
        target = "test_sequencer"
      case "tsf":
        target = "test_tsf3"
      case "tsf2":
        target = "test_tsf2"
      case "model":
        target = "test_model"
## -- end
      case _:
        raise Exception("invalid cmd %s" % (cmd,))

bin_path = os.path.join(current_path, "bin")
build_path = os.path.join(current_path, build_name)
cmake_options = []
if EXT_PATH:
  cmake_options.append("-DEXT_PATH="+EXT_PATH)

if not os.path.exists(build_path):
  os.mkdir(build_path)
if not os.path.exists(os.path.join(build_path, "CMakeCache.txt")):
  subprocess.run("cmake %s -S . -B %s -G \"%s\"" % (" ".join(cmake_options), build_name, generator_name), shell=True, check=True)
if generator_name == "Ninja Multi-Config" and not os.path.exists(os.path.join(build_path, "build-%s.ninja" % (build_mode,))):
  subprocess.run("cmake -S . -B %s" % (build_name,), shell=True, check=True)

shellcmd = "cmake --build %s --target \"%s\" --config %s" % (build_name, target, build_mode)
print(shellcmd)
subprocess.run(shellcmd, shell=True, check=True)

if executable:
  shellcmd = "%s/%s" % (runtime_path, executable,)
  if os.name == 'nt':
    shellcmd = "%s\\%s.exe" %  (runtime_path, executable.replace("/", "\\"),)

  if len(passargs) > 0:
    shellcmd += " \"" + "\" \"".join(passargs) + "\""

  print(shellcmd)
  subprocess.run(shellcmd, shell=True, check=True)