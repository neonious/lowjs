// -----------------------------------------------------------------------------
//  low_process.cpp
// -----------------------------------------------------------------------------

#include "low_process.h"

#include "low_alloc.h"
#include "low_config.h"
#include "low_main.h"
#include "low_loop.h"
#include "low_system.h"

#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>

#if LOW_ESP32_LWIP_SPECIALITIES
#include "esp_heap_caps.h"
#include "esp_timer.h"

void console_log(const char *loglevel, const char *txt);
#elif defined(__APPLE__)
#include <libproc.h>
#include <mach/clock.h>
#include <mach/mach.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <signal.h>
#else
#include <sys/sysinfo.h>
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
#if LOW_ESP32_LWIP_SPECIALITIES
extern int gProcessStdinJSObject;
#else
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
    int code = duk_get_int(ctx, 0);

    low_t *low = duk_get_low_context(ctx);
    if(low->signal_call_id)
    {
        low_push_stash(ctx, low->signal_call_id, false);
        duk_push_string(ctx, "emit");
        duk_push_string(ctx, "exit");
        duk_push_int(ctx, code);
        duk_call_prop(ctx, -4, 2);
    }
#if LOW_ESP32_LWIP_SPECIALITIES
    low->duk_flag_stop = 1;
    duk_generic_error(ctx, "abort (should not be visible)");
    duk_throw(ctx);
#else
    low_system_destroy();
    exit(code);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    return 0;
}


// -----------------------------------------------------------------------------
//  low_process_abort
// -----------------------------------------------------------------------------

static duk_ret_t low_process_abort(duk_context *ctx)
{
#if LOW_ESP32_LWIP_SPECIALITIES
    console_log("e", "Process aborted.\n");

    low_t *low = duk_get_low_context(ctx);

    low->duk_flag_stop = 1;
    duk_generic_error(ctx, "abort (should not be visible)");
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
        low_push_error(ctx, errno, "getcwd");
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
    low_t *low = duk_get_low_context(ctx);

    char *cwd = low_strdup(path);
    if(!cwd)
    {
        low_push_error(ctx, ENOMEM, "malloc");
        duk_throw(ctx);
    }

    low_free(low->cwd);
    low->cwd = cwd;
#else
    if(chdir(path) < 0)
    {
        low_push_error(ctx, errno, "getcwd");
        duk_throw(ctx);
    }
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    return 0;
}

// -----------------------------------------------------------------------------
//  low_process_umask
// -----------------------------------------------------------------------------

static duk_ret_t low_process_umask(duk_context *ctx)
{
    //Since there is no method to set the umask, we can always return the default
    duk_push_int(ctx, 0);
    return 1;
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
    low_t *low = duk_get_low_context(ctx);

#if LOW_ESP32_LWIP_SPECIALITIES
    duk_get_prop_string(ctx, -1, "stdin");
    gProcessStdinJSObject = low_add_stash(ctx, duk_get_top_index(ctx));
    duk_pop(ctx);

    duk_push_array(ctx);

    duk_push_string(ctx, "low");
    duk_dup(ctx, -1);
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, 0, "execPath");
    duk_put_prop_string(ctx, 0, "argv0");
    duk_put_prop_index(ctx, -2, 0);
    duk_put_prop_string(ctx, 0, "argv");

    duk_push_object(ctx);
    duk_put_prop_string(ctx, 0, "env");
#else

#ifdef __APPLE__
    char *path = (char *)low_alloc(PROC_PIDPATHINFO_MAXSIZE);
    if(!path)
    {
        low_push_error(ctx, ENOMEM, "malloc");
        duk_throw(ctx);
    }
    if(proc_pidpath(getpid(), path, PROC_PIDPATHINFO_MAXSIZE) <= 0)
    {
        free(path);
        low_push_error(ctx, errno, "proc_pidpath");
        duk_throw(ctx);
    }
#else
#define MAX_PATH_LEN 1024

    char *path = (char *)low_alloc(MAX_PATH_LEN + 1);
    if(!path)
    {
        low_push_error(ctx, ENOMEM, "malloc");
        duk_throw(ctx);
    }
    memset(path, 0, MAX_PATH_LEN + 1);
    if(readlink("/proc/self/exe", path, MAX_PATH_LEN) < 0)
    {
        free(path);
        low_push_error(ctx, ENOMEM, "readlink");
        duk_throw(ctx);
    }
#endif /* __APPLE__ */

    duk_push_array(ctx);

    duk_push_string(ctx, path);
    free(path);
    duk_dup(ctx, -1);
    duk_put_prop_index(ctx, -3, 0);
    duk_put_prop_string(ctx, 0, "execPath");

    if(g_low_system.argc >= 2)
    {
        char path2[PATH_MAX];
        realpath(g_low_system.argv[1], path2);
        duk_push_string(ctx, path2);
        duk_put_prop_index(ctx, -2, 1);

        for(int i = 2; i < g_low_system.argc; i++)
        {
            duk_push_string(ctx, g_low_system.argv[i]);
            duk_put_prop_index(ctx, -2, i);
        }
    }
    duk_put_prop_string(ctx, 0, "argv");

    if(g_low_system.argc)
    {
        duk_push_string(ctx, g_low_system.argv[0]);
        duk_put_prop_string(ctx, 0, "argv0");
    }

    duk_push_object(ctx);
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
    duk_put_prop_string(ctx, 0, "env");
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */

    duk_push_c_function(ctx, low_process_exit, 1);
    duk_put_prop_string(ctx, 0, "exit");
    duk_push_c_function(ctx, low_process_abort, 0);
    duk_put_prop_string(ctx, 0, "abort");
    duk_push_c_function(ctx, low_process_cwd, 0);
    duk_put_prop_string(ctx, 0, "cwd");
    duk_push_c_function(ctx, low_process_chdir, 1);
    duk_put_prop_string(ctx, 0, "chdir");
    duk_push_c_function(ctx, low_call_next_tick_js, DUK_VARARGS);
    duk_put_prop_string(ctx, 0, "nextTick");
    duk_push_c_function(ctx, low_process_umask, 0); //Deprecated in node v14
    duk_put_prop_string(ctx, 0, "umask");
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
    duk_push_string(ctx, DUK_GIT_DESCRIBE);
    duk_put_prop_string(ctx, -2, "duktape");
    duk_push_string(ctx, MBEDTLS_VERSION_STRING);
    duk_put_prop_string(ctx, -2, "mbedtls");

    // So node tests work
    duk_push_string(ctx, "mbedtls " MBEDTLS_VERSION_STRING);
    duk_put_prop_string(ctx, -2, "openssl");

    duk_put_prop_string(ctx, 0, "versions");
    duk_push_string(ctx, "v10.0.0");
    duk_put_prop_string(ctx, 0, "version");

    low->signal_call_id = low_add_stash(ctx, 0);
    return 0;
}


// -----------------------------------------------------------------------------
//  low_os_info
// -----------------------------------------------------------------------------

duk_ret_t low_os_info(duk_context *ctx)
{
    duk_push_object(ctx);

#if LOW_ESP32_LWIP_SPECIALITIES
    duk_push_int(ctx, heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    duk_put_prop_string(ctx, -2, "freemem");
    duk_push_int(ctx, 4 * 1024 * 1024);
    duk_put_prop_string(ctx, -2, "totalmem");
    duk_push_int(ctx, esp_timer_get_time() / 1000000);
    duk_put_prop_string(ctx, -2, "uptime");
#elif defined(__APPLE__)
{
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };
    if(sysctl(mib, 2, &boottime, &len, NULL, 0) >= 0)
    {
        time_t bsec = boottime.tv_sec, csec = time(NULL);
        duk_push_int(ctx, csec - bsec);
        duk_put_prop_string(ctx, -2, "uptime");
    }
}
{
    unsigned long long mem;
    size_t len = sizeof(mem);
    int mib[2] = { CTL_HW, HW_MEMSIZE };
    if(sysctl(mib, 2, &mem, &len, NULL, 0) >= 0)
    {
        duk_push_number(ctx, mem);
        duk_put_prop_string(ctx, -2, "totalmem");
    }
    // Get Virtual Memory Stats
    xsw_usage vmusage = {0};
    size_t size = sizeof(vmusage);
    if( sysctlbyname("vm.swapusage", &vmusage, &size, NULL, 0) == 0 )
    {
        mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
        vm_statistics_data_t vmstat;
        if(KERN_SUCCESS == host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&vmstat, &count)) {
        duk_push_number(ctx, (int64_t)vmstat.free_count * (int64_t)vmusage.xsu_pagesize);
        duk_put_prop_string(ctx, -2, "freemem");
        }
    }
}
#else
    struct sysinfo info;
    if(sysinfo(&info) == 0)
    {
        duk_push_number(ctx, info.freeram * (double)info.mem_unit);
        duk_put_prop_string(ctx, -2, "freemem");
        duk_push_number(ctx, info.totalram * (double)info.mem_unit);
        duk_put_prop_string(ctx, -2, "totalmem");
        duk_push_int(ctx, info.uptime);
        duk_put_prop_string(ctx, -2, "uptime");
    }
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    return 1;
}


// -----------------------------------------------------------------------------
//  low_tty_info
// -----------------------------------------------------------------------------

#if !LOW_ESP32_LWIP_SPECIALITIES
duk_ret_t low_tty_info(duk_context *ctx)
{
#if LOW_HAS_TERMIOS
    if(!g_low_system.isatty)
        return 0;

    low_t *low = duk_get_low_context(ctx);

    struct winsize w;
    if(ioctl(0, TIOCGWINSZ, &w) < 0)
    {
        low_push_error(ctx, errno, "ioctl");
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
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */

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
    {
        low_push_error(ctx, errno, "clock_gettime");
        duk_throw(ctx);
    }
#endif /* __APPLE__ */

    t = (((uint64_t)ts.tv_sec) * NANOS_PER_SEC + ts.tv_nsec);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    uint32_t *fields = (uint32_t *)duk_require_buffer_data(ctx, 0, nullptr);
    fields[0] = (t / NANOS_PER_SEC) >> 32;
    fields[1] = (t / NANOS_PER_SEC) & 0xffffffff;
    fields[2] = t % NANOS_PER_SEC;

    return 1;
}
