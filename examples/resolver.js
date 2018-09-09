var dns = require('dns');

console.log(JSON.stringify(dns.getServers()));
dns.reverse('136.243.111.27', function (err, addresses) {
    console.log(JSON.stringify(addresses, null, 4));
});
