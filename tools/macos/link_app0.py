"""Fill app0/ with symlinks to the dump, keeping directories the relinker created as real directories."""
import os, sys

def mirror(source, target):
    for name in sorted(os.listdir(source)):
        src, dst = os.path.join(source, name), os.path.join(target, name)
        if os.path.isdir(dst) and not os.path.islink(dst):
            mirror(src, dst)
        elif not os.path.lexists(dst):
            os.symlink(src, dst)

mirror(os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2]))
