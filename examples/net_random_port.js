var net = require('net');

var srv = net.createServer(function (sock) {
    sock.end('Hello world\n');
});
srv.listen(0, function () {
    console.log('Listening on port ' + srv.address().port);
});