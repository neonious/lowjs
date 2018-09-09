var fs = require('fs');
fs.open("/proc/notallowedtowritehere", "w", function (err) { console.log(err); console.log(JSON.stringify(err, null, 4)); });