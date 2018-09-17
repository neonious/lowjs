const dns = require('dns');
const options = {
};
dns.lookup('google.com', options, function (err, address, family) {
    console.log('address: %j family: IPv%s', address, family)
});
// address: "2606:2800:220:1:248:1893:25c8:1946" family: IPv6

// When options.all is true, the result will be an Array.
options.all = true;
dns.lookup('google.com', options, function (err, addresses) {
    console.log('addresses: %j', addresses);
    // addresses: [{"address":"2606:2800:220:1:248:1893:25c8:1946","family":6}]
});

dns.lookupService('127.0.0.1', 22, function (err, hostname, service) {
    console.log("lookupService: ", hostname, service);
    // Prints: localhost ssh
});

dns.resolve4('www.lowjs.org', function(err, res) {
	console.log("RESOLVE " + JSON.stringify(res));
});
