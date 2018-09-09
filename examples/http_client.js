var http = require('http');

http.globalAgent.maxSockets = 1;

setInterval(function () {
    http.get('http://localhost:8123/dist/index.json', function (res) {
        //http.get('http://localhost:8000/dist/index.json', function (res) {
        var statusCode = res.statusCode;
        var contentType = res.headers['content-type'];
        console.log("IN RES !!! " + res.statusCode + " -- " + res.statusMessage + " !***************");

        var error;
        if (statusCode !== 200) {
            error = new Error('Request Failed.\n' +
                'Status Code:' + statusCode);
        } else if (!/^application\/json/.test(contentType)) {
            error = new Error('Invalid content-type.\n' +
                'Expected application/json but received ' + contentType);
        }
        if (error) {
            console.error(error.message);
            // consume response data to free up memory
            res.resume();
            return;
        }

        res.setEncoding('utf8');
        var rawData = '';
        res.on('data', function (chunk) { rawData += chunk; });
        res.on('end', function () {
            try {
                var parsedData = JSON.parse(rawData);
                console.log(parsedData);
            } catch (e) {
                console.error(e.message);
            }
        });
    }).on('error', function (e) {
        console.error('Got error: ' + e.message);
    });
}, 5000);