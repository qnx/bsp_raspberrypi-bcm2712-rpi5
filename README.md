# Raspberry Pi 5 BSP

QNX Board Support Package (BSP) for the Raspberry Pi 5

## Usage

For more information on this BSP, including how to build it and load it onto your board, see the official BSP User Guide:
* [BSP User Guide](https://www.qnx.com/developers/docs/BSP8.0/com.qnx.doc.bsp_raspberrypi.bcm2712.rpi5_8.0/topic/about.html)
* [Build the BSP](https://www.qnx.com/developers/docs/BSP8.0/com.qnx.doc.bsp_raspberrypi.bcm2712.rpi5_8.0/topic/common/bsp_build.html)

Note that this Git version of the BSP differs from the QNX Software Center (QSC) version in a couple ways:
* The Git version contains source code only (no prebuilt binaries).
* The Git version is not fully supported by the QNX Momentics IDE.
* The QSC version is delivered as a ZIP file. (You may notice references to a ZIP file in the BSP User Guide.)

## Preparation

* You need to have a local QNX SDP to build this project. To start, get a QNX SDP 8.0 license and follow the official instructions to install QNX SDP 8.0 to your host machine.
* In addition to a baseline SDP 8.0 installation, you will also need to confirm that you have installed all of the packages listed in "source.xml".

## Notice on Software Maturity and Quality

The software included in this repository is classified under our Software Maturity Standard as Experimental Software – Software Quality and Maturity Level (SQML) 1.

As defined in the [QNX Development License Agreement](http://www.qnx.com/download/feature.html?programid=68651), Experimental Software represents early-stage deliverables intended for evaluation or proof-of-concept purposes.

SQML 1 indicates that this software is provided without one or more of the following:
* Formal requirements
* Formal design or architecture
* Formal testing
* Formal support
* Formal documentation
* Certifications of any type
* End-of-Life or End-of-Support policy

Additionally, this software is not monitored or scanned under our Cybersecurity Management Standard.

No warranties, guarantees, or claims are offered at this SQML level.
