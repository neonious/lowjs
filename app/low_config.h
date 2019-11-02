// -----------------------------------------------------------------------------
//  low_config.h
// -----------------------------------------------------------------------------

#ifndef __LOW_CONFIG_H__
#define __LOW_CONFIG_H__

#define LOW_NUM_DATA_THREADS 4

// Enables dns.resolve API but requires additional library c-ares
#define LOW_INCLUDE_CARES_RESOLVER 1

#define LOW_USE_SYSTEM_ALLOC 1

// Poll is superior to select because not limited to FD_SETSIZE sockets
#define LOW_HAS_POLL 1

#define LOW_HAS_TERMIOS 1

#define LOW_HAS_STRCASESTR 1

#define LOW_HAS_SYS_SIGNALS 1

#define LOW_HAS_UNIX_SOCKET 1

#define LOW_ESP32_LWIP_SPECIALITIES 0

#endif /* __LOW_CONFIG_H__ */
