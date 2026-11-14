import re
import sys
import unittest
from pathlib import Path

DRIVER = Path(__file__).resolve().parents[1]

LOOKUP = re.compile(r'Function<PFN_vkCmd\w+>\(\s*"vkCmd\w+"\s*\)(\s*\()?')
DEVICE_PROC = re.compile(r'deviceProc\([^;]*"vkCmd\w+"')

AUDITED = {
    "Graphics/src/Resources.cpp": (23, "fills the device table, whose vkCmd entries the command encoder then replaces"),
    "Graphics/src/Recorder.cpp": (10, "the recorder's members, replaced by the encoder's table entries where it has them; every other use takes the batch through BatchCommands::Use::Quiesced, plus the two checkpoint markers written into live()"),
    "Execution/src/PresentationScaler.cpp": (4, "the presentation's own command buffers, which the encoder never records"),
    "Execution/src/PresentationPass.cpp": (1, "the presentation's own command buffers, which the encoder never records"),
    "Execution/src/VulkanDevice.cpp": (3, "the presentation's own command buffers and their checkpoint markers"),
    "Graphics/src/Texture.cpp": (4, "a lookup that settles the encoder, followed by straight-line calls with no encoding window in between"),
    "Graphics/src/DepthSurface.cpp": (3, "two settling lookups with straight-line calls, and the cached push-descriptor pointer of DepthToStoragePass::Record, which runs with no encoding window on its path"),
    "Graphics/src/Draw.cpp": (1, "a settling lookup followed by straight-line calls, with no encoding window in between"),
    "Graphics/src/GpuColorTransfer.cpp": (1, "a settling lookup followed by straight-line calls, with no encoding window in between"),
    "Graphics/src/PlaceholderImages.cpp": (1, "a settling lookup followed by straight-line calls, with no encoding window in between"),
}


def raw_sites(text):
    sites = []
    for number, line in enumerate(text.splitlines(), 1):
        for match in LOOKUP.finditer(line):
            if match.group(1) is None:
                sites.append(number)
        if DEVICE_PROC.search(line):
            sites.append(number)
    return sites


def scan(root):
    found = {}
    for path in sorted(list((root / "Graphics").rglob("*.[ch]pp")) + list((root / "Execution").rglob("*.[ch]pp"))):
        sites = raw_sites(path.read_text(encoding="utf-8", errors="replace"))
        if sites:
            found[path.relative_to(root).as_posix()] = sites
    return found


def problems(found, audited):
    messages = []
    for name, sites in sorted(found.items()):
        expected = audited.get(name, (0, ""))[0]
        if len(sites) > expected:
            messages.append(f"{name}: {len(sites)} raw vkCmd pointers, {expected} audited (lines {', '.join(map(str, sites))}). A raw pointer called inside an encoding window records ahead of the window's queued commands: call the device table entry, look the command up where it is called, or quiesce, and add the site to AUDITED with its reason")
        elif len(sites) < expected:
            messages.append(f"{name}: {len(sites)} raw vkCmd pointers, {expected} audited: lower its AUDITED count")
    for name, (expected, _) in sorted(audited.items()):
        if name not in found and expected != 0:
            messages.append(f"{name}: no raw vkCmd pointers, {expected} audited: remove it from AUDITED")
    return messages


class RawSitesTest(unittest.TestCase):
    def test_a_called_lookup_is_not_a_raw_site(self):
        self.assertEqual(raw_sites('context.Function<PFN_vkCmdDraw>("vkCmdDraw")(commands, 3, 1, 0, 0);'), [])
        self.assertEqual(raw_sites('state->DeviceFunction<PFN_vkCmdDraw>( "vkCmdDraw" ) (commands, 3, 1, 0, 0);'), [])

    def test_a_kept_lookup_or_device_proc_pointer_is_a_raw_site(self):
        text = '\n'.join([
            'const auto copy = context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer");',
            'int unrelated = 0;',
            'auto write = reinterpret_cast<PFN_vkCmdWriteBufferMarkerAMD>(context.deviceProc(context.device, "vkCmdWriteBufferMarkerAMD"));',
            'functions.cmdDraw = context.Function<PFN_vkCmdDraw>("vkCmdDraw");',
            'const auto device = context.Function<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle");',
        ])
        self.assertEqual(raw_sites(text), [1, 3, 4])

    def test_counts_above_below_or_missing_from_the_audit_fail(self):
        audited = {"a.cpp": (1, "reason"), "gone.cpp": (2, "reason")}
        messages = problems({"a.cpp": [3, 9], "new.cpp": [5]}, audited)
        self.assertTrue(any(message.startswith("a.cpp: 2 raw vkCmd pointers, 1 audited (lines 3, 9)") for message in messages))
        self.assertTrue(any(message.startswith("new.cpp: 1 raw vkCmd pointers, 0 audited (lines 5)") for message in messages))
        self.assertTrue(any(message.startswith("gone.cpp: no raw vkCmd pointers") for message in messages))
        self.assertEqual(problems({"a.cpp": [3]}, {"a.cpp": (1, "reason")}), [])
        self.assertEqual(problems({"a.cpp": []}, {}), [])
        self.assertTrue(problems({}, {"a.cpp": (1, "reason")}))
        self.assertTrue(any("lower its AUDITED count" in message for message in problems({"a.cpp": [3]}, {"a.cpp": (2, "reason")})))

    def test_every_audit_entry_names_its_reason(self):
        for name, (count, reason) in AUDITED.items():
            self.assertGreater(count, 0, name)
            self.assertTrue(reason, name)


class DriverTreeTest(unittest.TestCase):
    def test_the_driver_holds_only_audited_raw_command_pointers(self):
        messages = problems(scan(DRIVER), AUDITED)
        self.assertEqual(messages, [], "\n".join(messages))


if __name__ == "__main__":
    unittest.main(argv=sys.argv[:1])
