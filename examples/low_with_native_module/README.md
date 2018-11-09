Simple example on how to add native modules to low.js based applications.

In index.js the module "native_adder" is require()d, which is implemented in native_adder.c.


Run

    make bin/low_with_native_module

from lowjs directory to compile

Call

    bin/low_with_native_module examples/low_with_native_module/index.js

to run example