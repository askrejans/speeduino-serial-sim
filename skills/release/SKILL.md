---
name: release
description: Prepare a speeduino-serial-sim release — version, changelog, platform and library updates, all environments built, firmware binaries attached.
---

# Release

1. Update platforms and libraries in `platformio.ini` (`pio pkg show <name>`
   lists versions; the registry list is not sorted by version). Build every
   environment: `pio run`, plus `pio run -e <each env>`.
2. `pio test -e native`.
3. `FIRMWARE_VERSION` in `include/Config.h`, `CHANGELOG.md`.
4. Hardware check (hardware-validation skill) or say it was skipped.
5. Binaries for the GitHub release, per ESP environment:
   `.pio/build/<env>/firmware.bin` plus, for a full flash image,
   `bootloader.bin` and `partitions.bin` (or a merged image via
   `esptool merge-bin`); AVR: `firmware.hex`. Name them
   `speeduino-serial-sim-<version>-<env>.<ext>` with a SHA256SUMS file.
6. Licence stays MIT (other contributors).
