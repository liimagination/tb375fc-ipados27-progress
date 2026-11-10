/*
  Lenovo TB375FC (MediaTek MT6897) BootShim.

  Why this file exists at all
  ---------------------------
  scripts/unixnu/bootimg.py compiles <uefi>/BootShim/BootShim.S with

      clang --target=aarch64-none-elf -c \
            -DFD_BASE=<base> -DFD_SIZE=<size> -DREQUIRES_KERNEL_HEADER=<0|1>

  then appends the firmware volume to it, gzips the pair, and writes the
  result as the kernel field of an Android boot v4 image.

  On this board LK decompresses that kernel field to kernel_addr 0x40000000
  (the stock vendor_boot.img header, measured: kernel_addr 0x40000000,
  ramdisk_addr 0x66f00000, tags_addr 0x47c80000) and branches to its first
  byte. So the firmware volume lands at FD_BASE + sizeof(shim), not at
  FD_BASE, and the shim has to move it down by sizeof(shim).

  The upstream Mu-Silicium shim does that copy in place, which overwrites the
  instructions it is still executing: the copy runs backwards, and its final
  iterations write over _Start/_CopyLoop themselves. That is only harmless by
  luck of instruction-cache behaviour. This shim first relocates the copy
  routine above the whole decompressed image, publishes it, and only then
  calls it - the same structure that is proven on the Lenovo TB365FC
  (UniXNU docs/zh/devices/tb365fc.md, same vendor, same MediaTek LK, Plan A:
  FD as the gzip kernel of a v4 image).

  Register contract
  -----------------
  Entered at _Head with nothing but the stack LK leaves behind; it exits by
  branching to FD_BASE, where the SEC module entry point of the firmware
  volume runs.

  SPDX-License-Identifier: BSD-2-Clause-Patent
*/

_Head:
  adr   x1, _Payload            /* x1 = source of the firmware volume          */
  b     _Start

  /*
    Staging area for the relocated copy loop: above the whole decompressed
    image, i.e. above FD_BASE + FD_SIZE + sizeof(shim). 0x70000 of slack keeps
    it clear of the image once the shim grows. This address is used only
    during the hand-off, before SEC initialises the heap that later lives
    here (see MemoryMapLib.c, "DXE Heap" starts at FD end + stack).
  */
.equ STAGE_ADDR, FD_BASE + FD_SIZE + 0x70000

#if REQUIRES_KERNEL_HEADER == 1
  /*
    ARM64 image header. bootimg.py checks both fields:
      offset 8  == FD_BASE                 (text_offset / load address)
      offset 16 == sizeof(shim) + FD_SIZE  (image_size)
    and requires "ARM\x64" at offset 56.
  */
  .quad FD_BASE                     /* Text Offset                          */
  .quad FD_SIZE + (_Payload - _Head)/* Size of shim plus firmware volume    */
  .quad 0                           /* Flags                                */
  .quad 0                           /* Reserved                             */
  .quad 0                           /* Reserved                             */
  .quad 0                           /* Reserved                             */
  .ascii "ARM\x64"                  /* ARM64 Magic                          */
  .long 0                           /* Reserved                             */
#endif

_Start:
  mov   x4, x1                      /* x4 = payload source                  */
  ldr   x5, =FD_BASE
  cmp   x4, x5
  beq   _Entry                      /* already in place: just enter         */

  /* Relocate the copy/clean block out of both the source and the destination. */
  adr   x2, _CopyLoop
  ldr   x3, =STAGE_ADDR
  ldr   x6, =(_RelocEnd - _CopyLoop)
  mov   x9, x6
_RelocLoop:
  ldp   x10, x11, [x2], #16
  stp   x10, x11, [x3], #16
  subs  x9, x9, #16
  b.ne  _RelocLoop

  /* Publish the relocated instructions, then enter the copy loop. */
  ldr   x8, =STAGE_ADDR
  ldr   x7, =(_RelocEnd - _CopyLoop)
  add   x7, x8, x7
  mrs   x9, ctr_el0
  ubfx  x9, x9, #16, #4
  mov   x10, #4
  lsl   x9, x10, x9
  sub   x10, x9, #1
  bic   x8, x8, x10
_RelocCleanLoop:
  dc    cvac, x8
  add   x8, x8, x9
  cmp   x8, x7
  b.lo  _RelocCleanLoop
  dsb   sy
  ic    iallu
  dsb   sy
  isb

  ldr   x7, =STAGE_ADDR
  mov   x4, x1
  ldr   x5, =FD_BASE
  ldr   x6, =FD_SIZE
  br    x7

.balign 16
_CopyLoop:
  /*
    Destination is FD_BASE, source is FD_BASE + sizeof(shim): the source is
    ABOVE the destination, so the copy must run FORWARD (ascending). A forward
    copy writes strictly below the read pointer and can therefore never touch
    bytes it has not read yet.

    An earlier revision copied backwards (ldp/stp with #-16). That is the rule
    for dst > src, and with dst < src it destroys the payload before reading
    it: the first store lands at fd_base + size - 16, which is still inside the
    unread source window, so the last sizeof(shim) bytes of the payload are
    clobbered. Proven on the unit on 2026-11-07 - a 416-byte probe that runs
    correctly on its own never executed behind this shim, because 336 of its
    416 bytes were overwritten before the branch.
  */
  mov   x4, x1
  ldr   x5, =FD_BASE
  ldr   x6, =FD_SIZE
_CopyChunk:
  ldp   x2, x3, [x4], #16
  stp   x2, x3, [x5], #16
  subs  x6, x6, #16
  b.ne  _CopyChunk
  ldr   x5, =FD_BASE

_Entry:
  /* Publish the firmware volume before executing it. */
  dsb   sy
  mrs   x9, ctr_el0
  ubfx  x9, x9, #16, #4
  mov   x10, #4
  lsl   x9, x10, x9
  sub   x10, x9, #1
  bic   x8, x5, x10
  ldr   x7, =FD_SIZE
  add   x7, x5, x7
_CleanLoop:
  dc    cvac, x8
  add   x8, x8, x9
  cmp   x8, x7
  b.lo  _CleanLoop
  dsb   sy
  ic    iallu
  dsb   sy
  isb
  br    x5

_Dead:
  b     _Dead

.text
.ltorg
.balign 16
_RelocEnd:
.balign 16

_Payload:
