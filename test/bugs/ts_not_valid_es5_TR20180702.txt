"use strict";

/*
 * This broke some Node.JS library code:
 * The following valid ES6 is not transpiled to valid ES5
 * with TypeScript (it keeps the functions as they are, but
 * ES5 strict mode forbids functions at the top of ifs, breaks Duktape)
 * Babel does it correctly, however.
 * Will not be fixed soon according to TS issues.
 */

if(a()) {
	function b() {
		console.log("X");
	}
	b();
} else {
	function b() {
		console.log("X");
	}
	b();
}
