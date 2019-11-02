// -----------------------------------------------------------------------------
//  low_system.h
// -----------------------------------------------------------------------------

#ifndef __LOW_SYSTEM_H__
#define __LOW_SYSTEM_H__

#include "low_config.h"

#if LOW_HAS_TERMIOS
#include <termios.h>
#endif /* LOW_HAS_TERMIOS */

#define LOW_EUNKNOWN 0

// 1-to-1 from c-ares, but with negative number
#define LOW_ENODATA -1
#define LOW_EFORMERR -2
#define LOW_ESERVFAIL -3
#define LOW_ENOTFOUND -4
#define LOW_ENOTIMP -5
#define LOW_EREFUSED -6
#define LOW_EBADQUERY -7
#define LOW_EBADNAME -8
#define LOW_EBADFAMILY -9
#define LOW_EBADRESP -10
#define LOW_ECONNREFUSED -11
#define LOW_ETIMEOUT -12
#define LOW_EOF -13
#define LOW_EFILE -14
#define LOW_ENOMEM -15
#define LOW_EDESTRUCTION -16
#define LOW_EBADSTR -17
#define LOW_EBADFLAGS -18
#define LOW_ENONAME -19
#define LOW_EBADHINTS -20
#define LOW_ENOTINITIALIZED -21
#define LOW_ELOADIPHLPAPI -22
#define LOW_EADDRGETNETWORKPARAMS -23
#define LOW_ECANCELLED -24

#define LOW_NUM_ERROR_CODES 25

struct low_system_t
{
#if !LOW_ESP32_LWIP_SPECIALITIES
    int argc;
    const char **argv;

    char *main_module_path;

    char *lib_path;
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */

#if LOW_HAS_TERMIOS
    bool isatty, raw_mode;
    struct termios orig_termios;
#endif /* LOW_HAS_TERMIOS */

    int signal_pipe_fd;
};

extern "C"
{
    bool low_system_init(int argc, const char *argv[]);
    void low_system_destroy();
}

bool low_set_raw_mode(bool mode);
int low_tick_count();

void low_push_error(struct low_t *low, int error, const char *syscall);

void low_error_errno();
void low_error(const char *txt);

#endif /* __LOW_SYSTEM_H__ */
