# How to build vdcd (and vdsm if needed)

(in particular: on RaspberryPi with plain Raspian or Minibian based P44-DSB-X, or on a vanilla Ubuntu desktop)

## Platform

This How-To assumes you have

- either a [P44-DSB-X SD Card image](https://plan44.ch/downloads/p44-dsb-x-diy.zip)
  or a regular [Raspian](http://www.raspbian.org) on a RaspberryPi. Model 2 recommended, or
  you'll have to waaaait a lot! 

- or a [Ubuntu 14.04.2 LTS desktop](http://www.ubuntu.com/download/desktop) - other Linux will probably do as well, but Ubuntu is what I just tested (fresh copy in VMWare)

On Unbuntu, we work from a user account and use "sudo" when needed

On the P44-DSB-X Minibian, there's only the "root" user, so we login as root there (user:root, password:eXperiment).

Therefore, we do not need (nor can't) use "sudo". To generalize the following instructions for both platforms, we'll define a shell var *$SUPER* to prefix commands that need root rights below. So you can 1:1 copy and use the command lines as shown below on both platforms.

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

## Install build tools and common libraries

	# update package lists
	$SUPER apt-get update
	
	# build tools
	$SUPER apt-get install git automake libtool autoconf g++

	# common libraries for both vdcd and vdsm
	$SUPER apt-get install libjson0-dev libsqlite3-dev protobuf-c-compiler libprotobuf-c0-dev

To put all projects into, we create a *ds* subdirectory and set the *$DSROOT* shell var to point to it:

	mkdir ~/ds
	cd ~/ds
	DSROOT=`pwd`


## build plan44.ch's vdcd

### install libraries needed for vdcd only

	$SUPER apt-get install libboost-dev libi2c-dev libssl-dev libavahi-core-dev
	
### Checkout vdcd sources

	cd ${DSROOT}
	git clone https://github.com/plan44/vdcd.git
	cd ${DSROOT}/vdcd

Note: after cloning, *master* branch is checked out, which represents a consistent state of currently tested development version (builds, runs).
If you want the last beta release, check out *testing*, for the last production release *production*, and for my cutting edge work in progress (sometimes not fully functional), check out the *luz* branch instead:

	git checkout testing
	
vdcd uses the [p44utils](https://github.com/plan44/vdcd) which are now located in a separate git repository. To get the matching version of p44utils:

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
	# this builds vdcd, demovdc and jsonrpctool
	# to rebuild after changes later, just type "make vdcd" or "make demovdc"
	make all
	
	
### quick test vdcd

	./vdcd --help
	
Should output the usage text explaining the command line options.


## build digitalSTROM vdsm

**Please check first if you really need your own build of the vdsm at all** (it takes time and space on your device to do so...)

- You **don't need to build** a vdsm (which has lots of dependencies) if you base your experiments on the P44-DSB-X image, which has a vdsm already running.

- You will **not need your own vdsm** (nor will the P44-DSB-X need it any more) in the mid term future, because the dSS will provide a running so-called *master vdSM* in a future release (you can already install a vdsm on today with *opkg* if your dSS is on the "testing" feed). In fact, P44-DSB-X, starting with firmware version 1.5.0.5, will search the network for the presence of a *master vdSM*, and if one is found, it will automatically shut down the internal (so called *auxiliary*) vdSM. 

But still, here are the instructions how to compile a vdSM if you need to:

### install tools and libraries needed for vdsm only

	$SUPER apt-get install cmake python
	$SUPER apt-get install libossp-uuid-dev libavahi-client-dev uthash-dev libconfig-dev
	
### Checkout vdsm sources

	cd ${DSROOT}

	# the vdsm itself
	git clone https://git.digitalstrom.org/virtual-devices/vdsm.git

	# parts of the digitalstrom stack needed by vdsm
	git clone https://git.digitalstrom.org/ds485-stack/ds485-core.git
	git clone https://git.digitalstrom.org/ds485-stack/libdsuid.git
	git clone https://git.digitalstrom.org/ds485-stack/ds485-netlib.git
	git clone https://git.digitalstrom.org/ds485-stack/ds485-client.git
	git clone https://git.digitalstrom.org/ds485-stack/dsm-api.git


### Build vdsm and required parts of ds485 stack

	cd ${DSROOT}

	# first build libraries vdsm depends on

	pushd libdsuid
	autoreconf -i
	./configure
	make
	$SUPER make install
	popd

	pushd ds485-core
	autoreconf -i
	./configure
	make
	$SUPER make install
	popd

	pushd ds485-netlib
	autoreconf -i
	./configure
	make
	$SUPER make install
	popd

	pushd ds485-client
	autoreconf -i
	./configure
	make
	$SUPER make install
	popd

	pushd dsm-api
	cmake .
	make
	$SUPER make install
	popd


	# now the vdsm itself

	pushd vdsm
	autoreconf -i
	./configure
	make
	popd
	
### quick test vdsm
	
	# vdsm needs libs that are in /usr/local/lib, which for some reason is not always
	# in the library load path (altough defined in /etc/ld.so.conf.d for libc).
	# So, just run ldconfig once to make sure lib path config is up to date.
	ldconfig
	
	src/vdSM -h
	
Should output the usage text explaining the vdsm command line options.


## Run vdcd (and vdsm if needed) for experimenting

**Note:** on a P44-DSB-X, there's a vdcd and vdsm already running. To test the self-built versions, you need to shut down these first:

	# to test your own vdcd:
	sv stop vdcd
	
	# to test your own vdsm (usually, you should be fine
	# testing with the built-in vdsm or an external master vdSM
	sv stop vdsm

For running either vdsm or vdcd, we need a directory for persistent data (sqlite3 databases) storage:

	mkdir ~/ds/data
		
Now everything is ready to actually run a vdcd with virtual devices, have the vdsm connect it and get connected into a digitalSTROM system on the same LAN.

### vdcd

Start a vdcd, with some command line options to create a console button and a console dimmer simulation device.
	
	~/ds/vdcd/vdcd --sqlitedir ~/ds/data --consoleio testbutton:button --consoleio testlamp:dimmer
	
If you want to explore the vdcd properties using JSON also enable the config api (with *--cfgapiport 8090*). On the P44-DSB-X, the mg44 webserver already handles forwarding http request to the socket based JSON API. On other platforms, see *json\_api\_forwarder* folder for a small PHP script for that task.

#### vdcd external devices API

vdcd supports "external devices", which are external scripts or programs that connect to the vdcd to instantiate devices, and communicated via the *plan44 vdcd external device API*. This API is a very easy to use API designed to simplify development of custom devices.

To start vdcd with external devices support enabled (API at TCP port 8999), call vdcd as follows:

    ~/ds/vdcd/vdcd --sqlitedir ~/ds/data --externaldevices 8999
    
To allow external device scripts/programs from other hosts than where vdcd runs (not recommended for security reasons in productive installations, but handy for development and testing), add the *--externalnonlocal* command line option.

For more information about the external devices API, please consult the *plan44 vdcd external device API.pdf* document in the *docs* folder, and check out the sample scripts in the *external\_devices\_samples* folder.


#### vdc API

See [www.digitalstrom.org/allianz/entwickler/architekturdokumente](https://www.digitalstrom.org/allianz/entwickler/architekturdokumente/) for the detailed vDC API specs.

Note that while the real vDC API is a protobuf API, the vdcd also exposes the same API as JSON (to be able to create web interfaces like P44-DSB's). For using the same functionality as described in the specs you need to use POST (or direct TCP socket connection) to send a JSON object containing either *method=\<methodname\>* or *notification=\<notificationname\>*, plus any parameter as described in the specs.

If you start the vdcd with -l 7 (full logging), you can watch the vdsm talking to the vdcd and see how it works - the requests and answers are then shown in a pseudo-JSON (no quotes) form.

### vdsm (if not on a P44-DSB-X with vdsm already running)

(preferably in a second console window, to see both program's outputs)

The vdsm needs to have a dSUID of its own (-m option). On P44-DSB-X, the p44maintd tool is installed and must be used to generate a stable unique dSUID for a vdsm on that particular RPi (so it matches the dSUID automatically announced via avahi on P44-DSB-X):

	VDSMID=`/usr/bin/p44maintd --vdsmdsuid`
	
p44maintd is not available on a standard Raspian or Ubuntu. But standard UUIDs are a valid option for dSUIDs (when a 00 is appended), so we can just use a generated UUID, slightly reformatted:

	VDSMID=`python -c 'import uuid; print str(uuid.uuid1()) + "00"' | sed -r -e "s/-//g"`

Now, start the vdsm with output to the console

	~/ds/vdsm/src/vdSM -B -b 8441 -s ~/ds/data/vdsm.db -m ${VDSMID} localhost:8340

The vdsm will start trying to connect the vdc host on port 8340 (which is the vdcd you started above, so you'll see some communication between the two immediately)
