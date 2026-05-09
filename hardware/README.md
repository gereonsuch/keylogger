# Hardware Keylogger — Academic Research Project

> **⚠️ LEGAL DISCLAIMER**
> Please read the [Disclaimer](#disclaimer) section in full before using anything in this repository.

---

## Subprojects

I tried different setups during the design process. This folder contains the relevant ones.

- pico2_usb_through_daughterboard
  This is a daughterboard for the Pico2 board you can commonly purchase. Since this PCB is pretty minimalistic, you can also choose to just implement on a perfboard or stripboard. 
  - /pico2_usb_through_daughterboard contains the kicad project including production files
  - /pico2_usb_through_daughterboard__gerber.zip contains the gerber files for jlcpcb production without any components

- full_keylogger_rp2354
  This is the full keylogger including on board SD card holder, power supply and quartz oscillator and USB in/out and USB debug connector. 
  This layout is very condensed, just as a keylogger should be ;) Maybe I design stl files for a 3d printable enclosing in the near future...
  As the pushbuttons are optional and the USB-A connectors are easily solderable by hand, they are left out in the production files. 
  - /full_keylogger_rp2354 contains the kicad project. be aware, that some footprints are custom from eda files. you will need to clone these with your preferred method. LCSC numbers for the parts are added
  - /full_keylogger_rp2354__only_top_components__gerber.zip contains all gerber files for PCB production for TOPSIDE assembly only. This reduces cost massively as only one side of the board needs to be assembled and the SD card is optional anyways.
  - /full_keylogger_rp2354__only_top_components__BOM.csv contains the material list of all TOPSIDE components
  - /full_keylogger_rp2354__only_top_components__CPL.csv contains the component placement of all TOPSIDE components
  - /full_keylogger_rp2354__gerber.zip contains all gerber files for PCB production for production. This is the dual side assembly, which is slightly more costly
  - /full_keylogger_rp2354__BOM.csv contains the material list for DUAL SIDE PLACEMENT components
  - /full_keylogger_rp2354__CPL.csv contains the component placement for DUAL SIDE PLACEMENT components
  
___!!! Be aware, that some component placements like the power regulator and the inductivity need to be rotated at JLCPCB production !!!___

---

## Hardware License

All hardware design files (schematics, PCB layout, BOM) are released under the
**CERN Open Hardware Licence Version 2 — Strongly Reciprocal (CERN-OHL-S v2)**.
See [`hardware/LICENSE`](hardware/LICENSE).

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
