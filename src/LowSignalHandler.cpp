// -----------------------------------------------------------------------------
//  LowSignalHandler.cpp
// -----------------------------------------------------------------------------

#include "LowSignalHandler.h"

#include "low_main.h"
#include "low_config.h"

#include <signal.h>

// -----------------------------------------------------------------------------
//  LowSignalHandler::LowSignalHandler
// -----------------------------------------------------------------------------

LowSignalHandler::LowSignalHandler(low_main_t *low, int signal) : LowLoopCallback(low), mLow(low), mSignal(signal)
{
    low_loop_set_callback(low, this);
}

// -----------------------------------------------------------------------------
//  LowSignalHandler::OnLoop
// -----------------------------------------------------------------------------

bool LowSignalHandler::OnLoop()
{
    const char *name;

    switch (mSignal)
    {
        case SIGUSR1:
            name = "SIGUSR1";
            break;
        case SIGUSR2:
            name = "SIGUSR2";
            break;
        case SIGPIPE:
            name = "SIGPIPE";
            break;
        case SIGWINCH:
            name = "SIGWINCH";
            break;
        case SIGTERM:
            name = "SIGTERM";
            break;
        case SIGINT:
            name = "SIGINT";
            break;
        case SIGHUP:
            name = "SIGHUP";
            break;
        default:
            return false;
    }

    low_push_stash(mLow, mLow->signal_call_id, false);
    duk_push_string(mLow->duk_ctx, name);
    duk_call(mLow->duk_ctx, 1);
    if (!duk_require_boolean(mLow->duk_ctx, -1) && (mSignal == SIGTERM || mSignal == SIGINT || mSignal == SIGHUP))
    {
#if LOW_HAS_SYS_SIGNALS
        // Go back to default handler
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESTART;
        action.sa_handler = SIG_DFL;
        sigaction(mSignal, &action, NULL);

        // Exit
        raise(mSignal);
#else
        while (true)
        {
        }
#endif /* LOW_HAS_SIGNALS */
    }

    return false;
}