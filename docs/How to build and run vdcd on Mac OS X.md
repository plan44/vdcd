# How to build and run the vdcd in XCode on OS X
As an Apple user, I'm using XCode as my primary development tool. Not only ObjC, but most of my C/C++ software starts there under Clang/LLVM, and only afterwards gets ported to the various embedded targets (usually Linux with older gcc).

Here's what you need to do if you want to build vdcd in XCode on OS X

## Prepare

### install XCode 7.x
Just from Mac App Store or see [https://developer.apple.com/xcode/](https://developer.apple.com/xcode/)

### install (home)brew
Go to [http://brew.sh](http://brew.sh) and follow instructions

### install needed packages from brew

	brew install json-c
	brew install protobuf-c
	brew install boost
	brew install ola

### go to your projects folder

	cd <your projects dir>

vdcd will be created as a subfolder of this folder


### clone the git repositories (and the p44utils/p44vdc submodules)

	git clone https://github.com/plan44/vdcd.git
	git clone https://github.com/plan44/libmongoose.git
	git clone https://github.com/plan44/libavahi-core.git
	cd vdcd
	git submodule init
	git submodule update
	
Note: Newer json-c versions put their headers into "json-c", whereas older ones put them into "json" (in /usr/local/include). Easiest is to set a link from one to the other:

	cd /usr/local/include
	ln -s json-c json


## Build

- open the vdcd.xcodeproj and build it (cmd-B)

- configure the arguments in XCode (Product->Scheme->Edit Scheme..., under "Run" -> "Arguments". If you don't know what arguments you need, just try --help to get a summary.

A possible vdcd command line could be:

	vdcd --cfgapiport 8090 -l 7 --consoleio testbutton:button --consoleio testlamp:dimmer

## Run

Now you have a vdcd running, which can accept JSON queries on port 8090. Note that this is not a http server, you need to open socket connections to send JSON requests (LF delimited).

Check out the small api.php script in the *json\_api\_forwarder* folder if you want a http frontend for the API.

Assuming you have api.php installed at *http://localhost/api.php*, you can do the following from a browser:

### list the entire property tree
	http://localhost/api.php/vdc?method=getProperty&dSUID=root&name=%20

### list of all devices of a specific devices class

The static device class containes the devices statically created at vdcd startup through options on the command line (or via Web UI).

	http://localhost/api.php/vdc?method=getProperty&dsuid=root&x-p44-itemSpec=vdc:Static_Device_Container&name=%20
	
### list of all properties of a device by dSUID
	http://localhost/api.php/vdc?method=getProperty&name=%20&dSUID=9ADC13F7D59E5B0280EC4E22E273FA0600


## vdcd external devices API

vdcd supports "external devices", which are external scripts or programs that connect to the vdcd to instantiate devices, and communicated via the *plan44 vdcd external device API*. This API is a very easy to use API designed to simplify development of custom devices.

To start vdcd with external devices support enabled (API at TCP port 8999), call vdcd as follows:

    vdcd --externaldevices 8999
    
To allow external device scripts/programs from other hosts than your Mac where vdcd runs, add the *--externalnonlocal* command line option.

For more information about the external devices API, please consult the *plan44 vdcd external device API.pdf* document in the *docs* folder, and check out the sample scripts in the *external\_devices\_samples* folder.


## vdc API

See [www.digitalstrom.org/allianz/entwickler/architekturdokumente](https://www.digitalstrom.org/allianz/entwickler/architekturdokumente/) for the detailed vDC API specs.

Note that while the real vDC API is a protobuf API, the vdcd also exposes the same API as a JSON. The examples above use a simplified GET variant, for using the same functionality as described in the specs you need to use POST (or direct TCP socket connection) to send a JSON object containing either *method=\<methodname\>* or *notification=\<notificationname\>*, plus any parameter as described in the specs.
