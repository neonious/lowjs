/*
 * duk_crash.c
 * 
 * Creates Duktape heap, attachs fake debugger which resumes and then does infinite recursion
 * Should produce fatal error, but crashes the current Duktape version
 * 
 * Tested with Duktape commit 26f00693780c976b4e490af68c022bccba9d80c0
 * and the following configuration:
		-DDUK_USE_EXEC_TIMEOUT_CHECK=\*\(unsigned\ char\ \*\)   \
		-DDUK_USE_INTERRUPT_COUNTER   \
		-DDUK_USE_FATAL_HANDLER   \
		-DDUK_USE_DEBUGGER_SUPPORT   \
		-DDUK_USE_DEBUGGER_PAUSE_UNCAUGHT   \
		-DDUK_USE_DEBUGGER_INSPECT   \
		-DDUK_USE_DEBUGGER_THROW_NOTIFY   \
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "duktape.h"

/*
 * We fake a debugger here, which directly sends the resume command, and nothing more
 */

int sent_resume = 0;

duk_size_t read_cb(void *udata, char *buffer, duk_size_t length)
{
    if(sent_resume == 3)
    {
        printf("Asking for more data after resume! Unexpected result, exiting!\n");
        exit(EXIT_FAILURE);
    }

    int at = 0;
    if(length && sent_resume == 0)
    {
        buffer[at++] = 0x01; /* DUK_DBG_IB_REQUEST */
        length--;
        sent_resume = 1;
    }
    if(length && sent_resume == 1)
    {
        buffer[at++] = (char)0x80 | 0x13; /* DUK_DBG_CMD_RESUME */
        length--;
        sent_resume = 2;
    }
    if(length && sent_resume == 2)
    {
        buffer[at++] = 0x00; /* DUK_DBG_IB_EOM */
        length--;
        sent_resume = 3;
    }

    return at;
}

duk_size_t write_cb(void *udata, const char *buffer, duk_size_t length)
{
    return length;
}

duk_size_t peek_cb(void *udata)
{
    return 3 - sent_resume;
}

void detached_cb(duk_context *ctx, void *udata)
{
    printf("Debugger detached! Unexpected result, exiting!\n");
    exit(EXIT_FAILURE);
}

void fatal_cb(void *udata, const char *msg)
{
    if(strcmp(msg, "uncaught: 'callstack limit'") == 0)
    {
        printf("Test passed!\n");
        exit(EXIT_SUCCESS);
    }
    else
    {
        printf("%s. Unexpected result, exiting!\n", msg);
        exit(EXIT_FAILURE);
    }
}

int main()
{
    char byte = 0;  // we use DDUK_USE_EXEC_TIMEOUT_CHECK=\*\(unsigned\ char\ \*\)
                    // but you do not need to use this to reproduce the bug
    duk_context *ctx = duk_create_heap(NULL, NULL, NULL, &byte, fatal_cb);

    duk_debugger_attach(ctx, read_cb, write_cb, peek_cb, NULL, NULL, NULL, detached_cb, NULL);
    duk_eval_string(ctx, "function a() { a(); } a();");

    printf("Did not hit fatal handler. Unexpected result, exiting!\n");
    return EXIT_FAILURE;
}
