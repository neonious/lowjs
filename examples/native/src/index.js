'use strict';

var file = require('path').join(__dirname, 'native.so');

console.log('Loading', file);

var native_adder = require('native-api').loadSync(file);

console.log('Loaded, calling');

console.log("2 + 3 = ", native_adder.add(2, 3));
