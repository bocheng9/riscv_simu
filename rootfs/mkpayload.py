#!/usr/bin/env python3
"""
mkpayload.py — rebuild images/fw_payload.bin from a freshly built rootfs

The simulator boots an OpenSBI + Linux image whose initramfs is embedded at a
fixed offset. Two constraints came out of testing and matter for any rebuild:

  1. The new gzip stream must not be longer than the original one. The payload
     is loaded as a flat image and anything after the initramfs shifts.
  2. The stream must be plain gzip. The in-kernel decompressor used here does
     not accept the FEXTRA field, so padding has to go after the stream, not
     inside it.

Usage:
    mkpayload.py <base payload> <rootfs cpio.gz> <output payload>

Typical use is to keep the OpenSBI + kernel part of the committed payload and
replace only the initramfs:

    python3 rootfs/mkpayload.py images/fw_payload.bin \\
            output/images/rootfs.cpio.gz images/fw_payload.new.bin
"""

import gzip
import io
import sys
import zlib

# Offset of the embedded initramfs inside the committed payload.
INITRAMFS_OFFSET = 0x1006038


def find_initramfs(data):
    """Locate the gzip stream that decompresses to a cpio archive."""
    pos = 0
    while True:
        i = data.find(b'\x1f\x8b\x08', pos)
        if i < 0:
            return None
        try:
            dec = zlib.decompressobj(16 + zlib.MAX_WBITS)
            out = dec.decompress(data[i:])
            if len(out) > 100_000 and out[:6] == b'070701':
                consumed = len(data) - i - len(dec.unused_data)
                return i, consumed
        except zlib.error:
            pass
        pos = i + 3


def prune_shared_libs(cpio):
    """Drop lib/, lib32/ and usr/lib/ from the cpio.

    busybox is linked statically, so none of the shared objects, the dynamic
    loader or the other C library files are needed. They account for most of
    the archive and without this step the rootfs is too large to fit the
    initramfs slot in the payload.
    """
    out = bytearray()
    off = 0
    dropped = 0
    while off < len(cpio) - 110:
        if cpio[off:off + 6] != b'070701':
            out += cpio[off:]
            break
        filesize = int(cpio[off + 54:off + 62], 16)
        namesize = int(cpio[off + 94:off + 102], 16)
        name = cpio[off + 110:off + 110 + namesize - 1].decode(errors='replace')
        header_end = (off + 110 + namesize + 3) & ~3
        entry_end = (header_end + filesize + 3) & ~3

        if name.startswith(('lib/', 'lib32/', 'usr/lib/')):
            dropped += 1
        else:
            out += cpio[off:entry_end]

        if name == 'TRAILER!!!':
            break
        off = entry_end

    if dropped:
        print(f'pruned        : {dropped} shared library entries')
    return bytes(out)


# Character devices the kernel needs before it can open an initial console.
# Buildroot leaves dev/ empty because devtmpfs normally populates it at boot,
# but the kernel opens /dev/console before that mount happens.
DEV_NODES = {
    'dev/console': (5, 1, 0o600),
    'dev/null':    (1, 3, 0o666),
    'dev/tty':     (5, 0, 0o666),
    'dev/zero':    (1, 5, 0o666),
}


def patch_dev_nodes(cpio):
    """Drop any existing entries for these devices and re-add them as proper
    character devices."""
    out = bytearray()
    off = 0
    inserted = False
    removed = 0
    found_dev_dir = False

    def emit(name, mode, rdevmajor, rdevminor):
        ns = len(name) + 1
        header = b'070701'
        # newc field order: ino, mode, uid, gid, nlink, mtime, filesize,
        # devmajor, devminor, rdevmajor, rdevminor, namesize, check
        for v in (0, mode, 0, 0, 1, 0, 0, 0, 0, rdevmajor, rdevminor, ns, 0):
            header += b'%08X' % v
        block = header + name.encode() + b'\x00'
        block += b'\x00' * ((4 - len(block) % 4) % 4)
        return block

    while off < len(cpio) - 110:
        if cpio[off:off + 6] != b'070701':
            out += cpio[off:]
            break
        filesize = int(cpio[off + 54:off + 62], 16)
        namesize = int(cpio[off + 94:off + 102], 16)
        name = cpio[off + 110:off + 110 + namesize - 1].decode(errors='replace')
        header_end = (off + 110 + namesize + 3) & ~3
        entry_end = (header_end + filesize + 3) & ~3

        if name in DEV_NODES:
            removed += 1
        else:
            out += cpio[off:entry_end]
            if name == 'dev':
                found_dev_dir = True

        if name == 'dev' and not inserted:
            for dev, (maj, mn, perm) in DEV_NODES.items():
                out += emit(dev, 0o020000 | perm, maj, mn)
            inserted = True

        if name == 'TRAILER!!!':
            break
        off = entry_end

    if inserted:
        print(f'dev nodes     : {len(DEV_NODES)} added ({removed} stale removed)')
    else:
        print('warning: no dev/ directory found; console nodes not added')
    return bytes(out)


def link_init_to_busybox(cpio):
    """Turn /init into a cpio hard link of bin/busybox.

    Buildroot installs /init as a symlink to sbin/init, which in turn points
    at ../bin/busybox. Resolving that chain costs a second copy of the binary
    in the archive; a hard link shares the inode and keeps the rootfs inside
    the slot. The kernel resolves cpio hard links by inode, so this also gives
    it the file contents directly.
    """
    entries = []
    off = 0
    while off < len(cpio) - 110:
        if cpio[off:off + 6] != b'070701':
            break
        fields = [int(cpio[off + 6 + 8 * i:off + 14 + 8 * i], 16) for i in range(13)]
        ino, mode, filesize, namesize = fields[0], fields[1], fields[6], fields[11]
        rdevmajor, rdevminor = fields[9], fields[10]
        name = cpio[off + 110:off + 110 + namesize - 1].decode(errors='replace')
        header_end = (off + 110 + namesize + 3) & ~3
        entry_end = (header_end + filesize + 3) & ~3
        entries.append([name, ino, mode,
                        bytes(cpio[header_end:header_end + filesize]),
                        rdevmajor, rdevminor])
        if name == 'TRAILER!!!':
            break
        off = entry_end

    busybox_ino = None
    for name, ino, _mode, _data, _rmaj, _rmin in entries:
        if name == 'bin/busybox':
            busybox_ino = ino
            break
    if busybox_ino is None:
        return cpio, False

    out = bytearray()
    patched = False
    for name, ino, mode, data, rdevmajor, rdevminor in entries:
        nlink = 1
        if name == 'init' and (mode & 0o170000) == 0o120000:
            ino, mode, data, nlink = busybox_ino, 0o100755, b'', 2
            rdevmajor = rdevminor = 0
            patched = True
        elif name == 'bin/busybox':
            nlink = 2       # shares its inode with /init

        ns = len(name) + 1
        header = b'070701'
        # newc field order: ino, mode, uid, gid, nlink, mtime, filesize,
        # devmajor, devminor, rdevmajor, rdevminor, namesize, check
        for v in (ino, mode, 0, 0, nlink, 0, len(data), 0, 0,
                  rdevmajor, rdevminor, ns, 0):
            header += b'%08X' % v
        out += header + name.encode() + b'\x00'
        out += b'\x00' * ((4 - len(out) % 4) % 4)
        out += data
        out += b'\x00' * ((4 - len(out) % 4) % 4)

    if patched:
        print('init          : hard link to bin/busybox')
    return bytes(out), patched


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2

    base_path, rootfs_path, out_path = sys.argv[1:4]

    base = open(base_path, 'rb').read()
    found = find_initramfs(base)
    if found is None:
        print('error: no initramfs found in the base payload')
        return 1

    offset, capacity = found
    if offset != INITRAMFS_OFFSET:
        print(f'note: initramfs sits at {offset:#x}, not the expected '
              f'{INITRAMFS_OFFSET:#x}')

    # The build produces a .gz; recompress so the stream is plain gzip with a
    # zero mtime and no FEXTRA field.
    raw = gzip.decompress(open(rootfs_path, 'rb').read())
    raw = prune_shared_libs(raw)
    raw = patch_dev_nodes(raw)
    raw, _ = link_init_to_busybox(raw)
    buf = io.BytesIO()
    with gzip.GzipFile(fileobj=buf, mode='wb', compresslevel=9, mtime=0) as f:
        f.write(raw)
    stream = buf.getvalue()

    print(f'rootfs cpio   : {len(raw):,} bytes')
    print(f'gzip stream   : {len(stream):,} bytes')
    print(f'capacity      : {capacity:,} bytes')
    print(f'headroom      : {capacity - len(stream):+,} bytes')

    if len(stream) > capacity:
        print('error: the new initramfs does not fit; shrink the rootfs')
        return 1
    blob = stream + b'\x00' * (capacity - len(stream))
    if zlib.decompressobj(16 + zlib.MAX_WBITS).decompress(blob) != raw:
        print('error: round-trip check failed')
        return 1

    out = bytearray(base)
    out[offset:offset + capacity] = blob
    open(out_path, 'wb').write(bytes(out))
    print(f'wrote {out_path}: {len(out):,} bytes (same length as the base)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
