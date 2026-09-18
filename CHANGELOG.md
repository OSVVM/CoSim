# CoSim

| Revision  |  Release Summary | 
------------|-------------------
| ????.??   | streamGetBurst updated to add returning RX byte count |
| 2026.08   | Added support for DLL and PHY traffic generation over MIT for PCIe VC |
|           | Added support for burst word transactions |
|           | Updated rv32 to 1.3.6 |
|           | Added JFIF library |
|           | Added new demo (non-regression) tests |
|           | Added new generic user callback mechanism |
| 2026.01   | Added the inclusion of the PCIe C model to CoSim library compilation |
|           | Changes in support of PCIe VC |
|           | Added VIrq CoSim procedure |
|           | Updated rv32 RISC-V ISS to v1.3.1 |
| 2024.07   | Updated calls to CreateClock for it being moved to ClockResetPkg |
| 2023.07   | Updates to RISC-V ISS libraries and headers for FreeRTOS support |
| 2023.05   | Support for split transactions, responder, streaming and checking |
| 2023.01   | Initial release |

## ????.?? September 2026
- Variants of streamGetBurst added to return RX byte count when bytesize parameter passed in as *int

## 2026.08 August 2026
- Added support for DLL and PHY traffic generation over MIT for PCIe VC with
  new commands to both generate and receive DLL packets and generated PHY TS/OS
  traffic and retrieve TS/OS RX event counts.
- Added support for burst word transactions in addtion to byte bursts
- Updated the rv32 RISC-V ISS library to latest v1.3.6 version
- Added a new JFIF library for decoding JFIF/JPEG image files
- Added two new non-regression demonstration tests: one for using GTK+ 3.0 library with JFIF library, and one for rv32 application to compress data with LZW code
- Added a VHDL co-simulation procedure to call a user defined callback function with type and value arguments. This is was added mainly to have a means for a clock count to be passed to the C/C++ domain for synchronising time between logic simulation and software, but it can be used for sending any kind of data.

## 2026.01 January 2026
- Added the inclusion of the PCIe C model to CoSim library compilation.
  These new libraries exits in `PCIe/lib`, with headers in `PCIe/include`
- Changes in support of PCIe VC, including adding support for `SetModelOptions` and `GetModelOptions`
- Added a `VIrq` co-simulation procedure, wrapped in a new `CoSimIrq` VHDL procedure,
  to raise interrupts independent of `CoSimTrans` where required
- The rv32 RISC-V instruction set simulator library is updated to verion 1.3.1. New features include:
  - API access to internal cycle count
  - Registering of callback function on unimplemented instruction
  - Internal timing model configurable over API
  - Generation of software interrupt over external IRQ callback
  - Support for RV32B standard extensions (Zba, Zbb and Zbs)
  - Support for RV32_Zbc standard extension
  - Reading of binary program in addition to ELF support


## 2024.07 July 2024
- Updated calls to CreateClock for it being moved to ClockResetPkg

## 2023.07 July 2023
- Updates RISC-V ISS rv32 libraries and include headers for new features and fixes to support running FreeRTOS real-time operating system

## 2023.05 May 2023
- Added split transaction methods for address bus model independent manager
- Added support address bus model independent subordinate/responder
- Added support for streaming bus model independent transactions
- Added test data pattren generation methods
- Added data check methods

## 2023.01 January 2023
- New repository with ability to run C code in the testbench in a CoSim environment.  See README.md

 
## Copyright and License
Copyright (C) 2023 - 2026 by [OSVVM Authors](AUTHORS.md)   

This file is part of OSVVM.

    Licensed under Apache License, Version 2.0 (the "License")
    You may not use this file except in compliance with the License.
    You may obtain a copy of the License at

  [http://www.apache.org/licenses/LICENSE-2.0](http://www.apache.org/licenses/LICENSE-2.0)

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
