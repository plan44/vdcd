vdcd
====

*[[if you want to support vdcd development, please consider to sponsor plan44]](https://github.com/sponsors/plan44)*

*vdcd* is a free (opensource, GPLv3) home automation daemon for operating various types of home automation devices of various technologies under a common device model and API.

The *vdcd* project has started as, and is still used as, a virtual device connector (*vdc*) implementation for the *Digital Strom* home automation system. A *vdc* integrates third-party automation hardware as virtual devices into the *Digital Strom* system. Hence, the device model and API design originates from a cooperation with *Digital Strom*.

However, *vdcd* can also be **operated as a fully standalone home automation controller** (see _--localcontroller_ commandline option) or **as a *matter* bridge** when used together with [p44mbrd](https://github.com/plan44/p44mbrd).

*vdcd* has ready-to-use implementation for various **EnOcean** devices, **DALI** lights (single dimmers, **RGB and RGBW** multi-channel **color lights** including DT6 and DT8 support), **Wiser-by-Feller** device (via **uGateway**), Philips **hue LED color lights** (via hue bridge), **WS281x RGB LED chains** (directly on RPi, via p44-ledchain driver on MT7688), simple contacts and on-off switches connected to Linux **GPIO** and **I2C** pins, **PWM** outputs via i2c, **DMX512** support via OLA or UART, console based debugging devices and a lot more.

When vdcd is built with *p44script* enabled, **custom devices** can be implemented as **simple scripts**. The p44script language has support for http and websocket APIs, MIDI, modbus, sockets, UART etc. and can make use of the [**p44lrgraphics**](https://github.com/plan44/p44lrgraphics) subsystem to create complex LED matrix effects. It can also use [**lvgl**](https://lvgl.io/) to define UIs on local touch displays directly from p44script.

In addition to these built-in implementations, vdcd provides the **plan44 vdcd external device API**, a simple socket-based API that allows implementing **custom devices as external scripts or programs** in any language which can open socket connections (almost any, sample code for bash, C and nodeJS is included)

vdcd however is not limited to the set of features listed above - it is based on a generic C++ framework called [**p44vdc**](https://github.com/plan44/p44vdc) which is included as a submodule into this project.

**p44vdc** is designed for easily creating additional integrations for many other types of third-party hardware. The framework implements the entire complexity of a modern automation device model, provides the vDC API to access it on an unified high level with standard behaviour for device classes like buttons, inputs, lights, shades, fans, generic outputs and various sensors.

For new hardware, only the actual access to the device's hardware needs to be implemented.

vdcd/p44vdc are based on a set of generic C++ utility classes called [**p44utils**](https://github.com/plan44/p44utils), which provides basic mechanisms for mainloop-based, nonblocking I/O driven automation daemons, as well as a script language, [**p44script**](https://plan44.ch/p44-techdocs/en/#topics). p44utils is also included as a submodule into this project.


If you like this project you might want to...

- See it in action by [trying the p44 automation platform](#try), see below.
- Join the [plan44 community forum](https://forum.plan44.ch/t/opensource-c-vdcd) to ask questions and discuss vdcd related topics.
- See [github project](https://github.com/plan44/vdcd) to get the latest version of the software (the required p44vdc and p44utils submodules are also [on github](https://github.com/plan44))
- not forget to support it via [github sponsors](https://github.com/sponsors/plan44) or [flattr](https://flattr.com/@luz)


License
-------

vdcd is licensed under the GPLv3 License (see COPYING).

vdcd makes use of several submodules, which may be entirely or partially under different, compatible licenses.
Please see the COPYING files, README.md and license header comments in the individual submodules.


Features
--------

- Implements the complete *Digital Strom* vDC API including behaviours for buttons, binary inputs, lights, color lights, sensors, heating valves and shadow blinds.
- Provides the *vDC API* (which is based on protobuf) also in a JSON version, with additional features which allow building local web interfaces.
- Optionally provides a second instance of the JSON api for connecting bridges to other types of home automation systems, in particular *matter* (see [p44mbrd](https://github.com/plan44/p44mbrd)).
- Provides the *plan44 vdcd external device API* for easily building custom devices as external scripts or programs.
- Provides extended customisation features by using the *p44script* scripting language
- Allows implementing fully dS compliant devices of all kinds completely in *p44script* without any external code needed.
- Supports EnOcean TCM310 based gateway modules, connected via serial port or network
- Supports Philips hue lights via the hue bridge and its JSON API
- Supports Wiser-by-Feller installations via full integration with the uGateway API
- Supports building really fancy effect color LED lights out WS281x LED chip based LED chains/matrices, with moving segments, lightspots, gradients etc, based on [**p44lrgraphics**](https://github.com/plan44/p44lrgraphics), a graphics library specifically written for lo-res LED matrix displays.
  On Raspberry Pi, just connect a WS2812's data-in to RPi P1 Pin 12, GPIO 18 (thanks to the [rpi_ws281x library](https://github.com/richardghirst/rpi_ws281x.git)).
  On MT7688 systems under OpenWrt, use the [p44-ledchain kernel driver](https://github.com/plan44/plan44-feed/tree/master/p44-ledchain).
- Supports DMX512 based lights via RS485 UART or via [Open Lighting Architecture - OLA](http://www.openlighting.org/) - single channel, RGB, RGBW, RGBWA, moving head.
- Allows to use Linux GPIO pins (e.g. on RaspberryPi) as button inputs or on/off outputs
- Allows to use Linux PWM output pins as dimmable outputs
- Allows to use i2c and spi peripherals (supported chips e.g. TCA9555, PCF8574, PCA9685, MCP23017, MCP23S17, LM75, MCP3021, MAX1161x, MCP3008, MCP3002) for digital and analog I/O

Getting Started
---------------

### <a id="try">To try it out

- plan44.ch provides free RaspberryPi images named `P44-LC-X` (standalone controller / *matter* bridge) and `P44-DSB-X` (for use with *Digital Strom*)  which contains a complete OpenWrt ready to run first experiments with virtual devices (for example driving GPIO pins of the Raspberry). You can download it from [plan44.ch/automation/p44-lc-x.php](https://plan44.ch/automation/p44-lc-x.php) and [plan44.ch/automation/p44-dsb-x.php](https://plan44.ch/automation/p44-dsb-x.php), resp., copy it to a SD Card, and use it with a RPi B, B+, 2,3 and 4.
- you can also build the openwrt image yourselves, using [p44-xx-open](https://github.com/plan44/p44-xx-open).

### Build it

- Choose suitable branch:
  - **main**: consistent state of current tested development version (builds, runs)
  - **luz**: sometimes contains interesting work in progress not yet in master...
  - **testing**: corresponds with testing version deployed to beta testers of P44-DSB-E/P44-DSB-DEH product users.
  - **production**: corresponds with version productively used in current [plan44.ch products](https://plan44.ch/automation/) (P44-DSB, P44-LC and P44-AC devices)

### Build it directly on Linux or macOS

- Clone the github repository for that branch

    ```
    # set the branch name you want to use
    BRANCH=main
    # clone including all needed submodules
    git clone -b ${BRANCH} --recurse-submodules https://github.com/plan44/vdcd
    ```

- consult the */docs* folder: see *"How to build vdcd on Linux.md"* and *"How to build and run vdcd on Mac OS X.md"*.

### Build it as an openwrt package

- Include the [plan44 OpenWrt feed](https://github.com/plan44/plan44-feed) and install/build the `vdcd` package.

### Build and run it in a Container

- Build container image. If you want another branch than main,

    ```bash
    # set the branch name you want to use
    BRANCH=main
    # clone, but no submodules needed, just to get the Dockerfile
    git clone -b ${BRANCH} https://github.com/plan44/vdcd
    # build the docker image
    cd vdcd
    docker build --build-arg BRANCH=${BRANCH} -t my_vdcd .
    ```

- Run vdcd as container, for the autodiscovery to work you have to mount your dbus and avahi-daemon socket into the container

    ```bash
    docker run --network="host" -v /var/run/dbus:/var/run/dbus -v /var/run/avahi-daemon/socket:/var/run/avahi-daemon/socket my_vdcd vdcd [options]
    ```


Supporting vdcd
---------------

1. use it!
2. support development via [github sponsors](https://github.com/sponsors/plan44) or [flattr](https://flattr.com/@luz)
3. Discuss it in the [plan44 community forum](https://forum.plan44.ch/t/opensource-c-vdcd).
3. contribute patches, report issues and suggest new functionality [on github](https://github.com/plan44/vdcd) or in the [forum](https://forum.plan44.ch/t/opensource-c-vdcd).
4. build cool new device integrations and contribute those
5. Buy plan44.ch [products](https://plan44.ch/automation/) - sales revenue is paying the time for contributing to opensource projects :-)

*(c) 2013-2025 by Lukas Zeller / [plan44.ch](http://www.plan44.ch/automation)*







