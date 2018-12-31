cmake_minimum_required(VERSION 3.12)
project(lowjs)

if (UNIX AND NOT APPLE)
  set(BUILD_SHARED_LIBS OFF)
endif ()


find_package(Threads REQUIRED)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif ()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_C_FLAGS "-Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_C_FLAGS_RELEASE "-O3")

set(DEP_DUK ${PROJECT_SOURCE_DIR}/deps/duktape)
set(DEP_ARES ${PROJECT_SOURCE_DIR}/deps/c-ares)
set(DEP_MBED ${PROJECT_SOURCE_DIR}/deps/mbedtls)

set(ENABLE_TESTING OFF)
set(CARES_STATIC ON)
set(CARES_SHARED OFF)
add_subdirectory(${DEP_MBED})
add_subdirectory(${DEP_ARES})

set(MBEDTLS_INCLUDE_DIRS ${DEP_MBED}/include)

set(DUK_BUILD_FLAGS
  "-DDUK_USE_FATAL_HANDLER"
  "-DDUK_USE_GLOBAL_BUILTIN"
  "-DDUK_USE_BOOLEAN_BUILTIN"
  "-DDUK_USE_ARRAY_BUILTIN"
  "-DDUK_USE_OBJECT_BUILTIN"
  "-DDUK_USE_FUNCTION_BUILTIN"
  "-DDUK_USE_STRING_BUILTIN"
  "-DDUK_USE_NUMBER_BUILTIN"
  "-DDUK_USE_DATE_BUILTIN"
  "-DDUK_USE_REGEXP_SUPPORT"
  "-DDUK_USE_MATH_BUILTIN"
  "-DDUK_USE_JSON_BUILTIN"
  "-DDUK_USE_BUFFEROBJECT_SUPPORT"
  "-DDUK_USE_ENCODING_BUILTINS"
  "-DDUK_USE_PERFORMANCE_BUILTIN"
  "-DDUK_USE_OBJECT_BUILTIN"
  "-DDUK_USE_ES6_PROXY"
  "-DDUK_USE_GLOBAL_BINDING"
  "-DDUK_USE_SYMBOL_BUILTIN"
  "-DDUK_USE_SECTION_B"
  )

add_custom_command(
  COMMENT "Making duktape" VERBATIM
  WORKING_DIRECTORY ${DEP_DUK}
  OUTPUT ${DEP_DUK}/src-low/duktape.c
  DEPENDS "${DEP_DUK}/src-input/"
  COMMAND rm -rf ${DEP_DUK}/src-low
  COMMAND python tools/configure.py --output-directory=src-low ${DUK_BUILD_FLAGS}
)

add_custom_command(
  COMMENT "Installing Node modules" VERBATIM
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT ${PROJECT_SOURCE_DIR}/node_modules/
  DEPENDS "${PROJECT_SOURCE_DIR}/package.json"
  COMMAND pnpm install
)

add_custom_command(
  COMMENT "Making JavaScript modules" VERBATIM
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT ${PROJECT_SOURCE_DIR}/lib/
  DEPENDS dukc ${PROJECT_SOURCE_DIR}/node_modules/
  COMMAND rm -rf lib lib_js/build
  COMMAND cd lib_js && node ../node_modules/typescript/bin/tsc
  COMMAND mkdir lib
  COMMAND ./util/dukc lib_js/build lib
)

add_custom_target(
  lowjs_lib
  DEPENDS ${PROJECT_SOURCE_DIR}/lib/
)

# dukc
add_executable(
  dukc
  ${PROJECT_SOURCE_DIR}/util/dukc.c
)
target_include_directories(
  dukc
  PRIVATE ${DEP_DUK}/src-low/
)
target_link_libraries(
  dukc
  duktape m
)
set_target_properties(dukc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/util)

# duktape
add_library(
  duktape STATIC
  ${DEP_DUK}/src-low/duktape.c
)
target_include_directories(
  duktape
  PRIVATE ${DEP_DUK}/src-low/
)

# lowjs
file(GLOB lowjs_src
  ${PROJECT_SOURCE_DIR}/src/*.c
  ${PROJECT_SOURCE_DIR}/src/*.h
  ${PROJECT_SOURCE_DIR}/src/*.cpp
  )

add_library(
  lowjs STATIC
  ${lowjs_src}
)

target_include_directories(
  lowjs
  PRIVATE ${PROJECT_SOURCE_DIR}/include
  PRIVATE ${PROJECT_SOURCE_DIR}/app
  PUBLIC ${DEP_DUK}/src-low/
  PUBLIC ${MBEDTLS_INCLUDE_DIRS}
)
target_link_libraries(
  lowjs
  mbedtls duktape c-ares m ${CMAKE_THREAD_LIBS_INIT}
  -static-libstdc++ -static-libgcc
)

add_dependencies(
  lowjs
  mbedtls duktape c-ares
)

# lowjs_main
add_executable(
  lowjs_main
  ${PROJECT_SOURCE_DIR}/app/main.c
)
target_include_directories(
  lowjs_main
  PRIVATE ${PROJECT_SOURCE_DIR}/include
  PRIVATE ${PROJECT_SOURCE_DIR}/app
  PUBLIC ${DEP_DUK}/src-low/
)
target_link_libraries(
  lowjs_main
  lowjs
)
add_dependencies(lowjs_main lowjs_lib)

# lowjs_with_native_modules
file(GLOB lowjs_with_native_modules_src
  ${PROJECT_SOURCE_DIR}/examples/low_with_native_module/*.c
  )

add_executable(
  lowjs_with_native_modules
  ${lowjs_with_native_modules_src}
)
target_include_directories(
  lowjs_with_native_modules
  PRIVATE ${PROJECT_SOURCE_DIR}/include
  PRIVATE ${PROJECT_SOURCE_DIR}/app
  PUBLIC ${DEP_DUK}/src-low/
)
target_link_libraries(
  lowjs_with_native_modules
  lowjs
)
add_dependencies(lowjs_with_native_modules lowjs_lib)
