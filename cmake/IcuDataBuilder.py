import builtins
import runpy
import sys

builtins.exit = sys.exit
sys.path.insert(0, sys.argv.pop(1))
runpy.run_module("icutools.databuilder", run_name="__main__")
