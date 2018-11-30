// low.js Duktape precompiler

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "duktape.h"

// Stubs, used by DukTape to call lowjs_esp32
// Are unused in POSIX version of lowjs
char user_lock_debugstream(char lock, char block)
{
    return 0;
}
void user_broken_debugstream() {}
int neoniousGetStackFree()
{
    return 10000000;
}
void duk_compress_stack(duk_context *ctx, duk_ret_t (*func)(duk_context *ctx, void *udata), void *udata)
{ printf("should not be called!\n"); }
void code_print_error() {}
void code_gc() {}

int pathStartLen;

static void fatal_handler(void *udata, const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}

char handle1(const char *fPathIn, const char *fPathOut)
{
    // Get file name
    int pos = strlen(fPathIn);
    char file_name[pos + 5];
    int end = pos;
    for(; pos > 0; pos--)
    {
        if(fPathIn[pos] == '.')
            end = pos;
        if(fPathIn[pos - 1] == '/' || fPathIn[pos - 1] == '\\')
            break;
    }
    pos = pathStartLen + 1;

    memcpy(file_name, "lib:", 4);
    memcpy(file_name + 4, fPathIn + pos, end - pos);
    file_name[4 + end - pos] = '\0';

    printf("Writing %s...\n", file_name);

    // Read in file
    FILE *f = fopen(fPathIn, "r");
    if(!f)
    {
        fprintf(stderr, "Cannot open '%s' for reading!\n", fPathIn);
        fclose(f);
        return 0;
    }

    int len;
    if(fseek(f, 0, SEEK_END) < 0 || (len = ftell(f)) < 0 ||
       fseek(f, 0, SEEK_SET) < 0)
    {
        fprintf(stderr, "Cannot get file size of '%s'!\n", fPathIn);
        fclose(f);
        return 0;
    }

    char *data = (char *)malloc(1024 + len);
    if(!data)
    {
        fprintf(stderr, "Memory full!\n");
        fclose(f);
        return 0;
    }

    // Add CommonJS wrapper
    strcpy(data, "function(exports,require,module,__filename,__dirname){");
    pos = strlen(data);

    if(len && fread(data + pos, len, 1, f) != 1)
    {
        fprintf(stderr, "Cannot read '%s'!\n", fPathIn);
        fclose(f);
        return 0;
    }

    strcpy(data + pos + len, "}");
    len += pos + 1;

    // Setup DukTape
    duk_context *heap = duk_create_heap(NULL, NULL, NULL, NULL, fatal_handler);
    if(!heap)
    {
        fprintf(stderr, "Cannot create DukTape heap!\n");
        return 0;
    }

    duk_push_string(heap, file_name);
    duk_compile_lstring_filename(
      heap, DUK_COMPILE_FUNCTION | DUK_COMPILE_STRICT, data, len);
    free(data);
    duk_dump_function(heap);

    duk_size_t outLen = 0;
    data = (char *)duk_get_buffer_data(heap, 0, &outLen);
    if(!data || !outLen)
    {
        fprintf(stderr, "DukTape error!\n");
        return 0;
    }

    f = fopen(fPathOut, "w");
    if(!f)
    {
        fprintf(stderr, "Cannot open '%s' for writing!\n", fPathOut);
        return 0;
    }
    if(fwrite(data, outLen, 1, f) != 1)
    {
        fprintf(stderr, "Cannot write '%s'!\n", fPathOut);
        fclose(f);
        return 0;
    }
    fclose(f);

    printf("Written %s.\n", file_name);
    return 1;
}

char handle_dir(const char *pathIn, const char *pathOut)
{
    DIR *dir = opendir(pathIn);
    if(!dir)
    {
        fprintf(stderr, "Cannot open '%s' for traversing!\n", pathIn);
        return 0;
    }

    int lenIn = strlen(pathIn);
    int lenOut = strlen(pathOut);
    struct dirent *ent;

    while((ent = readdir(dir)) != NULL)
    {
        if(ent->d_name[0] == '.')
            continue;

        int len = strlen(ent->d_name);
        char pathInFile[lenIn + strlen(ent->d_name) + 2];
        sprintf(pathInFile, "%s/%s", pathIn, ent->d_name);
        char pathOutFile[lenOut + strlen(ent->d_name) + 5];
        sprintf(pathOutFile, "%s/%s", pathOut, ent->d_name);

        if(ent->d_type == DT_DIR)
        {
            mkdir(pathOutFile, 0755);
            if(!handle_dir(pathInFile, pathOutFile))
                return 0;
        }
        else
        {
            int pos = strlen(pathOutFile);
            for(; pos > 0; pos--)
            {
                if(pathOutFile[pos] == '.')
                {
                    strcpy(pathOutFile + pos, ".low");
                }
                if(pathOutFile[pos - 1] == '/' || pathOutFile[pos - 1] == '\\')
                    break;
            }

            if(!handle1(pathInFile, pathOutFile))
                return 0;
        }
    }
    closedir(dir);
    return 1;
}

int main(int argc, char **argv)
{
    if(argc != 3)
    {
        fprintf(stderr, "Syntax: %s in-dir out-dir\n", argc ? argv[0] : "dukc");
        return EXIT_FAILURE;
    }

    pathStartLen = strlen(argv[1]);
    if(!handle_dir(argv[1], argv[2]))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
