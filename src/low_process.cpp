// -----------------------------------------------------------------------------
//  low_process.cpp
// -----------------------------------------------------------------------------

#include "low_process.h"

#include "low_alloc.h"
#include "low_config.h"
#include "low_main.h"
#include "low_system.h"

#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>

#if LOW_ESP32_LWIP_SPECIALITIES
#include "esp_timer.h"

void console_log(const char *loglevel, const char *txt);
#elif defined(__APPLE__)
#include <libproc.h>
#include <mach/clock.h>
#include <mach/mach.h>
#include <sys/types.h>
#include <signal.h>
#else
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>
#endif /* __APPLE__ */

#if LOW_INCLUDE_CARES_RESOLVER
#include "../deps/c-ares/ares.h"
#endif /* LOW_INCLUDE_CARES_RESOLVER */
#include "mbedtls/version.h"

// used in low_hrtime() below
#define NANOS_PER_SEC 1000000000
#define NANOS_PER_MICROSEC 1000

// Global variables
#if !LOW_ESP32_LWIP_SPECIALITIES
extern char **environ;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

extern low_system_t g_low_system;

// -----------------------------------------------------------------------------
//  low_gc
// -----------------------------------------------------------------------------

duk_ret_t low_gc(duk_context *ctx)
{
    duk_gc(ctx, 0);
    return 0;
}

// -----------------------------------------------------------------------------
//  low_process_exit
// -----------------------------------------------------------------------------

static duk_ret_t low_process_exit(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    if(low->signal_call_id)
    {
        low_push_stash(low, low->signal_call_id, false);
        duk_push_string(ctx, "exit");
        duk_call(ctx, 1);
    }
#if LOW_ESP32_LWIP_SPECIALITIES
    low->duk_flag_stop = 1;
    duk_throw(ctx);
#else
    low_set_raw_mode(false);
    exit(duk_get_int(ctx, 0));
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    return 0;
}


// -----------------------------------------------------------------------------
//  low_process_abort
// -----------------------------------------------------------------------------

static duk_ret_t low_process_abort(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    if(low->signal_call_id)
    {
        low_push_stash(low, low->signal_call_id, false);
        duk_push_string(ctx, "exit");
        duk_call(ctx, 1);
    }

    low_set_raw_mode(false);
#if LOW_ESP32_LWIP_SPECIALITIES
    console_log("e", "Process aborted.");

    low->duk_flag_stop = 1;
    duk_throw(ctx);
#else
    abort();
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    return 0;
}


// -----------------------------------------------------------------------------
//  low_process_cwd
// -----------------------------------------------------------------------------

static duk_ret_t low_process_cwd(duk_context *ctx)
{
#if LOW_ESP32_LWIP_SPECIALITIES
    duk_push_string(ctx, duk_get_low_context(ctx)->cwd);
#else
    char path[1024];
    if(!getcwd(path, sizeof(path)))
    {
        low_push_error(duk_get_low_context(ctx), errno, "getcwd");
        duk_throw(ctx);
    }

    duk_push_string(ctx, path);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    return 1;
}

// -----------------------------------------------------------------------------
//  low_process_chdir
// -----------------------------------------------------------------------------

static duk_ret_t low_process_chdir(duk_context *ctx)
{
    const char *path = duk_require_string(ctx, 0);

#if LOW_ESP32_LWIP_SPECIALITIES
    low_main_t *low = duk_get_low_context(ctx);

    char *cwd = low_strdup(path);
    if(!cwd)
    {
        low_push_error(low, ENOMEM, "malloc");
        duk_throw(ctx);
    }

    low_free(low->cwd);
    low->cwd = cwd;
#else
    if(chdir(path) < 0)
    {
        low_push_error(duk_get_low_context(ctx), errno, "getcwd");
        duk_throw(ctx);
    }
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    return 0;
}
/*
// -----------------------------------------------------------------------------
//  low_process_memoryUsage
// -----------------------------------------------------------------------------

static duk_ret_t low_process_memoryUsage(duk_context *ctx)
{
    duk_push_object(ctx);
NOT DONE YET
#if LOW_ESP32_LWIP_SPECIALITIES
    duk_push_int(ctx, "win32");
    duk_put_prop_string(ctx, 0, "rss");
    duk_push_int(ctx, "win32");
    duk_put_prop_string(ctx, 0, "heapTotal");
    duk_push_int(ctx, "win32");
    duk_put_prop_string(ctx, 0, "heapUsed");
    duk_push_int(ctx, 0);
    duk_put_prop_string(ctx, 0, "external");
#else
    // TODO for non-esp32
#endif * LOW_ESP32_LWIP_SPECIALITIES *

    return 1;
}
*/
// -----------------------------------------------------------------------------
//  low_process_info
// -----------------------------------------------------------------------------

duk_ret_t low_process_info(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);

    duk_push_object(ctx);
#if !LOW_ESP32_LWIP_SPECIALITIES
    for(int i = 0; environ && environ[i]; i++)
    {
        int j;
        for(j = 0; environ[i][j] && environ[i][j] != '='; j++)
        {
        }

        if(environ[i][j])
        {
            duk_push_string(ctx, &environ[i][j + 1]);
            environ[i][j] = '\0';
            duk_put_prop_string(ctx, -2, environ[i]);
            environ[i][j] = '=';
        }
        else
        {
            duk_push_string(ctx, "");
            duk_put_prop_string(ctx, -2, environ[i]);
        }
    }
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */
    duk_put_prop_string(ctx, 0, "env");

    duk_push_c_function(ctx, low_process_exit, 1);
    duk_put_prop_string(ctx, 0, "exit");
    duk_push_c_function(ctx, low_process_abort, 0);
    duk_put_prop_string(ctx, 0, "abort");
    duk_push_c_function(ctx, low_process_cwd, 0);
    duk_put_prop_string(ctx, 0, "cwd");
    duk_push_c_function(ctx, low_process_chdir, 1);
    duk_put_prop_string(ctx, 0, "chdir");
    //    duk_push_c_function(ctx, low_process_memoryUsage, 0);
    //    duk_put_prop_string(ctx, 0, "memoryUsage");

#if LOW_ESP32_LWIP_SPECIALITIES
    duk_push_string(ctx, "esp32");
#elif defined(__APPLE__)
    duk_push_string(ctx, "darwin");
#elif defined(__linux__)
    duk_push_string(ctx, "linux");
#elif defined(__FreeBSD__)
    duk_push_string(ctx, "freebsd");
#elif defined(__OpenBSD__)
    duk_push_string(ctx, "openbsd");
#elif defined(_WIN32)
    duk_push_string(ctx, "win32");
#else
#error "unknown operating system"
#endif
    duk_put_prop_string(ctx, 0, "platform");

#if LOW_ESP32_LWIP_SPECIALITIES
    duk_push_string(ctx, "xtensa");
#elif defined(__aarch64__)
    duk_push_string(ctx, "arm64");
#elif defined(__arm__)
    duk_push_string(ctx, "arm");
#elif defined(__x86_64__)
    duk_push_string(ctx, "x64");
#elif defined(__i386__)
    duk_push_string(ctx, "x32");
#else
#error "unknown architecture"
#endif
    duk_put_prop_string(ctx, 0, "arch");

    duk_push_object(ctx);
    duk_push_string(ctx, LOW_VERSION);
    duk_put_prop_string(ctx, -2, "lowjs");
#if LOW_ESP32_LWIP_SPECIALITIES
    duk_push_string(ctx, LOW_ESP32_VERSION);
    duk_put_prop_string(ctx, -2, "lowjs_esp32");
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    duk_push_string(ctx, "10.0.0");
    duk_put_prop_string(ctx, -2, "node");
#if LOW_INCLUDE_CARES_RESOLVER
    duk_push_string(ctx, ares_version(NULL));
    duk_put_prop_string(ctx, -2, "ares");
#endif /* LOW_INCLUDE_CARES_RESOLVER */
    duk_push_string(ctx, MBEDTLS_VERSION_STRING);
    duk_put_prop_string(ctx, -2, "mbedtls");
    duk_put_prop_string(ctx, 0, "versions");

    low->signal_call_id = low_add_stash(low, 1);
    return 0;
}

// -----------------------------------------------------------------------------
//  low_tty_info
// -----------------------------------------------------------------------------

duk_ret_t low_tty_info(duk_context *ctx)
{
#if LOW_HAS_TERMIOS
    if(!g_low_system.isatty)
        return 0;

    low_main_t *low = duk_get_low_context(ctx);

    struct winsize w;
    if(ioctl(0, TIOCGWINSZ, &w) < 0)
    {
        low_push_error(low, errno, "ioctl");
        duk_throw(ctx);
    }

    duk_push_object(ctx);
    duk_push_int(ctx, w.ws_row);
    duk_put_prop_string(ctx, -2, "rows");
    duk_push_int(ctx, w.ws_col);
    duk_put_prop_string(ctx, -2, "columns");
    return 1;
#else
    return 0;
#endif /* LOW_HAS_TERMIOS */
}

// -----------------------------------------------------------------------------
//  low_hrtime
// -----------------------------------------------------------------------------

duk_ret_t low_hrtime(duk_context *ctx)
{
    uint64_t t;

#if LOW_ESP32_LWIP_SPECIALITIES
    t = NANOS_PER_MICROSEC * (uint64_t)esp_timer_get_time();
#else
#ifdef __APPLE__
    // Mac OS X >= 10.12 also support clock_gettime(CLOCK_MONOTONIC),
    // but this also works for older versions
    static clock_serv_t cclock;
    static bool alloc_cclock = false;
    mach_timespec_t ts;

    if(!alloc_cclock)
    {
        host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
        alloc_cclock = true;
    }
    clock_get_time(cclock, &ts);
#else
    struct timespec ts;
    if(clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
        low_error_errno();
#endif /* __APPLE__ */

    t = (((uint64_t)ts.tv_sec) * NANOS_PER_SEC + ts.tv_nsec);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    uint32_t *fields = (uint32_t *)duk_require_buffer_data(ctx, 0, nullptr);
    fields[0] = (t / NANOS_PER_SEC) >> 32;
    fields[1] = (t / NANOS_PER_SEC) & 0xffffffff;
    fields[2] = t % NANOS_PER_SEC;

    return 1;
}
