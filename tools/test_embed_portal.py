"""Check portal packaging without modifying firmware resources or device state."""
import gzip
import re
import tempfile
import unittest
from pathlib import Path
from embed_portal import generate


class PortalPackagingTests(unittest.TestCase):
    def test_line_endings_and_repeat_builds(self):
        with tempfile.TemporaryDirectory(prefix="aurageek-portal-") as temporary:
            root = Path(temporary)
            (root / "web").mkdir()
            (root / "src/web").mkdir(parents=True)
            source = root / "web/portal.html"
            output = root / "src/web/PortalPage.generated.cpp"
            original = "<!doctype html>\n<title>AuraGeek 配置</title>\n".encode("utf-8")
            source.write_bytes(original)
            generate(root)
            expected = output.read_bytes()
            source.write_bytes(original.replace(b"\n", b"\r\n"))
            generate(root)
            self.assertEqual(output.read_bytes(), expected)
            generate(root)
            self.assertEqual(output.read_bytes(), expected)
            packed = bytes(int(value, 16) for value in re.findall(rb"0x([0-9a-f]{2})", expected))
            self.assertEqual(packed[9], 255)
            self.assertEqual(gzip.decompress(packed), original)


if __name__ == "__main__":
    unittest.main()
