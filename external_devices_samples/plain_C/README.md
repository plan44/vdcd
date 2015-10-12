## vdcd External Devices - C sample

This folder contains a sample C program for an external button. It is a simplistic implementation to keep the sample short - it only sends data to the vdcd and does not receive responses. A more complete implementation would use select() or poll() to handle multiple I/O streams (stdin and vdcd socket) without blocking.

### How to compile and run

On any computer with a standard C compiler installed you should be able to compile the sample with

    cc -o externalButton externalButton.c
        
and then run it with

	./externalButton 127.0.0.1 8999
	
(assuming you have a vdcd with external device API enabled running on the same computer)
	
If you are on Mac OS X and have XCode installed, you can also open the *externalButton.xcodeproj* to build and run the sample in XCode.
	
Note: the program does not restart the connection when it is closes (due to error or vdcd being restarted). So, for permanent operation, the program should be put under control of a daemon supervisor such as *runit* or *launchd* to make sure the program is restarted automatically.

### More information 

Please refer to the *plan44 vdcd external device API* PDF document in the *docs* folder for a full documentation of the external device API and its features.
