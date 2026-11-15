import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[4]
CMAKE = shutil.which("cmake")


class JpegToolchain(unittest.TestCase):
    def build(self, configured):
        self.assertIsNotNone(CMAKE, "CMake is required for the external-project fixture")
        with tempfile.TemporaryDirectory(prefix="jpeg toolchain ") as directory:
            root = pathlib.Path(directory)
            source = root / "source"
            jpeg = source / "3rdparty/libjpeg-turbo"
            (jpeg / "src").mkdir(parents=True)
            (source / "3rdparty/stb").mkdir()
            probe = "int jpeg_toolchain_probe(void) { return 123; }\n"
            if configured:
                probe = "#if !defined(ANYPS5_JPEG_TOOLCHAIN_PROBE) || ANYPS5_JPEG_TOOLCHAIN_PROBE != 123\n#error nested JPEG build lost the selected toolchain\n#endif\n" + probe
            (jpeg / "probe.c").write_text(probe)
            (jpeg / "CMakeLists.txt").write_text("""cmake_minimum_required(VERSION 3.20)
project(JpegToolchainChild LANGUAGES C)
add_library(turbojpeg STATIC probe.c)
if(MSVC)
    set_target_properties(turbojpeg PROPERTIES OUTPUT_NAME turbojpeg-static)
endif()
install(TARGETS turbojpeg ARCHIVE DESTINATION lib)
""")
            helper = (ROOT / "core/Decoder/Jpeg/CMakeLists.txt").as_posix()
            (source / "CMakeLists.txt").write_text(f"""cmake_minimum_required(VERSION 3.20)
project(JpegToolchainParent LANGUAGES C CXX)
set(BUILD_TESTING OFF)
add_library(probe_runtime INTERFACE)
include([==[{helper}]==])
add_jpeg_decoder(probe_decoder probe_runtime)
""")
            command = [CMAKE, "-S", str(source), "-B", str(root / "build"), "-DCMAKE_BUILD_TYPE=Release"]
            if configured:
                toolchain = root / "selected toolchain.cmake"
                toolchain.write_text('set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -DANYPS5_JPEG_TOOLCHAIN_PROBE=123")\n')
                command.append("-DCMAKE_TOOLCHAIN_FILE=" + str(toolchain))
            self.run_command(command)
            self.run_command([CMAKE, "--build", str(root / "build"), "--target", "jpeg_turbo_external", "--parallel", "1", "--config", "Release"])
            archives = list((root / "build/libjpeg-turbo/lib").glob("*turbojpeg*"))
            self.assertTrue(archives)
            self.assertTrue(all(path.stat().st_size > 0 for path in archives))

    def run_command(self, command):
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.assertEqual(result.returncode, 0, " ".join(command) + "\n" + result.stdout[-7000:])

    def test_selected_toolchain_reaches_nested_compilation(self):
        self.build(True)

    def test_native_build_without_toolchain_still_builds(self):
        self.build(False)


if __name__ == "__main__":
    unittest.main()
