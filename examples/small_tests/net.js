/*
 * From: https://gist.github.com/tedmiston/5935757
 * 
 * Expected output:
 * 
 * Connected
 * Received: Echo server
 * Hello, server! Love, Client.
 * Connection closed
 */

/*
In the node.js intro tutorial (http://nodejs.org/), they show a basic tcp 
server, but for some reason omit a client connecting to it.  I added an 
example at the bottom.
Save the following server in example.js:
*/

var net = require('net');

var server = net.createServer(function (socket) {
    socket.setTimeout(3000);
    socket.on('close', function () {
        console.log('Server conn closed');
    });
    console.log("At server connected " + JSON.stringify(socket.address()));
    socket.write('Echo server\n');
    socket.pipe(socket);
    socket.on('timeout', function () {
        console.log("SERVER CLOSE");
        socket.destroy();
        server.close();
    });
});

server.listen(1337, '127.0.0.1');


/*
And connect with a tcp client from the command line using netcat, the *nix 
utility for reading and writing across tcp/udp network connections.  I've only 
used it for debugging myself.
$ netcat 127.0.0.1 1337
You should see:
> Echo server
*/

/* Or use this example tcp client written in node.js.  (Originated with 
example code from 
http://www.hacksparrow.com/tcp-socket-programming-in-node-js.html.) */

var net = require('net');

var client = new net.Socket();
client.connect(1337, '127.0.0.1', function () {
    console.log("At client connected " + JSON.stringify(client.address()));
    client.write('Hello, server! Love, Client.');
});

client.on('data', function (data) {
    console.log('Received: ' + data);
});

client.on('close', function () {
    console.log('Connection closed');
});

client.setTimeout(1000);
client.on('timeout', function () {
    client.destroy();
});
