
vdcd
====

*[[if you want to support vdcd development, please consider to sponsor plan44]](https://github.com/sponsors/plan44)* 

"vdcd" is a free (opensource, GPLv3) virtual device connector (vdc) implementation for Digital Strom systems.
A vdc integrates third-party automation hardware as virtual devices into a Digital Strom system.
Optionally, it can also run as a standalone home automation controller (see _--localcontroller_ option).

This vdcd has ready-to-use implementation for various **EnOcean** devices, **DALI** lamps (single dimmers or **RGB and RGBW** multi-channel **color lights** including DT6 and DT8 support), Philips **hue LED color lights**,
**WS281x RGB LED chains** (directly on RPi, via p44-ledchain driver on MT7688), simple contacts and on-off switches connected to Linux **GPIO** and **I2C** pins, **PWM** outputs via i2c, experimental **DMX512** support via OLA and console based debugging devices.

When vdcd is built with *p44script* enabled, **custom devices** can be implemented as **simple scripts**. The p44script language has support for http and websocket APIs, and can make use of the [**p44lrgraphics**](https://github.com/plan44/p44lrgraphics) subsystem to create complex LED matrix effects.

In addition to the built-in implementations, vdcd provides the **plan44 vdcd external device API**, a simple socket-based API that allows implementing **custom devices as external scripts or programs** in any language which can open socket connections (almost any, sample code for bash, C and nodeJS is included)

vdcd however is not limited to the set of features listed above - it is based on a generic C++ framework called [**p44vdc**](https://github.com/plan44/p44vdc) which is included as a submodule into this project.

**p44vdc** is designed for easily creating additional integrations for many other types of third-party hardware. The framework implements the entire complexity of the Digital Strom vDC API and the standard behaviour expected from Digital Strom buttons, inputs, (possibly dimming) outputs and various sensors.

For new hardware, only the actual access to the device's hardware needs to be implemented.

vdcd/p44vdc are based on a set of generic C++ utility classes called [**p44utils**](https://github.com/plan44/p44utils), which provides basic mechanisms for mainloop-based, nonblocking I/O driven automation daemons, as well as a script language, [**p44script**](https://plan44.ch/p44-techdocs/en/#topics). p44utils is also included as a submodule into this project.


If you like this project you might want to...

- Join the [plan44 community forum](https://forum.plan44.ch/t/opensource-c-vdcd) to ask questions and discuss vdcd related topics.
- See [github project](https://github.com/plan44/vdcd) to get the latest version of the software (the required p44vdc and p44utils submodules are also [on github](https://github.com/plan44))
- See [digitalstrom.com](http://www.digitalstrom.com) and [digitalstrom.org](http://www.digitalstrom.org) for more about Digital Strom
- not forget to support it via [github sponsors](https://github.com/sponsors/plan44) or [flattr](https://flattr.com/@luz)


License
-------

vdcd is licensed under the GPLv3 License (see COPYING).

If that's a problem for your particular application, I am open to provide a commercial license, please contact me at [luz@plan44.ch](mailto:luz@plan44.ch).


Features
--------
- Implements the complete Digital Strom vDC API including behaviours for buttons, binary inputs, lights, color lights, sensors, heating valves and shadow blinds.
- Provides the *vDC API* (which is based on protobuf) also in a JSON version, with additional features which allow building local web interfaces.
- Provides the *plan44 vdcd external device API* for easily building custom devices as external scripts or programs.
- Provides extended customisation features by using the *p44script* scripting language
- Allows implementing fully dS compliant devices of all kinds completely in *p44script* without any external code needed.
- Supports EnOcean TCM310 based gateway modules, connected via serial port or network
- Supports Philips hue lights via the hue bridge and its JSON API
- Supports building really fancy effect color LED lights out WS281x LED chip based LED chains/matrices, with moving segments, lightspots, gradients and even [*expressive pixels*](https://www.microsoft.com/en-us/research/project/microsoft-expressive-pixels) animations.
Based on [**p44lrgraphics**](https://github.com/plan44/p44lrgraphics), a graphics library specifically written for lo-res LED matrix displays.
  On Raspberry Pi, just connect a WS2812's data-in to RPi P1 Pin 12, GPIO 18 (thanks to the [rpi_ws281x library](https://github.com/richardghirst/rpi_ws281x.git)).
  On MT7688 systems under OpenWrt, use the [p44-ledchain kernel driver](https://github.com/plan44/plan44-feed/tree/master/p44-ledchain).
- Allows to use Linux GPIO pins (e.g. on RaspberryPi) as button inputs or on/off outputs
- Allows to use Linux PWM output pins as dimmable outputs
- Allows to use i2c and spi peripherals (supported chips e.g. TCA9555, PCF8574, PCA9685, MCP23017, MCP23S17, LM75, MCP3021, MAX1161x, MCP3008, MCP3002) for digital and analog I/O
- Implements interface to [Open Lighting Architecture - OLA](http://www.openlighting.org/) to control DMX512 based lights (single channel, RGB, RGBW, RGBWA, moving head)

Getting Started
---------------

### To try it out

- plan44.ch provides a RaspberryPi image named P44-DSB-X which contains a complete OpenWrt ready to run first experiments with virtual devices (for example driving GPIO pins of the Raspberry). You can download it from [https://plan44.ch/automation/p44-dsb-x.php](https://plan44.ch/automation/p44-dsb-x.php), copy it to a SD Card and use it with a RPi B, B+, 2,3 and 4.

### To build it or use the external devices API

- Clone the github repository

    `git clone https://github.com/plan44/vdcd`

- Choose suitable branch:
  - **master**: consistent state of current tested development version (builds, runs)
  - **luz**: sometimes contains interesting work in progress not yet in master...
  - **testing**: corresponds with testing version deployed to beta testers of P44-DSB-E/P44-DSB-DEH product users.
  - **production**: corresponds with version productively used in current [plan44.ch products](https://plan44.ch/automation/digitalstrom.php) (P44-DSB-E/E2/DEH/DEH2, P44-LC-DE)

- consult the */docs* folder: For building the vdcd, see *"How to build vdcd on Linux.md"* and *"How to build and run vdcd on Mac OS X.md"*. For documentation of the external device API, see the PDF document named *"plan44 vdcd external device API.pdf"*

### Build and run it in a Container

- Clone the github repository

    `git clone https://github.com/plan44/vdcd`

- Set the vdcd branch you want to use (see above)
  (by changing the ENV BRANCH line in the `Dockerfile`; as-is, this is set to `master`)

- Build container image

    `cd vdcd`
    `docker build -t myimagename .`

- Run vdcd as container, for the autodiscovery to work you have to mount your dbus and avahi-daemon socket into the container

    `docker run --network="host" -v /var/run/dbus:/var/run/dbus -v /var/run/avahi-daemon/socket:/var/run/avahi-daemon/socket myimagename vdcd [options]`

Supporting vdcd
---------------

1. use it!
2. support development via [github sponsors](https://github.com/sponsors/plan44) or [flattr](https://flattr.com/@luz)
3. Discuss it in the [plan44 community forum](https://forum.plan44.ch/t/opensource-c-vdcd).
3. contribute patches, report issues and suggest new functionality [on github](https://github.com/plan44/vdcd) or in the [forum](https://forum.plan44.ch/t/opensource-c-vdcd).
4. build cool new device integrations and contribute those
5. Buy plan44.ch [products](https://plan44.ch/automation/products.php) - sales revenue is paying the time for contributing to opensource projects :-)

*(c) 2013-2022 by Lukas Zeller / [plan44.ch](http://www.plan44.ch/automation)*







