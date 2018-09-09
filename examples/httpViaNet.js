var net = require('net');
var client = new net.Socket();

client.connect(80, 'www.lowjs.org', function () {
    console.log('Connected');
    client.write("GET / HTTP/1.0\r\nHost: www.lowjs.org\r\n\r\n");
});
client.on('data', function (data) {
    console.log('Received: ' + data);
});
client.on('close', function () {
    console.log('Connection closed');
});
client.on('lookup', function (err, address, family, host) {
    console.log('Looked up host err=', err, address, family, host);
});