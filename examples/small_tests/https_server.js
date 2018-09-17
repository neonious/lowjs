var https = require('https');
var fs = require('fs');
var tls = require('tls');
var path = require('path');

var options = {
    key: fs.readFileSync(path.join(__dirname, 'server.key')),
    cert: fs.readFileSync(path.join(__dirname, 'server.crt'))
};

var a = https.createServer(options, function (req, res) {
    res.writeHead(200);
    res.end('hello world\n');
});
a.on('error', function () { console.log("err handler"); });
a.listen(3001, function () {
    console.log("listening on port 3001");
});
