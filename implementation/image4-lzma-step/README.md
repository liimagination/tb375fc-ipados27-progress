# TB375FC Image4 LZMA payload test

This is a host-built, not yet flashed boot candidate. It carries the 128 MiB FAT volume as an 18.6 MB LZMA-Alone RAW section. `UniXNURamDiskDxe` decompresses it during DXE and publishes a read-only Block I/O device after validating the FAT volume.

First device check is limited to reaching the UniXNU menu and confirming the payload/config entry appears. Only after that succeeds should the kernel Image4 entry be tried. This is not an iPadOS installation image and has no root/Data volume.

`boot-image4-test.img` SHA256: `f5b8b6a9a36afa19f31e95166d9e6b008641bdbd8a3f2b6be9b18454368d2c41`.
`rollback-unixnu-previous.img` restores the previous UniXNU test state, not stock Android.
