import os
import subprocess

root_dir = "src"

CXX_FLAGS = ["--std=c++11", "-Wall", "-Wextra", "-Wpedantic"]


files = set()

for _directory, _, _files in os.walk(root_dir):
    for file_name in _files:
        ext = file_name.split(".")[-1]
        if ext in ("c", "cpp", "cxx"):
            rel_file = os.path.join("..", _directory, file_name)
            files.add(rel_file)


if not os.path.exists("obj"):
    os.mkdir("obj")
os.chdir("obj")
subprocess.run("mpicxx -c " + " ".join(CXX_FLAGS) + " " + " ".join(files), shell = True)
subprocess.run("mpicxx *.o -o winemakers " + ' '.join(CXX_FLAGS) + " && mv winemakers ..", shell = True)
subprocess.run("cd ../ && rm -rf obj", shell = True)
