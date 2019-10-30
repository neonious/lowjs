// -----------------------------------------------------------------------------
//  main.c
// -----------------------------------------------------------------------------

#ifndef __APPLE__
#define _GNU_SOURCE
#endif /* __APPLE__ */

#include "low.h"

#include <stdlib.h>
#include <stdio.h>

#ifndef __APPLE__
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif /* __APPLE__ */


#ifndef __APPLE__

// -----------------------------------------------------------------------------
//  handle_dist_loader - cleans up environment
// -----------------------------------------------------------------------------

static void handle_dist_loader(int *argc, char ***argv)
{

	if(*argc < 2)
		return;

	const char postfix[] = "../lib/low-exe";
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
        printf("Usage: %s [script.js] [arguments]\n", argv[0]);
        printf("       %s -h | --help | -v | --version\n", argv[0]);
        return EXIT_SUCCESS;
    }

    if(argc == 2
    && (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0))
    {
        printf("%s\n", LOW_VERSION);
        return EXIT_SUCCESS;
    }

    if(!low_system_init(argc, argv))
        return EXIT_FAILURE;

    low = low_init();
    if(!low)
        return EXIT_FAILURE;

    bool ok = false;
    if(low_lib_init(low) && low_module_main(low, argc > 1 ? argv[1] : NULL))
        ok = low_loop_run(low);

    low_destroy(low);
    low_system_destroy();

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
