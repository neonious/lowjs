FLAGS = -O3 -g

C = gcc
CFLAGS = $(FLAGS) -Iinclude -Ideps/duktape/src-low -Ideps/mbedtls/include

CXX = g++
CXXFLAGS = $(FLAGS) -Iinclude -Iapp -Ideps/duktape/src-low -Ideps/mbedtls/include --std=c++11

LD = g++
LDFLAGS = $(FLAGS) -lm -lpthread

# So distribution, built under Alpine Linux/musl, runs everywhere
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	LDFLAGS += -static -static-libstdc++ -static-libgcc
endif

OBJECTS =							\
	deps/duktape/src-low/duktape.o		\
	app/main.o						\
	src/low_main.o					\
	src/low_module.o				\
	src/low_native.o				\
	src/low_native_aux.o			\
	src/low_process.o				\
	src/low_loop.o					\
	src/low_fs.o					\
	src/low_fs_misc.o					\
	src/low_http.o					\
	src/low_net.o					\
	src/low_tls.o					\
	src/low_dns.o					\
	src/low_crypto.o				\
	src/LowCryptoHash.o				\
	src/low_data_thread.o			\
	src/low_web_thread.o			\
	src/low_alloc.o					\
	src/low_system.o				\
	src/LowFile.o					\
	src/LowFSMisc.o					\
	src/LowServerSocket.o			\
	src/LowSocket.o					\
	src/LowHTTPDirect.o				\
	src/LowSignalHandler.o			\
	src/LowDNSWorker.o				\
	src/LowDNSResolver.o			\
	src/LowTLSContext.o

all: bin/low lib/BUILT

clean:
	rm -rf */*.o */*.d bin/* deps/duktape/src-low lib lib_js/build node_modules util/dukc test/duk_crash
	cd deps/c-ares && make clean
	cd deps/mbedtls && make clean

bin/low: $(OBJECTS) deps/mbedtls/programs/test/benchmark
	mkdir -p bin
	 $(LD) -o bin/low deps/mbedtls/library/*.o deps/c-ares/libcares_la-*.o $(OBJECTS) $(LDFLAGS)
util/dukc: deps/duktape/src-low/duktape.o util/dukc.o
	 $(LD) -o util/dukc deps/duktape/src-low/duktape.o util/dukc.o $(LDFLAGS)

test/bugs/duk_crash_TR20180627: deps/duktape/src-low/duktape.o test/bugs/duk_crash_TR20180627.o
	 $(LD) -o test/bugs/duk_crash_TR20180627 deps/duktape/src-low/duktape.o test/bugs/duk_crash_TR20180627.o $(LDFLAGS)
test/bugs/duk_crash_TR20180706: deps/duktape/src-low/duktape.o test/bugs/duk_crash_TR20180706.o
	 $(LD) -o test/bugs/duk_crash_TR20180706 deps/duktape/src-low/duktape.o test/bugs/duk_crash_TR20180706.o $(LDFLAGS)

# Force compilation as C++ so linking works
deps/duktape/src-low/duktape.o: deps/duktape/src-low/duktape.c Makefile
	$(CXX) $(CXXFLAGS) -MMD -o $@ -c $<
%.o : %.c Makefile
	$(C) $(CFLAGS) -MMD -o $@ -c $<
%.o : %.cpp Makefile deps/c-ares/libcares.la
	$(CXX) $(CXXFLAGS) -MMD -o $@ -c $<

-include $(OBJECTS:.o=.d)

lib/BUILT: util/dukc node_modules/BUILT $(shell find lib_js)
	rm -rf lib lib_js/build
	cd lib_js && node ../node_modules/typescript/bin/tsc
	mkdir lib
	./util/dukc lib_js/build lib
	touch lib/BUILT
node_modules/BUILT: package.json
	npm install
	touch node_modules/BUILT

deps/duktape/src-low/duktape.c: $(shell find deps/duktape/src-input)
	rm -rf deps/duktape/src-low
	cd deps/duktape && python tools/configure.py --output-directory=src-low	\
		-DDUK_USE_FATAL_HANDLER   \
		-DDUK_USE_GLOBAL_BUILTIN   \
		-DDUK_USE_BOOLEAN_BUILTIN   \
		-DDUK_USE_ARRAY_BUILTIN   \
		-DDUK_USE_OBJECT_BUILTIN   \
		-DDUK_USE_FUNCTION_BUILTIN   \
		-DDUK_USE_STRING_BUILTIN   \
		-DDUK_USE_NUMBER_BUILTIN   \
		-DDUK_USE_DATE_BUILTIN   \
		-DDUK_USE_REGEXP_SUPPORT   \
		-DDUK_USE_MATH_BUILTIN   \
		-DDUK_USE_JSON_BUILTIN   \
		-DDUK_USE_BUFFEROBJECT_SUPPORT   \
		-DDUK_USE_ENCODING_BUILTINS   \
		-DDUK_USE_PERFORMANCE_BUILTIN   \
		-DDUK_USE_OBJECT_BUILTIN    \
		-DDUK_USE_ES6_PROXY	\
		-DDUK_USE_GLOBAL_BINDING \
		-DDUK_USE_SYMBOL_BUILTIN	\
		-DDUK_USE_SECTION_B

deps/c-ares/configure:
	cd deps/c-ares && . ./buildconf
deps/c-ares/Makefile: deps/c-ares/configure
	cd deps/c-ares && ./configure
deps/c-ares/libcares.la: deps/c-ares/Makefile
	cd deps/c-ares && make

deps/mbedtls/programs/test/benchmark:
	cd deps/mbedtls && make

# Builds distribution
DIST_NAME=lowjs-`uname | tr A-Z a-z`-`uname -m`-`date +"%Y%m%d"`

dist: bin/low lib/BUILT
	rm -rf dist $(DIST_NAME) $(DIST_NAME).tar $(DIST_NAME).tar.gz
	mkdir $(DIST_NAME)
	cp -r bin lib LICENSE README.md $(DIST_NAME)
	strip $(DIST_NAME)/bin/low
	rm $(DIST_NAME)/lib/BUILT
	tar -c $(DIST_NAME) > $(DIST_NAME).tar
	gzip $(DIST_NAME).tar
	mkdir dist
	mv $(DIST_NAME).tar.gz dist
	rm -rf $(DIST_NAME) $(DIST_NAME).tar
