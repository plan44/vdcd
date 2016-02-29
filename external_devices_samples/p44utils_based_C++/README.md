
## vdcd External Devices - C++ sample using the p44utils framework

This folder contains a sample C++ program for an external weather station with different sensors, connected via a serial port (Elsner P03).

The implementation uses the same base framework as vdcd, the "p44utils" (a set of C++ classes and a mainloop implementation for building applications based on I/O events and timers)

Even if you don't have this particular weather station, you will still see a sensor device with many inputs appear in your digitalSTROM system when you successfully build and run this example.

You can use this non-trivial example as a starting point to build your own devices, making use of the p44utils for managing serial ports, connections, timers, I/O pins etc.

### Notes about p44utils

This example is part of the [vdcd git repository](https://github.com/plan44/vdcd) and is located within the *external\_devices\_samples* subfolder.

If you haven't already, you can clone the entire vdcd project with

	git clone https://github.com/plan44/vdcd.git
	
As this example also needs the [p44utils](https://github.com/plan44/p44utils), which is included in *vdcd* as a submodule, make sure the *p44utils* submodule is initialized and up-to-date:

    git submodule init
    git submodule update

This example refers to the *vdcd* level *p44utils* via a relative softlink in this example's *src* folder.

So, in case you want to separate this sample from the vdcd project later, don't forget to replace the softlink by a git submodule cloned from the [p44utils repository](https://github.com/plan44/p44utils) or a simple file copy of the *p44utils* sources.

### Installing Dependencies (Linux)

This example uses autotools and the C++ compiler:

    sudo apt-get install automake autoconf g++

This example also needs boost and json-c, so install those as well:

    sudo apt-get install libboost-dev libjson0-dev

If you want to use the generic digital and/or analog I/O classes, you'll also need i2c support (not required for the example as-is):

    sudo apt-get install libi2c-dev
    
### Building the Example

To build just type:

    autoreconf -i
    ./configure
    make

Now you should have an executable named *p44utilsdevice*

### Testing the Example

Now you can run the example with

    ./p44utilsdevice --apihost 127.0.0.1 --apiport 8999 --serialport /dev/ttyUSBxyz

(assuming you have a vdcd with external device API enabled running on the same computer, and the ElsnerP03 weather station connected at /dev/ttyUSBxyz)

To avoid needing a separate logfile, external devices api supports sending log messages to the vdcd so these will appear in the regular vdcd log. The sample code can automatically transmit messages logged with LOG() to the vdcd when started with the *--logtoapi* flag.

### Installing the Example as a permanently operating external device

The example program itself does not restart the connection when it is closed (due to error or vdcd being restarted), but just exits.

So, for permanent operation, the program should be put under control of a daemon supervisor such as *runit* or *launchd* to make sure the program is restarted automatically.

If you want to use *runit* (already installed on P44-DSB-X) you can just copy the contents of */runit-service* dir to the runit service directory (*/etc/service* on the P44-DSB-X), and the *p44utilsdevice* to */usr/bin*. You may need to edit */etc/service/p44utilsdevice/run* to set the correct *SERIALPORT* definition to connect to the Elsner P03 hardware.

Then you can start and stop the external device:

	sv start p44utilsdevice
	sv stop p44utilsdevice
	
After reboot, the external device will automatically started by runit.  

### Building and running on OS X in XCode

If you are on Mac OS X and have XCode installed, you can also open the p44utilsdevice.xcodeproj to build and run the sample in XCode. You might need to install dependencies (boost, json-c) with [http://brew.sh](http://brew.sh) before:

	brew install json-c
	brew install boost

### License

This example is automatically licensed under the GPLv3 License becauses it uses the p44utils which are GPLv3 licensed.

### More information 

Please refer to the *plan44 vdcd external device API* PDF document in the *docs* folder for a full documentation of the external device API and its features.
