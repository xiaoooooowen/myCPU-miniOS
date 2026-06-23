#!/usr/bin/env python3
"""Create an 8 MiB MiniFS v2 image from a staging directory."""

from __future__ import annotations

import argparse
import os
import pathlib
import struct
import sys

BLOCK_SIZE = 512
DEVICE_BLOCK_COUNT = 16384
BLOCK_COUNT = 15360
IMAGE_SIZE = BLOCK_SIZE * DEVICE_BLOCK_COUNT
MAGIC = 0x4D465332
VERSION = 2
MAX_INODES = 256
MAX_FILE_SIZE = 64 * 1024
NAME_MAX = 55

INODE_BITMAP_START = 1
DATA_BITMAP_START = 2
DATA_BITMAP_BLOCKS = 4
INODE_TABLE_START = 6
DATA_START = 38

MODE_FILE = 1
MODE_DIR = 2
MODE_EXEC = 0x100
DIRECT_COUNT = 10

SUPER = struct.Struct("<12I464s")
INODE = struct.Struct("<4I10I2I")
DIRENT = struct.Struct("<2I56s")


class MiniFSBuilder:
    def __init__(self) -> None:
        self.image = bytearray(IMAGE_SIZE)
        self.inode_used = [False] * MAX_INODES
        self.block_used = [False] * DEVICE_BLOCK_COUNT
        self.inodes = [None] * MAX_INODES
        for block in range(DATA_START):
            self.block_used[block] = True
        self.inode_used[0] = True
        for block in range(BLOCK_COUNT, DEVICE_BLOCK_COUNT):
            self.block_used[block] = True
        self.inodes[0] = self._new_inode(MODE_DIR, 0)

    @staticmethod
    def _new_inode(mode: int, parent: int) -> dict:
        return {
            "mode": mode,
            "size": 0,
            "parent": parent,
            "links": 1,
            "direct": [0] * DIRECT_COUNT,
            "indirect": 0,
        }

    def alloc_inode(self, mode: int, parent: int) -> int:
        for number in range(1, MAX_INODES):
            if not self.inode_used[number]:
                self.inode_used[number] = True
                self.inodes[number] = self._new_inode(mode, parent)
                return number
        raise ValueError("MiniFS inode table is full")

    def alloc_block(self) -> int:
        for block in range(DATA_START, BLOCK_COUNT):
            if not self.block_used[block]:
                self.block_used[block] = True
                return block
        raise ValueError("MiniFS data area is full")

    def file_block(self, inode: dict, logical: int, allocate: bool) -> int:
        if logical < DIRECT_COUNT:
            if inode["direct"][logical] == 0 and allocate:
                inode["direct"][logical] = self.alloc_block()
            return inode["direct"][logical]
        slot = logical - DIRECT_COUNT
        if slot >= 128:
            raise ValueError("file exceeds MiniFS block mapping")
        if inode["indirect"] == 0:
            if not allocate:
                return 0
            inode["indirect"] = self.alloc_block()
        offset = inode["indirect"] * BLOCK_SIZE + slot * 4
        block = struct.unpack_from("<I", self.image, offset)[0]
        if block == 0 and allocate:
            block = self.alloc_block()
            struct.pack_into("<I", self.image, offset, block)
        return block

    def write_file(self, inode_number: int, data: bytes) -> None:
        if len(data) > MAX_FILE_SIZE:
            raise ValueError(f"file exceeds 64 KiB: inode {inode_number}")
        inode = self.inodes[inode_number]
        for offset in range(0, len(data), BLOCK_SIZE):
            logical = offset // BLOCK_SIZE
            block = self.file_block(inode, logical, True)
            chunk = data[offset : offset + BLOCK_SIZE]
            start = block * BLOCK_SIZE
            self.image[start : start + len(chunk)] = chunk
        inode["size"] = len(data)

    def add_dirent(self, parent_number: int, name: str, child: int,
                   mode: int) -> None:
        encoded = name.encode("utf-8")
        if not encoded or len(encoded) > NAME_MAX or b"/" in encoded:
            raise ValueError(f"invalid MiniFS name: {name!r}")
        parent = self.inodes[parent_number]
        index = parent["size"] // DIRENT.size
        block = self.file_block(parent, index // 8, True)
        offset = block * BLOCK_SIZE + (index % 8) * DIRENT.size
        DIRENT.pack_into(self.image, offset, child, mode,
                         encoded + b"\0" * (56 - len(encoded)))
        parent["size"] += DIRENT.size

    def import_tree(self, root: pathlib.Path, parent_number: int = 0) -> None:
        for path in sorted(root.iterdir(), key=lambda item: item.name):
            if path.is_symlink():
                raise ValueError(f"symbolic links are not supported: {path}")
            if path.is_dir():
                child = self.alloc_inode(MODE_DIR, parent_number)
                self.add_dirent(parent_number, path.name, child, MODE_DIR)
                self.import_tree(path, child)
                continue
            if not path.is_file():
                raise ValueError(f"unsupported file type: {path}")
            mode = MODE_FILE
            if os.access(path, os.X_OK):
                mode |= MODE_EXEC
            child = self.alloc_inode(mode, parent_number)
            self.add_dirent(parent_number, path.name, child, mode)
            self.write_file(child, path.read_bytes())

    def finish(self) -> bytes:
        SUPER.pack_into(
            self.image,
            0,
            MAGIC,
            VERSION,
            BLOCK_COUNT,
            MAX_INODES,
            INODE_BITMAP_START,
            1,
            DATA_BITMAP_START,
            DATA_BITMAP_BLOCKS,
            INODE_TABLE_START,
            32,
            DATA_START,
            MAX_FILE_SIZE,
            b"\0" * 464,
        )
        for number, used in enumerate(self.inode_used):
            if used:
                self.image[INODE_BITMAP_START * BLOCK_SIZE + number // 8] |= (
                    1 << (number % 8)
                )
        bitmap_start = DATA_BITMAP_START * BLOCK_SIZE
        for number, used in enumerate(self.block_used):
            if used:
                self.image[bitmap_start + number // 8] |= 1 << (number % 8)
        for number, inode in enumerate(self.inodes):
            if inode is None:
                continue
            offset = INODE_TABLE_START * BLOCK_SIZE + number * INODE.size
            INODE.pack_into(
                self.image,
                offset,
                inode["mode"],
                inode["size"],
                inode["parent"],
                inode["links"],
                *inode["direct"],
                inode["indirect"],
                0,
            )
        return bytes(self.image)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=pathlib.Path,
                        help="staging root directory to import")
    parser.add_argument("image", type=pathlib.Path,
                        help="output MiniFS disk image")
    parser.add_argument("--force", action="store_true",
                        help="replace an existing image")
    args = parser.parse_args()

    if not args.root.is_dir():
        parser.error(f"staging root does not exist: {args.root}")
    if args.image.exists() and not args.force:
        print(f"refusing to overwrite existing image: {args.image}",
              file=sys.stderr)
        return 1

    builder = MiniFSBuilder()
    builder.import_tree(args.root)
    image = builder.finish()
    args.image.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.image.with_suffix(args.image.suffix + ".tmp")
    temporary.write_bytes(image)
    temporary.replace(args.image)
    print(f"created {args.image} ({len(image)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
