var fs = require('fs');

var stream = fs.createWriteStream('test.txt');
stream.write('hello great world');

stream.end(function() {
	var fd = fs.openSync('test.txt', 'r');
	fs.readSync(fd, new Buffer(6), 0, 6, null);

	var stream2 = fs.createReadStream('test.txt',
		{ fd: fd, end: 11 });
	stream2.on('data', function(chunk) {
		// Should only output 'great', but outputs 'great world' with Node.JS
		// We guess end is only allowed with start, but it does not say so in the docs,
		// so we allow it.
		console.log(chunk.toString());
	});
});
