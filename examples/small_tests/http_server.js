// content of index.js
var http = require('http')
var port = 3000
var nn = 0
var requestHandler = function (request, response) {
    request.on('data', function (chunk) {
        console.log("DATA " + new Buffer(chunk).toString());
    });

    console.log(request.url);
    response.end('Hello Node.js Server ! ' + (nn++));
}

var server = http.createServer(requestHandler)

server.listen(port, function (err) {
    if (err) {
        return console.log('something bad happened', err)
    }

    console.log('server is listening on ' + port)
})
