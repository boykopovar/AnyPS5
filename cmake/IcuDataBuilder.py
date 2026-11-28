import builtins
import os
import runpy
import subprocess
import sys

subprocess_call = subprocess.call


def call(command, *args, **kwargs):
    if os.name == "nt" and kwargs.get("shell"):
        kwargs["shell"] = False
    return subprocess_call(command, *args, **kwargs)


builtins.exit = sys.exit
subprocess.call = call
sys.path.insert(0, sys.argv.pop(1))
runpy.run_module("icutools.databuilder", run_name="__main__")
