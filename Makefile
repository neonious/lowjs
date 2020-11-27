FLAGS = -O3 -DLOW_VERSION="\"`git show -s --format=%cd --date=format:%Y%m%d`_`git rev-parse --short HEAD`\""

C = gcc
CFLAGS = $(FLAGS) -Isrc -Iapp -Ideps/duktape/src-low -Ideps/mbedtls/include -Ideps/mbedtls/crypto/include

CXX = g++
CXXFLAGS = $(CXXFLAGS_SERV) $(FLAGS) -Isrc -Iapp -Ideps/duktape/src-low -Ideps/mbedtls/include -Ideps/mbedtls/crypto/include -Ideps/open62541/build/src_generated -Ideps/open62541/include -Ideps/open62541/arch -Ideps/open62541/plugins/include -Ideps/open62541/src/client -Ideps/open62541/deps -Ideps/open62541/src --std=c++11

LD = g++
LDFLAGS = $(FLAGS) -lm -ldl -lpthread deps/open62541/build/bin/libopen62541.a -lresolv

OBJECTS_LOW =						\
	app/main.o						\
	app/transpile.o
OBJECTS =							\
	deps/duktape/src-low/duktape.o		\
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
	src/low_dgram.o					\
	src/LowDatagram.o					\
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
	src/LowFD.o					\
	src/LowHTTPDirect.o				\
	src/LowSignalHandler.o			\
	src/LowDNSWorker.o				\
	src/LowDNSResolver.o			\
	src/LowTLSContext.o				\
	src/low_native_api.o			\
	src/low_promise.o				\
	src/low_opcua.o

all: bin/low lib/BUILT

clean:
	rm -rf */*.o */*.d bin/* deps/duktape/src-low lib lib_js/build node_modules util/dukc test/duk_crash
	cd deps/c-ares && make clean
	cd deps/mbedtls && make clean
	cd deps/open62541 && rm -rf build
	rm deps/c-ares/configure

bin/low: $(OBJECTS) $(OBJECTS_LOW) deps/mbedtls/programs/test/benchmark
	mkdir -p bin
	 $(LD) -o bin/low deps/mbedtls/library/*.o deps/mbedtls/crypto/library/*.o deps/c-ares/libcares_la-*.o $(OBJECTS) $(OBJECTS_LOW) $(LDFLAGS)

obj_lowjs_serv: $(OBJECTS) $(OBJECTS_LOW) deps/mbedtls/programs/test/benchmark util/dukc

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
%.o : %.cpp Makefile deps/c-ares/.libs/libcares.a deps/open62541/build/bin/libopen62541.a
	$(CXX) $(CXXFLAGS) -MMD -o $@ -c $<

-include $(OBJECTS:.o=.d) $(OBJECTS_LOW:.o=.d)

lib/BUILT: util/dukc node_modules/BUILT $(shell find lib_js) util/root-certs.json
	rm -rf lib lib_js/build
	cd lib_js && node ../node_modules/typescript/bin/tsc
	cp node_modules/\@babel/standalone/babel.min.js lib_js/build/babel.js
	mkdir lib
	util/dukc lib_js/build lib
	cp util/root-certs.json lib/internal
	touch lib/BUILT
node_modules/BUILT: package.json
	npm install
	touch node_modules/BUILT

deps/duktape/src-low/duktape.c: $(shell find deps/duktape/src-input)
	rm -rf deps/duktape/src-low
	cd deps/duktape && python tools/configure.py --output-directory=src-low	\
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
		-DDUK_USE_SECTION_B \
		-DDUK_USE_CPP_EXCEPTIONS

deps/c-ares/configure:
	cd deps/c-ares && . ./buildconf
deps/c-ares/Makefile: deps/c-ares/configure
	cd deps/c-ares && ./configure
deps/c-ares/.libs/libcares.a: deps/c-ares/Makefile
	cd deps/c-ares && make

deps/mbedtls/programs/test/benchmark:
	cd deps/mbedtls && make

deps/open62541/build/bin/libopen62541.a:
	cd deps/open62541 && rm -rf build && mkdir build
	cd deps/open62541/build && cmake ..
	cd deps/open62541/build && make

# Builds distribution
dist: all
	chmod 755 util/dist-build.sh
	util/dist-build.sh
