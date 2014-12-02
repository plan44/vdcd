
vdcd
====

[![Flattr this git repo](http://api.flattr.com/button/flattr-badge-large.png)](https://flattr.com/submit/auto?user_id=luz&url=https://github.com/plan44/vdcd&title=vdcd&language=&tags=github&category=software) 

"vdcd" is a free (opensource, GPLv3) virtual device connector (vdc) implementation for digitalSTROM systems.
A vdc integrates third-party automation hardware as virtual devices into a digitalSTROM system.

This vdcd has ready-to-use implementation for various **EnOcean** devices, **DALI** lamps (single dimmers or **RGB and RGBW** multi-channel **color lights**), Philips **hue LED color lights**,
simple contacts and on-off switches connected to Linux **GPIO** and **I2C** pins, **PWM** outputs via i2c, experimental **DMX512** support via OLA, **Spark Core** based devices support
and console based debugging devices.

vdcd however is not limited to that - is based on a generic C++ framework designed for easily creating additional integrations for many other types of third-party hardware. The framework implements the entire complexity of the digitalSTROM vDC API and the standard behaviour expected from digitalSTROM buttons, inputs, (possibly dimming) outputs and various sensors.

For new hardware, only the actual access to the device's hardware needs to be implemented.

<p>If you like this, please don't forget to flattr it :-)</p>

<ul>
<li>See <a target="_blank" href="https://github.com/plan44/vdcd">github project</a> to get the latest version of the software</li>
<li>See <a target="_blank" href="http://www.digitalstrom.com">digitalstrom.com</a> and <a target="_blank" href="http://www.digitalstrom.org">digitalstrom.org</a> for more about digitalSTROM</li>
</ul>

License
-------

vdcd is licensed under the GPLv3 License (see COPYING).

If that's a problem for your particular application, I am open to provide a commercial license, please contact me at [luz@plan44.ch](mailto:luz@plan44.ch).


Features
--------

- Implements the complete digitalSTROM vDC API including behaviours for buttons, binary inputs, lights, color lights, sensors and heating valves
- Provides the vDC API (which is based on protobuf) also in a JSON version, with additional features which allow building local web interfaces.
- Supports EnOcean TCM310 based gateway modules, connected via serial port or network
- Supports Philips hue lights via the hue bridge and its JSON API
- Allows to use Linux GPIO pins (e.g. on RaspberryPi) as button inputs or on/off outputs
- Allows to use i2c peripherals (supported chips e.g. TCA9555, PCF8574, PCA9685) for digital I/O as well as PWM outputs
- Implements interface to [Open Lighting Architecture - OLA](http://www.openlighting.org/) to control DMX512 based lights (single channel, RGB, RGBW, RGBWA, moving head)


Getting Started
---------------

- Clone the github repository

    `git clone https://github.com/plan44/vdcd`

- check out the /docs folder, in particular the file *"How to build and run vdcd + mg44 on Linux (in particular, Raspberry Pi).txt"*


Supporting vdcd
---------------

1. use it!
2. contribute patches, report issues and suggest new functionality
3. build cool new device integrations and contribute those
4. Buy plan44.ch products - sales revenue is paying the time for contributing to opensource projects :-)

(c) 2013-2014 by Lukas Zeller / [plan44.ch](http://www.plan44.ch/automation)







