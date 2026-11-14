from pathlib import Path
import subprocess
import sys


def main():
    genrb, icupkg, source, package, destination = sys.argv[1:]
    output = Path(destination)
    output.parent.mkdir(parents=True, exist_ok=True)
    compiled = output.parent / "resources"
    compiled.mkdir(exist_ok=True)
    subprocess.run([genrb, "-e", "UTF-8", "-d", str(compiled), source], check=True)
    subprocess.run([icupkg, "-a", "aps5ResourceFixture.res", "-s", str(compiled), package, str(output)], check=True)


if __name__ == "__main__":
    main()
