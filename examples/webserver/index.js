var http = require('http');
var https = require('https');
var fs = require('fs');
var tls = require('tls');
var path = require('path');

var wwwPath = path.join(__dirname, 'www');
function handleRequest(req, res) {
	var url = req.url == '/' ? '/index.html' : req.url;

	// add more as required...
	var contentType;
	if (url.substr(-4) == '.png')
		contentType = 'image/png';
	else if (url.substr(-4) == '.css')
		contentType = 'text/css';
	else
		contentType = 'text/html';

	console.log("streaming " + wwwPath + url);

	var stream = fs.createReadStream(wwwPath + url);
	stream.on('error', function(err) {
		res.statusCode = 404;
		res.end(err.message);
	});
	stream.pipe(res);
}

var httpServer = http.createServer(handleRequest).listen(0, function(err) {
	if(!err)
		console.log('listening on http://localhost:' + httpServer.address().port + '/');
});

var options = {
    key: fs.readFileSync(path.join(__dirname, 'server.key')),
    cert: fs.readFileSync(path.join(__dirname, 'server.crt'))
};
var httpsServer = https.createServer(options, handleRequest).listen(0, function(err) {
	if(!err)
		console.log('listening on https://localhost:' + httpsServer.address().port + '/ with self-signed certificate (warning is OK)');
});