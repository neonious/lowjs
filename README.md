# lowjs
low.js is a port of Node.JS with far lower system requirements. With low.js, you can program JavaScript applications utilizing the full Node.JS API and run them not only on normal PCs, but also on embedded devices, such as ones based on the $ 3 ESP32 microcontroller with Wifi on-board.

It currently uses under 2 MB of disk space and requires a minimum of around 1.5 MB of RAM (ESP32 version).


## What for?

### Hobbyists

low.js allows you to use Node.JS scripts on smaller devices such as routers which are based on Linux or uClinux without having to use all resources. This is great for configuration scripts and more, especially if these scripts shall also communicate with servers online.

For people interested in electronics, the neonious one is a great microcontroller board based on low.js for ESP32. Programmable in JavaScript ES 6 with the Node API, through an on-board browser-based IDE + Debugger or external IDE. Includes Wifi, Ethernet, additional Flash and an additional I/O controller.

More information on the neonious one: http://www.neonious.com/.

### Professionals

low.js for ESP32 is a port of Node.JS with far lower system requirements, low enough to run comfortably on the ESP32-WROVER module. With unit costs of under 3 $ for large amounts, the ESP32-WROVER module is a very cost effective solution for any (IoT) device requiring both a microcontroller and a Wifi connection.

low.js for ESP32 adds the additional benefit of fast software development and maintenance. The complete software stack of an IoT product (microcontrollers, websites, servers) can be based on the same software base. No specialized software developers are needed for the microcontroller software.

All of this becomes especially important if additional features will be added throughout the products life time via software update, which helps the product to stay leaders in their markets over time.


## Where to get?

### Community Edition

The community edition of low.js runs on POSIX based systems such as Linux, uClinux or Mac OS X. It is available on Github. Currently there is no ./configure. You might need programming skills to get low.js running on your systems.

    git clone https://github.com/neonious/lowjs
    cd lowjs
    make

low.js is now built in bin and can be called via bin/low, the lib directory is also required for low to work (it accesses it via path_to_bin/../lib) and must be copied when creating a distribution.

### low.js for ESP32
The ESP32 edition is identical to the community edition, but adapted for the ESP32 microcontroller. This version is not Open Source.

It is pre-flashed on the neonious one and plain ESP32-WROVER modules which you can order from neonious (direct link to store).


## License / Authors / Contributions
The community edition is placed under the MIT license. More information.

low.js is developed and maintained by neonious GmbH, the makers of the neonious one microcontroller board.

We are very happy for every person or company who is willing to contribute to low.js and its related products. We will gladly accept any code contribution which helps the cause after an appropriate review. Bug reports and suggestions are also welcome!


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

ESP32 and ESP32-WROVER are products by Espressif Systems (https://www.espressif.com/). neonious GmbH is in no way affiliated with this company.
