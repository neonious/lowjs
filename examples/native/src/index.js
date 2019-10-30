'use strict';

var native = require('./native');
if(native != require('./native'))
	console.log("not cached");

console.log("Simple adder test:")
console.log("2 + 3 = ", native.simple_add(2, 3));

// Some tests to check if C++ is working
console.log("C++ objects as global variables:")
console.log(native.object_heap_test() ? "working" : "not working");

console.log("C++ objects new/delete:")
console.log(native.new_test() ? "working" : "not working");

console.log("C++ objects destructing while throw:");
try {
    native.unwind_stack_test_do_unwind();
    console.log("unwind_stack_test_do_unwind did not throw");
} catch(i) {
    if(i != 123)
        console.log("unwind_stack_test_do_unwind: catch called with wrong value", i);
}
console.log(native.unwind_stack_test() ? "working" : "not working");

console.log("Throw tests...");
try {
    native.throw_duk_test();
    console.log("throw_duk_test did not throw");
} catch(err) {
    if(!err || err.message != "throw test")
        console.log("throw_duk_test: catch called with wrong value", err);
}
try {
    native.throw_exception_test();
    console.log("throw_exception_test did not throw");
} catch(err) {
    if(!err || !err.message || err.message.indexOf("std::exception") == -1)
        console.log("throw_exception_test: catch called with wrong value", err);
}
try {
    native.throw_something_test();
    console.log("throw_something_test did not throw");
} catch(err) {
    if(!err || !err.message || err.message.indexOf("std::exception") != -1)
        console.log("throw_something_test: catch called with wrong value", err);
}
console.log(native.throw_test() ? "working" : "not working");

console.log("Done.");
