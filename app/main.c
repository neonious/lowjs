// -----------------------------------------------------------------------------
//  main.c
// -----------------------------------------------------------------------------

#include "low.h"

#include <stdlib.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
//  main - program entry point
// -----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    low_t *low;

    if(argc > 2)
    {
        fprintf(stderr, "Syntax: %s [file.js]\n", argc ? argv[0] : "lowjs");
        return EXIT_FAILURE;
    }

    if(argc == 2
    && (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0))
    {
        printf("%s\n", LOW_VERSION);
        return EXIT_SUCCESS;
    }

    if(!low_system_init())
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
