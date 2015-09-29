
vdcd
====

[![Flattr this git repo](http://api.flattr.com/button/flattr-badge-large.png)](https://flattr.com/submit/auto?user_id=luz&url=https://github.com/plan44/vdcd&title=vdcd&language=&tags=github&category=software) 

"vdcd" is a free (opensource, GPLv3) virtual device connector (vdc) implementation for digitalSTROM systems.
A vdc integrates third-party automation hardware as virtual devices into a digitalSTROM system.

This vdcd has ready-to-use implementation for various **EnOcean** devices, **DALI** lamps (single dimmers or **RGB and RGBW** multi-channel **color lights**), Philips **hue LED color lights**,
**WS2812 RGB LED chains** (on RPi), simple contacts and on-off switches connected to Linux **GPIO** and **I2C** pins, **PWM** outputs via i2c, experimental **DMX512** support via OLA, **Spark Core** based devices support and console based debugging devices.

In addition to the built-in implementations, vdcd provides the **plan44 vdcd external device API**, a simple socket-based API that allows implementing **custom devices as external scripts or programs** in any language which can open socket connections (almost any, sample code for bash, C and nodeJS is included)

vdcd however is not limited to the set of features listed above - is based on a generic C++ framework designed for easily creating additional integrations for many other types of third-party hardware. The framework implements the entire complexity of the digitalSTROM vDC API and the standard behaviour expected from digitalSTROM buttons, inputs, (possibly dimming) outputs and various sensors.

For new hardware, only the actual access to the device's hardware needs to be implemented.

If you like this, please don't forget to flattr it :-)

- Join the [plan44_vdcd google groups mailing list](https://groups.google.com/forum/#!forum/plan44_vdcd) to ask questions and discuss vdcd related topics
- See [github project](https://github.com/plan44/vdcd) to get the latest version of the software
- See [digitalstrom.com](http://www.digitalstrom.com) and [digitalstrom.org](http://www.digitalstrom.org) for more about digitalSTROM

License
-------

vdcd is licensed under the GPLv3 License (see COPYING).

If that's a problem for your particular application, I am open to provide a commercial license, please contact me at [luz@plan44.ch](mailto:luz@plan44.ch).


Features
--------

- Implements the complete digitalSTROM vDC API including behaviours for buttons, binary inputs, lights, color lights, sensors, heating valves and shadow blinds.
- Provides the *vDC API* (which is based on protobuf) also in a JSON version, with additional features which allow building local web interfaces.
- Provides the *plan44 vdcd external device API* for easily building custom devices as external scripts or programs.
- Supports EnOcean TCM310 based gateway modules, connected via serial port or network
- Supports Philips hue lights via the hue bridge and its JSON API
- Supports WS2812 LED chip based RGB LED chains on Raspberry Pi (just connect a WS2812's data-in to RPi P1 Pin 12, GPIO 18), thanks to the [rpi_ws281x library](https://github.com/richardghirst/rpi_ws281x.git)
- Allows to use Linux GPIO pins (e.g. on RaspberryPi) as button inputs or on/off outputs
- Allows to use i2c peripherals (supported chips e.g. TCA9555, PCF8574, PCA9685) for digital I/O as well as PWM outputs
- Implements interface to [Open Lighting Architecture - OLA](http://www.openlighting.org/) to control DMX512 based lights (single channel, RGB, RGBW, RGBWA, moving head)


Getting Started
---------------

### To try it out

- plan44.ch provides a RaspberryPi image named P44-DSB-X which contains a complete Raspian/Minibian ready to run first experiments with virtual devices (for example driving GPIO pins of the Raspberry). You can download it from [plan44.ch/downloads/p44-dsb-x-diy.zip](https://plan44.ch/downloads/p44-dsb-x-diy.zip), copy it to a >=1GB SD Card and use it with a RPi B, B+ or 2.

### To build it or use the external devices API

- Clone the github repository

    `git clone https://github.com/plan44/vdcd`

- Choose suitable branch:
  - **master**: consistent state of current tested development version (builds, runs)
  - **luz**: sometimes contains interesting work in progress not yet in master...
  - **testing**: corresponds with testing version deployed to beta testers of P44-DSB-E/P44-DSB-DEH product users.
  - **production**: corresponds with version productively used in current [plan44.ch products](https://plan44.ch/automation/digitalstrom.php) (P44-DSB-E, P44-DSB-DEH)

- check out the /docs folder: For building the vdcd, see *"How to build vdsm and vdcd on Linux - in particular Raspian on P44-DSB-X.md"* and *How to build and run vdcd on Mac OS X*. For documentation of the external device API, see the PDF document named *plan44 vdcd external device API.pdf*


Supporting vdcd
---------------

1. use it!
2. contribute patches, report issues and suggest new functionality
3. build cool new device integrations and contribute those
4. Buy plan44.ch products - sales revenue is paying the time for contributing to opensource projects :-)

(c) 2013-2015 by Lukas Zeller / [plan44.ch](http://www.plan44.ch/automation)







