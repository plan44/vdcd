# How to build vdcd on Linux

(in particular: on RaspberryPi with plain Raspian or Minibian based P44-DSB-X, or on a vanilla Ubuntu desktop)

## Platform

This How-To assumes you have

- either a [P44-DSB-X SD Card image](https://plan44.ch/downloads/p44-dsb-x-diy.zip)
  or a regular [Raspian](http://www.raspbian.org) on a RaspberryPi. Model 2 recommended, or
  you'll have to waaaait a lot! 

- or a [Ubuntu 16.04.2 LTS desktop](http://www.ubuntu.com/download/desktop) - other Linux will probably do as well, but Ubuntu is what I just tested (fresh copy in VMWare)

**On Unbuntu**, we work from a user account and use "sudo" when needed

**On the P44-DSB-X Minibian**, there's only the "root" user, so we login as root there (user:root, password:eXperiment).
Therefore, we do not need (nor can't) use "sudo".

To generalize the following instructions for both platforms, we'll define a shell var *$SUPER* to prefix commands that need root rights below. So you can 1:1 copy and use the command lines as shown below on both platforms.

On the other hand, on RPi we need to make sure the entire space of the SD card (>=4GB recommended) is ready to be used, which means that the originally small (800MB) P44-DSB-X partition must be expanded.

So we have to prepare...

### ...for Minibian/Raspian

Login as *root* with password *raspberry* (on a fresh Raspian/Minibian) or password *eXperiment* on a P44-DSB-X. Then:

	# expand the partition first
	# - for Minibian/P44-DSB-X, raspi-config is not installed by default:
	apt-get install raspi-config
	raspi-config
	# Now choose option 1, "Expand Filesystem", and reboot, then login again as root
	
### ...for Ubuntu or other desktop Linux

	SUPER=sudo

Now the platform is ready to install & build!

## Install build tools

	# update package lists
	$SUPER apt-get update
	
	# build tools
	$SUPER apt-get install git automake libtool autoconf g++ make

To put all projects into, we create a *ds* subdirectory and set the *$DSROOT* shell var to point to it:

	mkdir ~/ds
	cd ~/ds
	DSROOT=$(pwd)


## build plan44.ch's vdcd

### install libraries needed for vdcd only

	$SUPER apt-get install libjson-c-dev libsqlite3-dev protobuf-c-compiler libprotobuf-c-dev libboost-dev libi2c-dev libssl-dev libavahi-core-dev libavahi-client-dev

On older systems you might also need to install openssl-dev (but on recent systems that package is no longer available nor needed, you'll get an error if you try):
	
    $SUPER apt-get install openssl-dev
	
### Checkout vdcd sources

	cd ${DSROOT}
	git clone https://github.com/plan44/vdcd.git
	cd ${DSROOT}/vdcd

Note: after cloning, *master* branch is checked out, which represents a consistent state of currently tested development version (builds, runs).
If you want the last beta release, check out *testing*, for the last production release *production*, and for my cutting edge work in progress (sometimes not fully functional), check out the *luz* branch instead:

	git checkout testing
	
vdcd uses the [p44vdc](https://github.com/plan44/p44vdc) and [p44utils](https://github.com/plan44/p44utils) frameworks, which are located in a separate git repositories. To get the matching versions of these submodules:

    git submodule init
    git submodule update

### build vdcd

	cd ${DSROOT}/vdcd
	autoreconf -i
	./configure
	# note: need make clean to force rebuilding protobuf-c generated files
	# (which are checked-in for bulding on devices w/o protoc)
	make clean
	# note: First build must be make all, because otherwise *.proto
	# derived sources are not generated.
	# to rebuild after changes later, just type "make" or "make vdcd"
	make all
	
	
### quick test vdcd

	./vdcd --help
	
Should output the usage text explaining the command line options.


## Run vdcd for experimenting

**Note:** on a P44-DSB-X, there's a vdcd and vdsm already running. To test the self-built versions, you need to shut down these first:

	# to test your own vdcd:
	sv stop vdcd
	
For running vdcd, we need a directory for persistent data (sqlite3 databases) storage:

	mkdir ~/ds/data
		
Now everything is ready to actually run a vdcd with virtual devices, have the vdsm connect it and get connected into a digitalSTROM system on the same LAN.

### vdcd

Start a vdcd, with some command line options to create a console button and a console dimmer simulation device.
	
	~/ds/vdcd/vdcd --vdsmnonlocal --sqlitedir ~/ds/data --consoleio testbutton:button --consoleio testlamp:dimmer
	
If you want to explore the vdcd properties using JSON also enable the config api (with *--cfgapiport 8090*). On the P44-DSB-X, the mg44 webserver already handles forwarding http request to the socket based JSON API. On other platforms, see *json\_api\_forwarder* folder for a small PHP script for that task.

#### vdcd external devices API

vdcd supports "external devices", which are external scripts or programs that connect to the vdcd to instantiate devices, and communicated via the *plan44 vdcd external device API*. This API is a very easy to use API designed to simplify development of custom devices.

To start vdcd with external devices support enabled (API at TCP port 8999), call vdcd as follows:

    ~/ds/vdcd/vdcd --sqlitedir ~/ds/data --externaldevices 8999
    
To allow external device scripts/programs from other hosts than where vdcd runs (not recommended for security reasons in productive installations, but handy for development and testing), add the *--externalnonlocal* command line option.

For more information about the external devices API, please consult the *plan44 vdcd external device API.pdf* document in the *docs* folder, and check out the sample scripts in the *external\_devices\_samples* folder.


#### vdc API

See [www.digitalstrom.org/allianz/entwickler/developer-resources/system-apis/](https://www.digitalstrom.org/allianz/entwickler/developer-resources/system-apis/) for the detailed vDC API specs.

Note that while the real vDC API is a protobuf API, the vdcd also exposes the same API as JSON (to be able to create web interfaces like P44-DSB's). For using the same functionality as described in the specs you need to use POST (or direct TCP socket connection) to send a JSON object containing either *method=\<methodname\>* or *notification=\<notificationname\>*, plus any parameter as described in the specs.

If you start the vdcd with -l 7 (full logging), you can watch the vdsm talking to the vdcd and see how it works - the requests and answers are then shown in a pseudo-JSON (no quotes) form.
