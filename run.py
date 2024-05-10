#!/usr/bin/env python
import os, sys
import subprocess
current_path = os.path.dirname(__file__)

print(os.name, sys.platform)

build_path = os.path.join(current_path, "build-ninja")
if not os.path.exists(build_path):
    os.mkdir(build_path)
if not os.path.exists(os.path.join(build_path, "CMakeCache.txt")):
    subprocess.run("cmake -S . -B %s -G '%s'" % ("build-ninja", "Ninja Multi-Config"), shell=True, check=True)

subprocess.run("cmake --build %s --target '%s' --config %s" % ("build-ninja", "vinfony", "Debug"), shell=True, check=True)

if os.name == 'nt':
    subprocess.run("vinfonyd.exe", shell=True, check=True)
else:
    subprocess.run("./vinfonyd", shell=True, check=True)