// Extracts root certs from Node.JS, to make sure low.js uses the same root certs than Node.JS does
// Requires Node > 12.3

const path = require('path');
const fs = require('fs');
const tls = require('tls');

// Certs are joined to 1 string, allows fast path in low_tls.cpp
let certs = tls.rootCertificates.join('\n').replace(new RegExp('\n\n', 'g'), '\n');

fs.writeFileSync(path.join(__dirname, 'root-certs.json'), JSON.stringify([certs]));
