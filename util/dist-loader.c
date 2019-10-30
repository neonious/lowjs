// loader to be able to distribute dynamic libs with executable
// used when creating distributable with make dist

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    char ldPath[] = "../lib";
    char ld[] = "../lib/low";
    char bin[] = "../lib/low-exe";
    char fName[1024];
    char argv0[32];
    sprintf(argv0, "/proc/%d/exe", getpid());
    memset(fName, 0, sizeof(fName));
    readlink(argv0, fName, sizeof(fName) - 1);
    int i;
    for(i = strlen(fName); i > 0; i--)
            if(fName[i - 1] == '/')
                    break;
    char *path = malloc(i + sizeof(ld));
    memcpy(path, fName, i);
    strcpy(path + i, ldPath);
    setenv("LD_LIBRARY_PATH", path, 1);
    strcpy(path + i, ld);
    char **argvNew = malloc((argc + 3) * sizeof(char *));
    argvNew[0] = path;
    argvNew[1] = malloc(i + sizeof(bin));
    memcpy(argvNew[1], fName, i);
    strcpy(argvNew[1] + i, bin);
    for(i = 0; i < argc; i++)
            argvNew[i + 2] = argv[i];
    argvNew[argc + 2] = NULL;
printf("PATH IS %s\n", path);
    for(i = 0; argvNew[i]; i++)
printf("%d %s\n", i, argvNew[i]);
    execv(path, argvNew);
    fprintf(stderr, "Cannot execute low main binary!\n");
    return EXIT_FAILURE;
}
