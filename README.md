IMPORTANT: This repository neither includes the ESP32 internals nor the neonious one IDE. Thus, most development happens outside of the repository. Thus, see the [Change Log](https://www.neonious.com/documentation/changelog) for more activity.

# lowjs

low.js is a port of Node.JS with far lower system requirements.

+ low.js starts up instantly, while Node.js does not (0.6 to 1.5 seconds wait time on Raspberry Pi 2).

+ low.js only uses a fraction of the disk space and memory which Node.js requires, leaving more resources for other processes.

+ With low.js, you can program JavaScript applications utilizing the full Node.js API and run them not only on normal PCs, but also on embedded devices, such as ones based on the $3 ESP32 microcontroller with Wifi on-board.

For more information on low.js, please visit http://www.lowjs.org/ .


## First steps / documentation

To try out low.js on a PC, try out the [webserver example](https://github.com/neonious/lowjs/tree/master/examples/webserver) in the repository, which you can run with both low.js and Node.js.

To try out low.js on a ESP32-WROVER, try out the [examples on our homepage](https://www.lowjs.org/examples/getting-started.html) on your own board.

The documentation is located at [lowjs documentation](https://www.neonious.com/Documentation/lowjs).


## Compile from source

In case the binary distributions available at http://www.lowjs.org/ do not work for your needs,  you can compile from source.

Before compiling, make sure you have the following software installed:

    make g++
    automake autoconf libtool
    python py-pip
    nodejs
    npm (if not already installed together with nodejs)

Note: Node.js is only used to transpile the low.js libs from ES6 to ES5. It is only need for the build process. The binary distributions of low.js available on http://www.lowjs.org/ do not include Node.js, of course - this would defeat the purpose.

With this software installed, please install pyyaml with pip:

    pip install pyyaml

The compilation itself can be done with these commands:

    git clone --recurse-submodules https://github.com/neonious/lowjs
    cd lowjs
    make

low.js is now built in bin and can be called via bin/low.

The lib directory is also required for low to work (it accesses it via path_to_bin/../lib) and must be copied when creating a distribution. When creating a distribution, it might also make sense to strip the binary with the strip command, to save some KB.


## Running tests

Please see the [lowjs test documentation](https://github.com/neonious/lowjs/blob/master/test/README.md).


## License / Authors / Contributions

We appreciate every person or company who is willing to contribute to low.js and its related products. We will gladly accept any code contribution which helps the cause after an appropriate review. Bug reports and suggestions are also welcome!

low.js, with exception of the adaption for ESP32, is placed under the MIT license, allowing you to use low.js commercially and even modify it. The adaption of low.js for ESP32 is not Open Source, but may be used freely with the lowsync flasher. low.js is maintained by neonious GmbH, the makers of the neonious one microcontroller board.


### Contact us

    neonious GmbH
    Münsterstr. 246
    40470 Düsseldorf
    Germany
    
    Managing Director / CEO
    Thomas Rogg
    
    TEL +49 211 9241 8187
    FAX +49 211 9241 8172
    
    info@neonious.com
    District Court Düsseldorf
    HRB 83086

Node.js is a trademark of Joyent, Inc. (https://www.nodejs.org/). ESP32 and ESP32-WROVER are products by Espressif Systems (https://www.espressif.com/). neonious GmbH is in no way affiliated with these companies.
