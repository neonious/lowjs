process.on('uncaughtException', function(err) {
	throw new Error("second layer, this is fatal");
});
throw new Error("first layer, should not be shown");
