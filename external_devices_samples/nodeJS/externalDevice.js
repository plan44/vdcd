var net = require('net');

var client = new net.Socket();
client.connect(8999, '127.0.0.1', function() {
	console.log('Connected');
	// send init message
  var initMessage = {
    "message":"init",
    "output":"light",
    "uniqueid":"externalDeviceNodeJSSample",
    "buttons":[
       {"buttontype":1,"hardwarename":"push","element":0} // a single push-button
    ]
  };
	client.write(JSON.stringify(initMessage)+"\n");
	// prepare stdin
  process.stdin.setEncoding('utf8');
  process.stdin.setRawMode(true);
  process.stdin.on('readable', function() {
    var key = process.stdin.read(1); // one char at a time
    if (key == '\u0003') {
      process.exit();
    }
    if (key !== null) {
      console.log('key pressed: ' + key);
      var buttonMessage = {
        "message":"button",
        "index":0,
        "value":200 // 200mS click simulation
      };
    	client.write(JSON.stringify(buttonMessage)+"\n");
    }
  });
	console.log('Press any key to simulate button press, ^C to exit');
});


client.on('data', function(data) {
	console.log('Received: ' + data);
	var msg = JSON.parse(data);
	var messageType = msg.message;
	if (messageType == "channel") {
	  console.log('>>> channel[' + msg.index + '] set to ' + msg.value);
	}
});

client.on('close', function() {
	console.log('Connection closed');
});