UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)

error:
	echo "Cannot compile on Mac OS X as Mac OS X cannot build ELF files. Please compile on Linux."

endif

all: src/native.so

esp32: src/native.esp32.so

clean:
	rm -f src/native.so src/native.esp32.so

src/native.so: src/native.c
	gcc -shared -fPIC -o src/native.so src/native.c -Ilow_native_api/include -Llow_native_api/lib -llow_native -fvisibility=hidden

src/native.esp32.so: src/native.c
	~/esp/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc -shared -fPIC -o src/native.so src/native.c -Ilow_native_api/include -Llow_native_api/lib -llow_native.esp32 -fvisibility=hidden -nostdlib
