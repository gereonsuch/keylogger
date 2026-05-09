# Hardware Keylogger — Academic Research Project

> **⚠️ LEGAL DISCLAIMER**
> Please read the [Disclaimer](#disclaimer) section in full before using anything in this repository.

---

## Table of Contents

- [Project Description](#project-description)
- [Academic Context](#academic-context)
- [Hardware](#hardware)
- [Software](#software)
- [Licenses](#licenses)
- [Disclaimer](#disclaimer)

---

## Project Description

This repository contains schematics, layout files, and firmware for a hardware keylogger developed as part of an academic thesis. 

The device is designed exclusively for **controlled laboratory environments and explicitly authorized penetration tests**.

---

## Academic Context

This project is part of the following academic work:

| | |
|---|---|
| **Title** | USB Keylogging from scratch |
| **Author** | Gereon Such |
| **Institution** | Wilhelm Büchner Hochschule |
| **Supervisor** | Michael Fleury |
| **Submitted** | May 2026 |

---

## Hardware

There are multiple schematics in this project:

- USB Feedthrough and daughterboard for Rsapberry Pi PICO2 board under hardware/pico2daughterboard
- Full custom Keylogger board with RP235x microcontroller under hardware/full_rp2350_keylogger
  Since some of the components suggested by the Raspberry Pi hardware guide do not have an equivalent footprint in the KiCAD base library. There is a script to clone these components to the project directory, which should be done before loading the schematic. Nonetheless, you can select footprints yourself, but this will most propably invalidate the pcb design.

### Hardware License

All hardware design files (schematics, PCB layout, BOM) are released under the
**CERN Open Hardware Licence Version 2 — Strongly Reciprocal (CERN-OHL-S v2)**.
See [`hardware/LICENSE`](hardware/LICENSE).

---

## Software

### Directory Structure
```
firmware/
├── src/              # Source code
├── platformio.ini    # Project configuration
├── build.sh          # Build script for compilation and flashing
└── LICENSE           # Software license for all firmware files
```

### Software License

All source code is released under the **GNU General Public License v3.0 (GPL-3.0)**.
See [`firmware/LICENSE`](firmware/LICENSE).

---

## Licenses

| Component | License |
|---|---|
| Firmware / Software | [GPL-3.0](firmware/LICENSE) |
| Hardware Design | [CERN-OHL-S v2](hardware/LICENSE) |
| Documentation | [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) |

Both the GPL-3.0 and CERN-OHL-S v2 are **strong copyleft licenses**: any derivative work must be released under the same terms.

---

## Disclaimer

This project was developed solely for **academic and scientific research purposes** under institutional supervision. It is intended exclusively for use in **authorized and controlled test environments**.

### Authorized Use Only

Using this device or any part of this software on systems, devices, or networks **without the explicit prior consent of the owner is illegal**. Unauthorized deployment may constitute a criminal offence under German law, including but not limited to:

| Statute | Offence |
|---|---|
| **§ 202a StGB** | *Ausspähen von Daten* — Unauthorized access to protected data |
| **§ 202b StGB** | *Abfangen von Daten* — Interception of data |
| **§ 303a StGB** | *Datenveränderung* — Unlawful alteration of data |
| **§ 303b StGB** | *Computersabotage* — Computer sabotage |

Depending on your jurisdiction, equivalent or additional statutes may apply. It is your sole responsibility to ensure that any use of this project complies with all applicable local, national, and international laws.

### No Liability

The authors, contributors, and affiliated institution of this project accept **no liability whatsoever** for any damages, data loss, legal consequences, or other harm arising from the use, misuse, or inability to use this project or any of its components. This includes, but is not limited to, direct, indirect, incidental, or consequential damages.

This software and hardware are provided **"as is"**, without warranty of any kind, express or implied.

### Responsible Disclosure

This project is published in the spirit of **responsible disclosure** and to advance the scientific and academic discourse on hardware security. The authors explicitly condemn any malicious, unauthorized, or otherwise unlawful use of the concepts, designs, or code contained in this repository.

---

*For questions regarding authorized use or the academic context of this project, please contact me via https://suchdsp.com*
