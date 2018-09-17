var http = require('http');

http.get('http://www.lowjs.org', function (res) {
	var statusCode = res.statusCode;

	var error;
	if (statusCode !== 200) {
		error = new Error('Request Failed.\n' +
			'Status Code:' + statusCode);
	}
	if (error) {
		console.error(error.message);
		// consume response data to free up memory
		res.resume();
		return;
	}

	var contentType = res.headers['content-type'];
	console.log("Content-Type: " + contentType);

	res.setEncoding('utf8');
	var rawData = '';
	res.on('data', function (chunk) { rawData += chunk; });
	res.on('end', function () {
		try {
			console.log(rawData);
		} catch (e) {
			console.error(e.message);
		}
	});
}).on('error', function (e) {
	console.error('Got error: ' + e.message);
});