var assert = require('assert');
var stream = require('stream');
var net = require('net');
function AClass() {
}
assert((new stream.Duplex) instanceof stream.Readable, "Duplex not instance of Readable");
assert((new stream.Duplex) instanceof stream.Writable, "Duplex not instance of Writable");
assert((new net.Socket) instanceof stream.Readable, "Socket not instance of Readable");
assert((new net.Socket) instanceof stream.Writable, "Socket not instance of Writable");
assert(!((new AClass) instanceof stream.Readable), "AClass instance of Readable");
assert(!((new AClass) instanceof stream.Writable), "AClass instance of Writable");
