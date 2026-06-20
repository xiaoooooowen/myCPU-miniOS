#!/usr/bin/env python3
"""Install a checksummed MiniOS kernel image into the disk boot slot."""

from __future__ import annotations

import argparse
import pathlib
import struct
import sys

BLOCK_SIZE = 512
DEVICE_BLOCK_COUNT = 16384
IMAGE_SIZE = BLOCK_SIZE * DEVICE_BLOCK_COUNT
MINIFS_MAGIC = 0x4D465332
MINIFS_VERSION = 2
MINIFS_BLOCK_COUNT = 15360
KERNEL_SLOT_START = MINIFS_BLOCK_COUNT
KERNEL_SLOT_SECTORS = DEVICE_BLOCK_COUNT - KERNEL_SLOT_START
KERNEL_LOAD_ADDRESS = 0x80200000
KERNEL_MAGIC = b"MINIKRNL"
KERNEL_VERSION = 1

SUPER_PREFIX = struct.Struct("<12I")
HEADER = struct.Struct("<8sIIQQII24s")
DATA_BITMAP_START = 2


def checksum32(data: bytes) -> int:
    return sum(data) & 0xFFFFFFFF


def bit_is_set(image: bytearray, bit: int) -> bool:
    offset = DATA_BITMAP_START * BLOCK_SIZE + bit // 8
    return (image[offset] & (1 << (bit % 8))) != 0


def set_bit(image: bytearray, bit: int) -> None:
    offset = DATA_BITMAP_START * BLOCK_SIZE + bit // 8
    image[offset] |= 1 << (bit % 8)


def migrate_minifs(image: bytearray) -> None:
    values = list(SUPER_PREFIX.unpack_from(image))
    magic, version, blocks = values[:3]
    if magic != MINIFS_MAGIC or version != MINIFS_VERSION:
        raise ValueError("disk does not contain MiniFS v2")
    if blocks == MINIFS_BLOCK_COUNT:
        return
    if blocks != DEVICE_BLOCK_COUNT:
        raise ValueError(f"unsupported MiniFS block count: {blocks}")
    used = [
        block for block in range(KERNEL_SLOT_START, DEVICE_BLOCK_COUNT)
        if bit_is_set(image, block)
    ]
    if used:
        raise ValueError(
            "cannot reserve kernel slot: MiniFS uses tail sectors "
            f"(first used sector {used[0]})"
        )
    values[2] = MINIFS_BLOCK_COUNT
    SUPER_PREFIX.pack_into(image, 0, *values)
    for block in range(KERNEL_SLOT_START, DEVICE_BLOCK_COUNT):
        set_bit(image, block)


def install_kernel(image_path: pathlib.Path,
                   kernel_path: pathlib.Path) -> None:
    image = bytearray(image_path.read_bytes())
    if len(image) != IMAGE_SIZE:
        raise ValueError("disk image must be exactly 8 MiB")
    kernel = kernel_path.read_bytes()
    payload_limit = (KERNEL_SLOT_SECTORS - 1) * BLOCK_SIZE
    if not kernel:
        raise ValueError("kernel image is empty")
    if len(kernel) > payload_limit:
        raise ValueError(
            f"kernel is too large: {len(kernel)} bytes, limit {payload_limit}"
        )

    migrate_minifs(image)
    slot_offset = KERNEL_SLOT_START * BLOCK_SIZE
    slot_size = KERNEL_SLOT_SECTORS * BLOCK_SIZE
    image[slot_offset:slot_offset + slot_size] = b"\0" * slot_size
    header = HEADER.pack(
        KERNEL_MAGIC,
        KERNEL_VERSION,
        HEADER.size,
        KERNEL_LOAD_ADDRESS,
        KERNEL_LOAD_ADDRESS,
        len(kernel),
        checksum32(kernel),
        b"\0" * 24,
    )
    image[slot_offset:slot_offset + len(header)] = header
    payload_offset = slot_offset + BLOCK_SIZE
    image[payload_offset:payload_offset + len(kernel)] = kernel

    temporary = image_path.with_suffix(image_path.suffix + ".tmp")
    temporary.write_bytes(image)
    temporary.replace(image_path)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=pathlib.Path)
    parser.add_argument("kernel", type=pathlib.Path)
    args = parser.parse_args()
    try:
        install_kernel(args.image, args.kernel)
    except (OSError, ValueError) as error:
        print(f"install_kernel: {error}", file=sys.stderr)
        return 1
    print(
        f"installed {args.kernel} at disk sector {KERNEL_SLOT_START} "
        f"for load address 0x{KERNEL_LOAD_ADDRESS:x}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
