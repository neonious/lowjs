'use strict';

var assert = require('assert').strict;
assert.throws(function() { throw new Error('foo'); }, { bar: true });
