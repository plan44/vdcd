## vdcd External Devices - nodeJS sample

This folder contains a sample node JS program implementing a basic light dimmer device with a button. The non-blocking I/O concept of nodeJS makes it especially simple to implement bidirectional communication with the vdcd external device API.

In the example, any console key press is translated into a 200mS button press, and any brightness channel changes are written to the console. Both input and output can be easily extended to interact with actual hardware devices, for example using GPIO libraries such as *pi-gpio* or *onoff*, or by communicating with devices using other IP based APIs.


### How to run

You need to have nodeJS installed to run the sample.

On Linux, you can install it with

    sudo apt-get install nodejs
    
On Mac OS X, if you have brew, you can install it with

    brew install nodejs

Once nodeJS is installed, you can run the sample with

	node externalDevice.js 
	
(assuming you have a vdcd with external device API enabled running on the same computer)
	
Note: the script does not restart the connection when it is closes (due to error or vdcd being restarted). So, for permanent operation, the script should be put under control of a daemon supervisor such as *runit* or the nodeJS specific *forever* tool.

### More information 

Please refer to the *plan44 vdcd external device API* PDF document in the *docs* folder for a full documentation of the external device API and its features.
