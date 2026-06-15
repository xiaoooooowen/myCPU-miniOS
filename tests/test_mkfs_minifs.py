import importlib.util
import pathlib
import stat
import struct
import subprocess
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "mkfs_minifs", ROOT / "tools" / "mkfs_minifs.py"
)
MKFS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MKFS)


class MkfsMiniFSTest(unittest.TestCase):
    def test_builds_v2_image_and_imports_tree(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory) / "root"
            (root / "bin").mkdir(parents=True)
            program = root / "bin" / "hello"
            program.write_bytes(b"ELF")
            program.chmod(program.stat().st_mode | stat.S_IXUSR)

            builder = MKFS.MiniFSBuilder()
            builder.import_tree(root)
            image = builder.finish()

            self.assertEqual(len(image), 8 * 1024 * 1024)
            magic, version, blocks, inodes = struct.unpack_from("<4I", image)
            self.assertEqual(magic, MKFS.MAGIC)
            self.assertEqual(version, 2)
            self.assertEqual(blocks, 16384)
            self.assertEqual(inodes, 256)
            self.assertGreater(sum(builder.inode_used), 2)

    def test_rejects_file_larger_than_64_kib(self):
        builder = MKFS.MiniFSBuilder()
        inode = builder.alloc_inode(MKFS.MODE_FILE, 0)
        builder.write_file(inode, b"x" * (64 * 1024))
        self.assertNotEqual(builder.inodes[inode]["indirect"], 0)
        with self.assertRaises(ValueError):
            builder.write_file(inode, b"x" * (64 * 1024 + 1))

    def test_imports_more_than_128_files_and_nested_directories(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory) / "root"
            nested = root / "one" / "two" / "three"
            nested.mkdir(parents=True)
            for index in range(130):
                (nested / f"file-{index:03d}").write_text("ok")

            builder = MKFS.MiniFSBuilder()
            builder.import_tree(root)
            self.assertGreaterEqual(sum(builder.inode_used), 134)

    def test_cli_refuses_to_overwrite_without_force(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory) / "root"
            image = pathlib.Path(directory) / "disk.img"
            root.mkdir()
            image.write_bytes(b"existing")
            result = subprocess.run(
                [sys.executable, str(ROOT / "tools" / "mkfs_minifs.py"),
                 str(root), str(image)],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(image.read_bytes(), b"existing")


if __name__ == "__main__":
    unittest.main()
