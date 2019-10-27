'use strict';

var native = require('./native');

console.log("Simple adder test:")
console.log("2 + 3 = ", native.simple_add(2, 3));

// Some tests to check if C++ is working
console.log("C++ objects as global variables:")
console.log(native.object_heap_test() ? "working" : "not working");

console.log("C++ objects new/delete:")
console.log(native.new_test() ? "working" : "not working");

/*
 * Not working yet!

console.log("C++ objects destructing while throw:");
try {
    native.unwind_stack_test_do_unwind();
    console.log("unwind_stack_test_do_unwind did not throw");
} catch(i) {
    if(i != 123)
        console.log("catch called with wrong value", i);
}
console.log(native.unwind_stack_test() ? "working" : "not working");
*/
