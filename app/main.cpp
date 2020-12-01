// -----------------------------------------------------------------------------
//  main.cpp
// -----------------------------------------------------------------------------

#ifndef __APPLE__
#define _GNU_SOURCE
#endif /* __APPLE__ */

#include "transpile.h"

#include "low_main.h"
#include "low_module.h"
#include "low_loop.h"
#include "low_system.h"

#include <stdlib.h>
#include <stdio.h>

#ifndef __APPLE__
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sysinfo.h>
#endif /* __APPLE__ */


// Stubs, used by low.js to call lowjs_esp32
// Are unused in POSIX version of lowjs
void duk_copy_breakpoints(duk_context *from, duk_context *to) {}
extern "C" void alloc_use_fund() {}


#ifndef __APPLE__

// -----------------------------------------------------------------------------
//  handle_dist_loader - cleans up environment
// -----------------------------------------------------------------------------

static void handle_dist_loader(int *argc, char ***argv)
{
	if(*argc < 2)
		return;

	const char postfix[] = LOW_LIB_PATH "low-exe";
	int len = strlen((*argv)[0]);
	if(len < sizeof(postfix) || strcmp((*argv)[0] + len - (sizeof(postfix) - 1), postfix) != 0)
		return;

	/*
	 * PART OF THE DISTRIBUTION LOADER
	 * This cleans up the command line which util/dist-loader.c
	 * has left a bit messed up.
	 */

	// Copy arguments so we can change command line
	char *cmdlineBuf = (*argv)[0];
	int cmdlineBufLen = *argc ? ((*argv)[*argc - 1] - (*argv)[0]) + strlen((*argv)[*argc - 1]) + 1 : 0;

	// First argument is path to launch low, not the path we want to use
	(*argc)--;
	char **newArgv = (char **)malloc(*argc * sizeof(char *));
	for(int i = 0; i < *argc; i++)
		newArgv[i] = strdup((*argv)[i + 1]);
	*argv = newArgv;

	// Remove ld-linux
	char txt[80];
	sprintf(txt, "/proc/%d/cmdline", getpid());
	int fd = open(txt, O_RDONLY);
	if(fd >= 0)
	{
		char cmdline[1024];
		int cmdline_len = read(fd, cmdline, sizeof(cmdline));
		if(cmdline_len > 0)
		{
			int len = strlen(newArgv[0]);
			char *dup = (char *)malloc(len + 2);
			dup[0] = '\0';
			memcpy(dup + 1, newArgv[0], len + 1);		// finding \0arg\0
			char *start = (char *)memmem(cmdline, cmdline_len, dup, len + 2);
			if(start)
			{
				int diff = start + 1 - cmdline;
				memmove(cmdlineBuf - diff, cmdlineBuf, cmdlineBufLen);
				cmdlineBuf -= diff;
				memset(cmdlineBuf + cmdlineBufLen, 0, diff);
			}
			free(dup);
		}
		close(fd);
	}
}

#endif /* __APPLE__ */


// -----------------------------------------------------------------------------
//  usage - prints usage message
// -----------------------------------------------------------------------------

static void usage(const char *prog_name)
{
    printf("Usage: %s [options] [script.js [arguments]]\n", prog_name);
    printf("\n");
    printf("Options\n");
    printf("  --transpile               Transpile JavaScript on-the-fly with Babel\n");
    printf("                            This is slow! Use only for testing.\n");
    printf("  --transpile-output        Output the transpiled main file\n");
    printf("  --max-old-space-size=...  Memory limit of JavaScript objects in MB\n");
    printf("\n");
    printf("  -h, --help                Show this message (no other arg allowed)\n");
    printf("  -v, --version             Show low.js version (no other arg allowed)\n");
}


// -----------------------------------------------------------------------------
//  main - program entry point
// -----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    low_t *low;

#ifndef __APPLE__
    handle_dist_loader(&argc, &argv);
#endif /* __APPLE__ */

    if(argc == 2
    && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }
    if(argc == 2
    && (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0))
    {
        printf("%s\n", LOW_VERSION);
        return EXIT_SUCCESS;
    }

    bool optTranspile = false, optTranspileOutput = false;
    char **restArgv = NULL;
    int maxMemSize = 0;

    for(int i = 1; i < argc; i++)
    {
        char maxOldSpaceSize[] = "--max-old-space-size=";

        if(argv[i][0] != '-')
        {
            argc = argc - i + 1;
            restArgv = (char **)malloc(sizeof(char *) * argc);
            restArgv[0] = argv[0];
            memcpy(restArgv + 1, argv + i, sizeof(char *) * (argc - 1));
            argv = restArgv;
        }
        else if(strcmp(argv[i], "--transpile") == 0)
            optTranspile = true;
        else if(strcmp(argv[i], "--transpile-output") == 0)
        {
            optTranspile = true;
            optTranspileOutput = true;
        }
        else if(strlen(argv[i]) > sizeof(maxOldSpaceSize) - 1
        && memcmp(argv[i], maxOldSpaceSize, sizeof(maxOldSpaceSize) - 1) == 0)
        {
            maxMemSize = atoi(argv[i] + sizeof(maxOldSpaceSize) - 1);
            if(maxMemSize <= 0)
            {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
            if(maxMemSize < 4)
                maxMemSize = 4;     // needed for init
        }
        else
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }
    if(!restArgv)
        argc = 1;
    if(optTranspile && argc == 1)
    {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if(!low_system_init(argc, (const char **)(restArgv ? restArgv : argv)))
        return EXIT_FAILURE;

    low = low_init();
    if(!low)
        return EXIT_FAILURE;

    if(!low_lib_init(low))
        goto err;
    if(maxMemSize)
        low->max_heap_size = maxMemSize * 1024 * 1024;
    else
    {
#ifndef __APPLE__
        struct sysinfo info;
        if(sysinfo(&info) < 0)
            goto err;

        int max = info.freeram + info.freeswap / 2;
        if(low->max_heap_size > max)
            low->max_heap_size = max;
#endif /* __APPLE__ */
    }

    if(optTranspile)
    {
        if(!init_transpile(low, optTranspileOutput))
            goto err;
    }

    if(!low_module_main(low, argc > 1 ? argv[1] : NULL))
        goto err;
    if(!low_loop_run(low))
        goto err;

    low_destroy(low);
    low_system_destroy();
    return EXIT_SUCCESS;

err:
    low_destroy(low);
    low_system_destroy();
    return EXIT_FAILURE;
}
