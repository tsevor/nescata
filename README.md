# nescata
---

usage: `nescata rom.nes`

or in command mode: `loadrom rom.nes`

press h for keybinds

in command mode, type help to see commands

---
## progress

- mappers
  - NROM (0)
  - MMC1 (1)
  - AxROM (7)

---
## todo

- fix scroll
  - https://www.nesdev.org/wiki/PPU_scrolling
  - https://forums.nesdev.org/viewtopic.php?t=21527
- add more mappers
  - mmc3
    - https://www.nesdev.org/wiki/MMC3
    - https://forums.nesdev.org/viewtopic.php?t=16781
- implement the apu
  - https://www.nesdev.org/wiki/APU



## list of ROMs that work decently

criteria:
  - uses mapper 0, 1, or 7
  - doesn't use advanced scroll techniques (it's implemented wrong)

good ones that work:
  - pacman
  - tetris
  - donkey kong
  - mario bros
  - super mario bros
  - solstice
  - micro mages
