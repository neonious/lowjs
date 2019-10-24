// -----------------------------------------------------------------------------
//  low_system.cpp
// -----------------------------------------------------------------------------

#include "low_system.h"
#include "low_main.h"
#include "low_alloc.h"
#include "low_config.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#if LOW_HAS_TERMIOS
#include <termios.h>
#endif /* LOW_HAS_TERMIOS */
#if LOW_ESP32_LWIP_SPECIALITIES
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

#ifdef __APPLE__
#include <libproc.h>
#include <mach/clock.h>
#include <mach/mach.h>
#else
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#endif /* __APPLE__ */

// Global variables
low_system_t g_low_system;

#if LOW_HAS_SYS_SIGNALS

// -----------------------------------------------------------------------------
//	low_system_signal
// -----------------------------------------------------------------------------

static void low_system_signal(int sig)
{
    if(g_low_system.signal_pipe_fd >= 0)
    {
        char s = sig;
        write(g_low_system.signal_pipe_fd, &s, 1);
    }
}

#endif /* LOW_HAS_SYS_SIGNALS */

// -----------------------------------------------------------------------------
//  low_system_init
// -----------------------------------------------------------------------------

bool low_system_init(int argc, const char *argv[])
{
    g_low_system.argc = argc;
    g_low_system.argv = argv;

    srand(time(NULL));

    // Setup signal handler
    g_low_system.signal_pipe_fd = -1;

#if LOW_HAS_SYS_SIGNALS
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    action.sa_handler = low_system_signal;

    sigaction(SIGUSR1, &action, NULL);
    sigaction(SIGUSR2, &action, NULL);
    sigaction(SIGPIPE, &action, NULL);
    sigaction(SIGWINCH, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
#endif /* LOW_HAS_SYS_SIGNALS */

    g_low_system.lib_path = NULL;

#if LOW_ESP32_LWIP_SPECIALITIES
    g_low_system.lib_path = (char *)"/lib/";
#else
    const char lib_add_path[] = "../lib/";
    int lib_add_path_len = sizeof(lib_add_path) - 1;

#ifdef __APPLE__
    char path[PROC_PIDPATHINFO_MAXSIZE + lib_add_path_len];
    g_low_system.lib_path =
        (char *)low_alloc(PROC_PIDPATHINFO_MAXSIZE + lib_add_path_len);
    if(!g_low_system.lib_path)
    {
        low_error_errno();
        goto err;
    }

    if(proc_pidpath(getpid(), path, PROC_PIDPATHINFO_MAXSIZE) <= 0)
    {
        low_error_errno();
        goto err;
    }
#else
#define MAX_PATH_LEN 1024
    char path[MAX_PATH_LEN + 1 + lib_add_path_len];

    g_low_system.lib_path =
        (char *)low_alloc(MAX_PATH_LEN + 1 + lib_add_path_len);
    if(!g_low_system.lib_path)
    {
        low_error_errno();
        goto err;
    }

    memset(path, 0, MAX_PATH_LEN + 1);
    if(readlink("/proc/self/exe", path, MAX_PATH_LEN) < 0)
    {
        low_error_errno();
        goto err;
    }
#endif /* __APPLE__ */

    int i;
    for(i = strlen(path); i > 0; i--)
        if(path[i - 1] == '/')
        {
            path[i] = '\0';
            break;
        }
    strcpy(path + i, lib_add_path);

    realpath(path, g_low_system.lib_path);
    i = strlen(g_low_system.lib_path);
    g_low_system.lib_path[i] = '/';
    g_low_system.lib_path[i + 1] = '\0';
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

#if LOW_HAS_TERMIOS
    g_low_system.isatty = isatty(0);
    g_low_system.raw_mode = false;
    if(g_low_system.isatty)
    {
        if(tcgetattr(0, &g_low_system.orig_termios) < 0)
        {
            low_error_errno();
            goto err;
        }
    }
#endif /* LOW_HAS_TERMIOS */

    return true;

err:
#if !LOW_ESP32_LWIP_SPECIALITIES
    low_free(g_low_system.lib_path);
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */
    return false;
}

// -----------------------------------------------------------------------------
//  low_system_destroy
// -----------------------------------------------------------------------------

void low_system_destroy()
{
    low_set_raw_mode(false);
#if !LOW_ESP32_LWIP_SPECIALITIES
    low_free(g_low_system.lib_path);
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */
}

// -----------------------------------------------------------------------------
//  low_set_raw_mode -
// -----------------------------------------------------------------------------

bool low_set_raw_mode(bool mode)
{
#if LOW_HAS_TERMIOS
    if(!g_low_system.isatty || g_low_system.raw_mode == mode)
        return true;

    if(mode)
    {
        struct termios raw;
        raw =
            g_low_system.orig_termios; /* copy original and then modify below */

        /* input modes - clear indicated ones giving: no break, no CR to NL,
        no parity check, no strip char, no start/stop output (sic) control */
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

        /* control modes - set 8 bit chars */
        raw.c_cflag |= (CS8);

        /* local modes - clear giving: echoing off, canonical off (no erase with
        backspace, ^U,...),  no extended functions, no signal chars (^Z,^C) */
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

        /* control chars - set return condition: min number of bytes and timer
         */
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0; /* after two bytes, no timer  */

        /* put terminal in raw mode after flushing */
        if(tcsetattr(0, TCSAFLUSH, &raw) < 0)
            return false;
        g_low_system.raw_mode = true;
    }
    else
    {
        if(tcsetattr(0, TCSAFLUSH, &g_low_system.orig_termios) < 0)
            return false;
        g_low_system.raw_mode = false;
    }

    return true;
#else
    return false;
#endif /* LOW_HAS_TERMIOS */
}

// -----------------------------------------------------------------------------
//  low_tick_count - give monotic tick count in ms
// -----------------------------------------------------------------------------

int low_tick_count()
{
#if LOW_ESP32_LWIP_SPECIALITIES
    return xTaskGetTickCount();
#else
#ifdef __APPLE__
    // Mac OS X >= 10.12 also support clock_gettime(CLOCK_MONOTONIC),
    // but this also works for older versions
    static clock_serv_t cclock;
    static bool alloc_cclock = false;
    mach_timespec_t tv;

    if(!alloc_cclock)
    {
        host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
        alloc_cclock = true;
    }
    clock_get_time(cclock, &tv);
#else
    struct timespec tv;
    if(clock_gettime(CLOCK_MONOTONIC, &tv) < 0)
        low_error_errno();
#endif /* __APPLE__ */

    return (int)(((long long)tv.tv_sec) * 1000 + tv.tv_nsec / 1000000);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
}

// -----------------------------------------------------------------------------
//  low_push_error
// -----------------------------------------------------------------------------

void low_push_error(struct low_main_t *low, int error, const char *syscall)
{
    const char *low_errcode[LOW_NUM_ERROR_CODES] = {
        "EUNKNOWN",        "ENODATA",       "EFORMERR",
        "ESERVFAIL",       "ENOTFOUND",     "ENOTIMP",
        "EREFUSED",        "EBADQUERY",     "EBADNAME",
        "EBADFAMILY",      "EBADRESP",      "ECONNREFUSED",
        "ETIMEOUT",        "EOF",           "EFILE",
        "ENOMEM",          "EDESTRUCTION",  "EBADSTR",
        "EBADFLAGS",       "ENONAME",       "EBADHINTS",
        "ENOTINITIALIZED", "ELOADIPHLPAPI", "EADDRGETNETWORKPARAMS",
        "ECANCELLED"};
    const char *low_errtext[LOW_NUM_ERROR_CODES] = {
        "Unknown error",
        "DNS server returned answer with no data",
        "DNS server claims query was misformatted",
        "DNS server returned general failure",
        "Domain name not found",
        "DNS server does not implement requested operation",
        "DNS server refused query",
        "Misformatted DNS query",
        "Misformatted domain name",
        "Unsupported address family",
        "Misformatted DNS reply",
        "Could not contact DNS servers",
        "Timeout while contacting DNS servers",
        "End of file",
        "Error reading file",
        "Out of memory",
        "Channel is being destroyed",
        "Misformatted string",
        "Illegal flags specified",
        "Given hostname is not numeric",
        "Illegal hints flags specified",
        "c-ares library initialization not yet performed",
        "Error loading iphlpapi.dll",
        "Could not find GetNetworkParams function",
        "DNS query cancelled"};

    char message[1024] = "", txt[10];
    const char *code;

    if(-error >= 0 && -error < LOW_NUM_ERROR_CODES)
    {
        code = low_errcode[-error];
        strcpy(message, low_errtext[-error]);
    }
    else
        switch(error)
        {
        case EPERM:
            code = "EPERM";
            break;
        case ENOENT:
            code = "ENOENT";
            break;
        case ESRCH:
            code = "ESRCH";
            break;
        case EINTR:
            code = "EINTR";
            break;
        case EIO:
            code = "EIO";
            break;
        case ENXIO:
            code = "ENXIO";
            break;
        case E2BIG:
            code = "E2BIG";
            break;
        case ENOEXEC:
            code = "ENOEXEC";
            break;
        case EBADF:
            code = "EBADF";
            break;
        case ECHILD:
            code = "ECHILD";
            break;
        case EDEADLK:
            code = "EDEADLK";
            break;
        case ENOMEM:
            code = "ENOMEM";
            break;
        case EACCES:
            code = "EACCES";
            break;
        case EFAULT:
            code = "EFAULT";
            break;
        case EBUSY:
            code = "EBUSY";
            break;
        case EEXIST:
            code = "EEXIST";
            break;
        case EXDEV:
            code = "EXDEV";
            break;
        case ENODEV:
            code = "ENODEV";
            break;
        case ENOTDIR:
            code = "ENOTDIR";
            break;
        case EISDIR:
            code = "EISDIR";
            break;
        case EINVAL:
            code = "EINVAL";
            break;
        case ENFILE:
            code = "ENFILE";
            break;
        case EMFILE:
            code = "EMFILE";
            break;
        case ENOTTY:
            code = "ENOTTY";
            break;
        case ETXTBSY:
            code = "ETXTBSY";
            break;
        case EFBIG:
            code = "EFBIG";
            break;
        case ENOSPC:
            code = "ENOSPC";
            break;
        case ESPIPE:
            code = "ESPIPE";
            break;
        case EROFS:
            code = "EROFS";
            break;
        case EMLINK:
            code = "EMLINK";
            break;
        case EPIPE:
            code = "EPIPE";
            break;
        case EDOM:
            code = "EDOM";
            break;
        case ERANGE:
            code = "ERANGE";
            break;
        case EAGAIN:
            code = "EAGAIN";
            break;
        case EINPROGRESS:
            code = "EINPROGRESS";
            break;
        case EALREADY:
            code = "EALREADY";
            break;
        case ENOTSOCK:
            code = "ENOTSOCK";
            break;
        case EDESTADDRREQ:
            code = "EDESTADDRREQ";
            break;
        case EMSGSIZE:
            code = "EMSGSIZE";
            break;
        case EPROTOTYPE:
            code = "EPROTOTYPE";
            break;
        case ENOPROTOOPT:
            code = "ENOPROTOOPT";
            break;
        case EPROTONOSUPPORT:
            code = "EPROTONOSUPPORT";
            break;
        case ENOTSUP:
            code = "ENOTSUP";
            break;
        case EPFNOSUPPORT:
            code = "EPFNOSUPPORT";
            break;
        case EAFNOSUPPORT:
            code = "EAFNOSUPPORT";
            break;
        case EADDRINUSE:
            code = "EADDRINUSE";
            break;
        case EADDRNOTAVAIL:
            code = "EADDRNOTAVAIL";
            break;
        case ENETDOWN:
            code = "ENETDOWN";
            break;
        case ENETUNREACH:
            code = "ENETUNREACH";
            break;
        case ENETRESET:
            code = "ENETRESET";
            break;
        case ECONNABORTED:
            code = "ECONNABORTED";
            break;
        case ECONNRESET:
            code = "ECONNRESET";
            break;
        case ENOBUFS:
            code = "ENOBUFS";
            break;
        case EISCONN:
            code = "EISCONN";
            break;
        case ENOTCONN:
            code = "ENOTCONN";
            break;
        case ETOOMANYREFS:
            code = "ETOOMANYREFS";
            break;
        case ETIMEDOUT:
            code = "ETIMEDOUT";
            break;
        case ECONNREFUSED:
            code = "ECONNREFUSED";
            break;
        case ELOOP:
            code = "ELOOP";
            break;
        case ENAMETOOLONG:
            code = "ENAMETOOLONG";
            break;
        case EHOSTDOWN:
            code = "EHOSTDOWN";
            break;
        case EHOSTUNREACH:
            code = "EHOSTUNREACH";
            break;
        case ENOTEMPTY:
            code = "ENOTEMPTY";
            break;
        case EDQUOT:
            code = "EDQUOT";
            break;
        case ESTALE:
            code = "ESTALE";
            break;
        case ENOLCK:
            code = "ENOLCK";
            break;
        case ENOSYS:
            code = "ENOSYS";
            break;
        case EOVERFLOW:
            code = "EOVERFLOW";
            break;
        case ECANCELED:
            code = "ECANCELED";
            break;
        case EIDRM:
            code = "EIDRM";
            break;
        case ENOMSG:
            code = "ENOMSG";
            break;
        case EILSEQ:
            code = "EILSEQ";
            break;
        case EBADMSG:
            code = "EBADMSG";
            break;
        case EMULTIHOP:
            code = "EMULTIHOP";
            break;
        case ENODATA:
            code = "ENODATA";
            break;
        case ENOLINK:
            code = "ENOLINK";
            break;
        case ENOSR:
            code = "ENOSR";
            break;
        case ENOSTR:
            code = "ENOSTR";
            break;
        case EPROTO:
            code = "EPROTO";
            break;
        case ETIME:
            code = "ETIME";
            break;
        case ENOTRECOVERABLE:
            code = "ENOTRECOVERABLE";
            break;
        case EOWNERDEAD:
            code = "EOWNERDEAD";
            break;

#ifdef __APPLE__
        case EPROCLIM:
            code = "EPROCLIM";
            break;
        case EBADRPC:
            code = "EBADRPC";
            break;
        case ERPCMISMATCH:
            code = "ERPCMISMATCH";
            break;
        case EPROGUNAVAIL:
            code = "EPROGUNAVAIL";
            break;
        case EPROGMISMATCH:
            code = "EPROGMISMATCH";
            break;
        case EPROCUNAVAIL:
            code = "EPROCUNAVAIL";
            break;
        case EFTYPE:
            code = "EFTYPE";
            break;
        case EAUTH:
            code = "EAUTH";
            break;
        case ENEEDAUTH:
            code = "ENEEDAUTH";
            break;
        case EPWROFF:
            code = "EPWROFF";
            break;
        case EDEVERR:
            code = "EDEVERR";
            break;
        case EBADEXEC:
            code = "EBADEXEC";
            break;
        case EBADARCH:
            code = "EBADARCH";
            break;
        case ESHLIBVERS:
            code = "ESHLIBVERS";
            break;
        case EBADMACHO:
            code = "EBADMACHO";
            break;
        case ENOATTR:
            code = "ENOATTR";
            break;
        case EOPNOTSUPP:
            code = "EOPNOTSUPP";
            break;
        case ENOPOLICY:
            code = "ENOPOLICY";
            break;
        case EQFULL:
            code = "EQFULL";
            break;
#endif /* __APPLE__ */

#if !LOW_ESP32_LWIP_SPECIALITIES
        case ENOTBLK:
            code = "ENOTBLK";
            break;
        case ESOCKTNOSUPPORT:
            code = "ESOCKTNOSUPPORT";
            break;
        case ESHUTDOWN:
            code = "ESHUTDOWN";
            break;
        case EUSERS:
            code = "EUSERS";
            break;
        case EREMOTE:
            code = "EREMOTE";
            break;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

        default:
            code = txt;
            sprintf(txt, "E%d", error);
            break;
        }

    if(!message[0])
        strerror_r(error, message, sizeof(message) - 16 - strlen(syscall));
    sprintf(message + strlen(message), " (at %s)", syscall);
    duk_push_error_object(low->duk_ctx, DUK_ERR_ERROR, message);
    duk_push_string(low->duk_ctx, code);
    duk_put_prop_string(low->duk_ctx, -2, "code");
    duk_push_int(low->duk_ctx, -error);
    duk_put_prop_string(low->duk_ctx, -2, "errno");
    duk_push_string(low->duk_ctx, syscall);
    duk_put_prop_string(low->duk_ctx, -2, "syscall");
}

// -----------------------------------------------------------------------------
//  low_error_errno
// -----------------------------------------------------------------------------

void low_error_errno()
{
    char txt[1024];
    strerror_r(errno, txt, sizeof(txt));
    low_error(txt);
}

// -----------------------------------------------------------------------------
//  low_error
// -----------------------------------------------------------------------------

#if 0
#include <execinfo.h>

static void output_backtrace(void)
{
    int nptrs;
    void *buffer[100];

    nptrs = backtrace(buffer, 100);
    backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO);
}

void low_error(const char *txt)
{
    output_backtrace();
#else
void low_error(const char *txt)
{
#endif

#if LOW_ESP32_LWIP_SPECIALITIES
    // error is printed by modified DukTape in ESP32 version
#else
    fprintf(stderr, "%s\n", txt);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
}
