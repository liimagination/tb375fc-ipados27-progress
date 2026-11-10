# Contents and provenance

- `docs/HANDOFF-2026-11-08-COMPLETE.md`: consolidated handoff written from current local artifacts and historical device records.
- `docs/{task_plan,findings,progress}.md`: working notes copied from the workspace; consult COMPLETE first where status conflicts.
- `device/source/`: text-only TB375FC device config, DTS, scripts, and documentation. Local `local.toml`, raw FDT/DTB files, and output directories are excluded.
- `uefi-tb375fc/`: text-only platform source/config snapshot. Reference dumps and generated DTBs are excluded.
- `implementation/image4-lzma-step/`: DXE source/config snapshot and qualification. No boot image, FD, FAT, kernel, or LZMA payload is included.
- `evidence/`: JSON results selected from the test folders and the stock boot_a restore record.
- `source-diffs/`: captured uncommitted changes. Apply only after reviewing base commit and patch contents; these were not pushed to UniXNU upstream.

This repository was assembled as a curated snapshot, not as a replacement for the upstream UniXNU repository. Current GitHub remote is intentionally unset until a private destination is selected and authenticated.
