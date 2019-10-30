UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
error:
	@echo "Cannot compile on Mac OS X as Mac OS X cannot build ELF files. Please compile on Linux."
	@echo "The resulting native modules can then be used under Mac OS X, too!"
endif

all: src/native.so
esp32: src/native.so

C = gcc
CXX = g++
LD = ld

FLAGS = -O3 -fPIC -fvisibility=hidden -fno-stack-protector -fno-builtin

CFLAGS = $(FLAGS) -Ilow_native_api/include
CXXFLAGS = $(FLAGS) -Ilow_native_api/include
LDFLAGS = $(FLAGS) -shared

esp32: C = ~/esp/xtensa-esp32-elf/bin/xtensa-esp32-elf-gcc
esp32: CXX = ~/esp/xtensa-esp32-elf/bin/xtensa-esp32-elf-g++
esp32: LD = ~/esp/xtensa-esp32-elf/bin/xtensa-esp32-elf-ld

esp32: CFLAGS = $(FLAGS) -Ilow_native_api/include -DDUK_F_32BIT_PTRS
esp32: CXXFLAGS = $(FLAGS) -Ilow_native_api/include -DDUK_F_32BIT_PTRS

OBJECTS =	\
	src/native.o

clean:
	rm -f src/native.so */*.o */*.d

src/native.so: $(OBJECTS) Makefile
	$(LD) -o src/native.so $(OBJECTS) $(LDFLAGS)

%.o : %.c Makefile
	$(C) $(CFLAGS) -MMD -o $@ -c $<

%.o : %.cpp Makefile
	$(CXX) $(CXXFLAGS) -MMD -o $@ -c $<

-include $(OBJECTS:.o=.d) $(OBJECTS_LOW:.o=.d)
