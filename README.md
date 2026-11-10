# TB375FC UniXNU / iPadOS 27 porting progress (private working snapshot)

This repository preserves the TB375FC porting notes, source snapshots, test evidence, and next steps for a private handoff. It is **not** an iPadOS image and does not claim that iPadOS 27 boots on the tablet.

## Current verified state (2026-11-10)

- An earlier no-payload boot image reached the UniXNU menu on the TB375FC.
- BSP EL1 PAC key access and PACIA/AUTIA were observed to pass. Other CPU cores and EL0 are not covered by those tests.
- A 95,125,504-byte Image4/corecrypto minimal kernel candidate passed host prelink/parser checks. Previous hardware reached `trust_cache_runtime_init` and reported `image4 interface not available`; current closure has not been retested on hardware.
- A new compressed-FAT Image4 boot candidate passed host build/package checks but has not been flashed.
- At the user's request on 2026-11-08, the stock 64 MiB `boot_a` image was restored successfully and fastboot reboot returned OKAY. Android userspace was not observed after reboot.
- The tablet was reported out of battery before the restore request; recheck its live state before any later test.
- iPadOS 27 UI, root volume, Data volume, UFS bring-up, and complete arm64e qualification remain unfinished. See the full handoff.

## Start here

1. `docs/HANDOFF-2026-11-08-COMPLETE.md` — detailed state, evidence, chronology, recovery boundaries, and next steps.
2. `docs/task_plan.md`, `docs/findings.md`, `docs/progress.md` — working notes. These contain older append-only entries; the COMPLETE handoff supersedes stale status claims.
3. `implementation/image4-lzma-step/` — source/config snapshot and host qualification for the newest unflashed Image4 candidate.
4. `evidence/` — machine-readable results for significant device tests and restore.
5. `source-diffs/` — snapshots of uncommitted changes in the separate UniXNU checkout and its XnuPrelink submodule. Base commits are recorded next to the patches.

## Privacy and artifacts

This snapshot is intended for a **private GitHub repository**. It excludes boot/firmware images, original vendor firmware, kernel binaries, raw DTBs, build logs, local-only paths/credentials, and `tb375fc-port/private/`. The source notes still contain device-specific technical information and should stay private unless reviewed and scrubbed before publication.

## Reproduce / continue

Use the WSL build paths and commands in the complete handoff. The latest candidate has not had an on-device test. Start with the payload discovery stage, then XNU entry, preserving the one-stage-at-a-time method documented there.
