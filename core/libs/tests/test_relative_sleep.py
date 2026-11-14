import subprocess
import sys


def main():
    for milliseconds in (1 << 32, 1 << 63, (1 << 64) - 1):
        process = subprocess.Popen(
            [sys.argv[1], str(milliseconds)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            bufsize=0,
        )
        try:
            for line in process.stdout:
                if line.strip() == b"sleeping":
                    break
            else:
                output, errors = process.communicate()
                raise AssertionError(f"the relative-sleep probe did not start: {output!r}, {errors!r}")
            try:
                output, errors = process.communicate(timeout=0.2)
            except subprocess.TimeoutExpired as error:
                output = error.stdout or b""
                if b"returned" in output or b"failed" in output:
                    raise AssertionError(f"sleep({milliseconds}) finished early: {output!r}")
                continue
            raise AssertionError(
                f"sleep({milliseconds}) returned early: {process.returncode}, {output!r}, {errors!r}"
            )
        finally:
            if process.poll() is None:
                process.kill()
            process.communicate()
    print("64-bit relative sleep checks passed")


if __name__ == "__main__":
    main()
