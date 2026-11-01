import re
import unittest
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
KEY = re.compile(r"^\s*key: (ffmpeg-.*?)\s*$", re.M)


class FfmpegCacheKeyTests(unittest.TestCase):
    def keys(self, name):
        text = (REPOSITORY / ".github/workflows" / name).read_text(encoding="utf-8")
        return KEY.findall(text)

    def test_the_winlibs_build_names_its_toolchain_in_the_ffmpeg_key(self):
        keys = self.keys("build.yml")
        self.assertEqual(len(keys), 1)
        self.assertIn("-winlibs-${{ env.WINLIBS_ZIP }}-", keys[0])

    def test_the_msys2_build_names_its_toolchain_in_the_ffmpeg_key(self):
        keys = self.keys("windows-msys2.yml")
        self.assertEqual(len(keys), 1)
        self.assertIn("-msys2-", keys[0])

    def test_the_two_toolchains_never_share_an_ffmpeg_cache_entry(self):
        self.assertNotEqual(self.keys("build.yml")[0], self.keys("windows-msys2.yml")[0])


if __name__ == "__main__":
    unittest.main()
