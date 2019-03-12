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
    res.setHeader("Content-Type", contentType);

    console.log("streaming " + wwwPath + url);

    var stream = fs.createReadStream(wwwPath + url);
    stream.on('error', function (err) {
        res.statusCode = 404;
        res.end(err.message);
    });
    stream.pipe(res);
}

var httpServer = http.createServer(handleRequest).listen(0, function (err) {
    if (!err)
        console.log('listening on http://localhost:' + httpServer.address().port + '/');
});

var options = {
    key: fs.readFileSync(path.join(__dirname, 'server.key')),
    cert: fs.readFileSync(path.join(__dirname, 'server.crt'))
};
var httpsServer = https.createServer(options, handleRequest).listen(0, function (err) {
    if (!err)
        console.log('listening on https://localhost:' + httpsServer.address().port + '/ with self-signed certificate (warning is OK)');
});


// ***** WebSocket server for chat
// remove this code to remove chat functionality

var WebSocket = require('ws');
var wss = new WebSocket.Server({ noServer: true });

wss.on('connection', function connection(ws) {
    ws.on('message', function incoming(data) {
        console.log("broadcasting message: " + data);
        wss.clients.forEach(function each(client) {
            if (client !== ws && client.readyState === WebSocket.OPEN)
                client.send(data);
        });
    });
});
function upgradeToWSS(req, socket, head) {
    if (req.url === '/Chat') {
        console.log("webbrowser connected to chat");

        wss.handleUpgrade(req, socket, head, function done(ws) {
            wss.emit('connection', ws, req);
        });
    } else
        socket.destroy();
}

httpServer.on('upgrade', upgradeToWSS);
httpsServer.on('upgrade', upgradeToWSS);
