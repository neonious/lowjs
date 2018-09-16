// curl -k https://localhost:8000/
var https = require('https');
var http = require('http');
var fs = require('fs');
var tls = require('tls');
var path = require('path');

var options = {
    key: fs.readFileSync(path.join(__dirname, 'server.key')),
    cert: fs.readFileSync(path.join(__dirname, 'server.crt'))
};
//console.log(JSON.stringify(options, null, 2));

https.createServer(options, function (req, res) {
    res.writeHead(200);
    res.end('hello world\n');
}).listen(8000);

var a = http.createServer(function (req, res) {
    res.writeHead(200);
    res.end('hello world\n');
});
a.on('error', function () { console.log("err handler"); });
a.listen(8001);
