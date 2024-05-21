#!/usr/bin/env python
import os, sys
import subprocess
current_path = os.path.dirname(__file__)

print(os.name, sys.platform)

target = "vinfony"
executable = "vinfonyd"
build_name = "build-ninja"
build_mode = "Debug"

build_path = os.path.join(current_path, build_name)
if not os.path.exists(build_path):
  os.mkdir(build_path)
if not os.path.exists(os.path.join(build_path, "CMakeCache.txt")):
  subprocess.run("cmake -S . -B %s -G \"%s\"" % (build_name, "Ninja Multi-Config"), shell=True, check=True)

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
      case "test":
        target = "test_sequencer"
        executable = "%s/%s/test_sequencer" %  (build_name, build_mode)
      case "tsf":
        target = "test_model"
        executable = "%s/%s/test_tsf3" %  (build_name, build_mode)
      case "model":
        target = "test_model"
        executable = "%s/%s/test_model" %  (build_name, build_mode)
      case _:
        raise Exception("invalid cmd %s" % (cmd,))

shellcmd = "cmake --build %s --target \"%s\" --config %s" % (build_name, target, build_mode)
print(shellcmd)
subprocess.run(shellcmd, shell=True, check=True)

shellcmd = "./%s" % (executable,)
if os.name == 'nt':
  shellcmd = "%s.exe" %  (executable.replace("/", "\\"),)

if len(passargs) > 0:
  shellcmd += " \"" + "\" \"".join(passargs) + "\""

print(shellcmd)
subprocess.run(shellcmd, shell=True, check=True)