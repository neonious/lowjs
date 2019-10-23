'use strict';

var path = require('path');
var native_api = require('native-api');

var file = path.join(__dirname, 'native.so');
var native_adder = native_api.loadSync(file);

console.log("2 + 3 = ", native_adder.add(2, 3));
