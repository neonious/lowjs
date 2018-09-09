/*
 * duk_crash.c
 * 
 * Should result into Maximum call stack size exceeded
 * But results into Segmentation fault
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

    duk_eval_string(ctx, "function AClass() {}			    \
Object.defineProperty(AClass, Symbol.hasInstance, {		    \
    value: function (object) {								\
        return object instanceof this;						\
    }														\
});															\
var w = new AClass();                                       \
w instanceof AClass;                                        \
");

	printf("No fatal handler called. Unexpected result, exiting!\n");
    return EXIT_FAILURE;
}
