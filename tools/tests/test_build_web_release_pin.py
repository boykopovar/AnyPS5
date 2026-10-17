import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_web


class ReleasePinTests(unittest.TestCase):
    def test_publishes_assets_from_the_requested_release(self):
        tag = "v1.2.3"
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        output = Path(temporary.name)
        assets = []
        responses = {}
        for platform in ("windows", "linux"):
            buffer = io.BytesIO()
            with zipfile.ZipFile(buffer, "w") as archive:
                archive.writestr("libs/libkernel.prx", platform.encode())
            data = buffer.getvalue()
            url = f"https://example.invalid/{platform}.zip"
            assets.append({
                "name": f"prx-{platform}-{tag}.zip",
                "size": len(data),
                "digest": "sha256:" + hashlib.sha256(data).hexdigest(),
                "browser_download_url": url,
            })
            responses[url] = data
        release = {"tag_name": tag, "html_url": "https://example.invalid/release", "assets": assets}
        api_url = f"https://api.github.com/repos/boykopovar/AnyPS5/releases/tags/{tag}"
        responses[api_url] = json.dumps(release).encode()
        requested = []

        def fetch(url):
            requested.append(url)
            return io.BytesIO(responses[url])

        with patch.object(build_web, "request", side_effect=fetch), patch("builtins.print"):
            build_web.publish_libraries(output, tag)

        manifest = json.loads((output / "release.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["tag"], tag)
        self.assertEqual(requested[0], api_url)
        self.assertEqual(set(manifest["platforms"]), {"windows", "linux"})

    def test_rejects_an_invalid_release_tag_before_requesting_it(self):
        with tempfile.TemporaryDirectory() as temporary, patch.object(build_web, "request") as request:
            with self.assertRaisesRegex(ValueError, "Invalid release tag"):
                build_web.publish_libraries(Path(temporary), "../latest")
            request.assert_not_called()


if __name__ == "__main__":
    unittest.main()
