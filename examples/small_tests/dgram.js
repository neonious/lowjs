const dgram = require('dgram');
const server = dgram.createSocket('udp4');
const server2 = dgram.createSocket('udp4');

server.on('error', function(err) {
  console.log('server error:', err);
  server.close();
});

server.on('message', function(msg, rinfo) {
  console.log('server got', msg.toString(), 'from', rinfo);
  server.close();
});

server.on('listening', function() {
  const address = server.address();
  console.log('server listening', address);
  server2.send('hello, world', 41234, function(err) {
    if(err)
      console.log('server2 error:', err);
    server2.close();
  });
});

server.bind(41234, 'localhost');
// server listening 0.0.0.0:41234
