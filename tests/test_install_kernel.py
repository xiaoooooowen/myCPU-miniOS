import importlib.util
import pathlib
import struct
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


MKFS = load_module("mkfs_minifs", ROOT / "tools" / "mkfs_minifs.py")
INSTALL = load_module(
    "install_kernel", ROOT / "tools" / "install_kernel.py"
)


class InstallKernelTest(unittest.TestCase):
    def make_paths(self, directory, kernel=b"kernel payload"):
        image_path = pathlib.Path(directory) / "disk.img"
        kernel_path = pathlib.Path(directory) / "kernel.bin"
        image_path.write_bytes(MKFS.MiniFSBuilder().finish())
        kernel_path.write_bytes(kernel)
        return image_path, kernel_path

    def test_installs_header_payload_and_checksum(self):
        with tempfile.TemporaryDirectory() as directory:
            kernel = bytes(range(256)) * 3
            image_path, kernel_path = self.make_paths(directory, kernel)
            prefix_before = image_path.read_bytes()[
                :INSTALL.KERNEL_SLOT_START * INSTALL.BLOCK_SIZE
            ]

            INSTALL.install_kernel(image_path, kernel_path)
            image = image_path.read_bytes()
            slot = INSTALL.KERNEL_SLOT_START * INSTALL.BLOCK_SIZE
            fields = INSTALL.HEADER.unpack_from(image, slot)

            self.assertEqual(fields[0], INSTALL.KERNEL_MAGIC)
            self.assertEqual(fields[1], INSTALL.KERNEL_VERSION)
            self.assertEqual(fields[2], INSTALL.HEADER.size)
            self.assertEqual(fields[3], INSTALL.KERNEL_LOAD_ADDRESS)
            self.assertEqual(fields[4], INSTALL.KERNEL_LOAD_ADDRESS)
            self.assertEqual(fields[5], len(kernel))
            self.assertEqual(fields[6], INSTALL.checksum32(kernel))
            self.assertEqual(
                image[slot + INSTALL.BLOCK_SIZE:
                      slot + INSTALL.BLOCK_SIZE + len(kernel)],
                kernel,
            )
            self.assertEqual(
                image[:INSTALL.KERNEL_SLOT_START * INSTALL.BLOCK_SIZE],
                prefix_before,
            )

    def test_migrates_legacy_minifs_when_tail_is_unused(self):
        with tempfile.TemporaryDirectory() as directory:
            image_path, kernel_path = self.make_paths(directory)
            image = bytearray(image_path.read_bytes())
            struct.pack_into("<I", image, 8, INSTALL.DEVICE_BLOCK_COUNT)
            for block in range(
                INSTALL.KERNEL_SLOT_START, INSTALL.DEVICE_BLOCK_COUNT
            ):
                offset = (INSTALL.DATA_BITMAP_START * INSTALL.BLOCK_SIZE +
                          block // 8)
                image[offset] &= ~(1 << (block % 8))
            image_path.write_bytes(image)

            INSTALL.install_kernel(image_path, kernel_path)
            migrated = bytearray(image_path.read_bytes())
            self.assertEqual(struct.unpack_from("<I", migrated, 8)[0],
                             INSTALL.MINIFS_BLOCK_COUNT)
            self.assertTrue(all(
                INSTALL.bit_is_set(migrated, block)
                for block in range(
                    INSTALL.KERNEL_SLOT_START, INSTALL.DEVICE_BLOCK_COUNT
                )
            ))

    def test_rejects_legacy_image_using_reserved_tail(self):
        with tempfile.TemporaryDirectory() as directory:
            image_path, kernel_path = self.make_paths(directory)
            image = bytearray(image_path.read_bytes())
            struct.pack_into("<I", image, 8, INSTALL.DEVICE_BLOCK_COUNT)
            for block in range(
                INSTALL.KERNEL_SLOT_START, INSTALL.DEVICE_BLOCK_COUNT
            ):
                offset = (INSTALL.DATA_BITMAP_START * INSTALL.BLOCK_SIZE +
                          block // 8)
                image[offset] &= ~(1 << (block % 8))
            INSTALL.set_bit(image, INSTALL.KERNEL_SLOT_START + 7)
            image_path.write_bytes(image)
            with self.assertRaisesRegex(ValueError, "uses tail sectors"):
                INSTALL.install_kernel(image_path, kernel_path)

    def test_rejects_oversized_kernel(self):
        with tempfile.TemporaryDirectory() as directory:
            limit = ((INSTALL.KERNEL_SLOT_SECTORS - 1) *
                     INSTALL.BLOCK_SIZE)
            image_path, kernel_path = self.make_paths(
                directory, b"x" * (limit + 1)
            )
            with self.assertRaisesRegex(ValueError, "too large"):
                INSTALL.install_kernel(image_path, kernel_path)


if __name__ == "__main__":
    unittest.main()
