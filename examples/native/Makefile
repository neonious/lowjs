UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
error:
        @echo "Cannot compile on Mac OS X as Mac OS X cannot build ELF files. Please compile on Linux."
        @echo "The resulting native modules can then be used under Mac OS X, too!"
endif

FLAGS = -O3 -fPIC

C = gcc
CFLAGS = $(FLAGS) -fvisibility=hidden -fno-stack-protector -Ilow_native_api/include

LD = gcc
LDFLAGS = $(FLAGS) -shared -Llow_native_api/lib -llow_native

LD_ESP32 = ~/esp/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc
LDFLAGS_ESP32 = $(FLAGS) -shared -Llow_native_api/lib -llow_native.esp32

OBJECTS =	\
	src/native.o

all: src/native.so

esp32: src/native.esp32.so

clean:
	rm -f src/native.so src/native.esp32.so */*.o */*.d

src/native.so: $(OBJECTS) Makefile
	$(LD) -o src/native.so $(OBJECTS) $(LDFLAGS)

src/native.esp32.so: $(OBJECTS) Makefile
	$(LD_ESP32) -o src/native.so $(OBJECTS) $(LDFLAGS)

%.o : %.c Makefile
	$(C) $(CFLAGS) -MMD -o $@ -c $<

-include $(OBJECTS:.o=.d) $(OBJECTS_LOW:.o=.d)
