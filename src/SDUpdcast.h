#ifndef SDUPDCAST_H
#define SDUPDCAST_H

#include <arch/arch.h>
#include <arch/exec.h>
#include <kos/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SDUPDCAST_DIR "/sd/SDUpdcast"
#define SDUPDCAST_FQFN SDUPDCAST_DIR "/running.cfg"

typedef void (*SDUpdcast_PreExecFunc)(void);

/* ---------------- Recursion / SD checks ---------------- */
static int SDUpdcast_HasSkipUpdate(int argc, char *argv[])
{
    if (!argv) return 0;
    for (int i = 0; i < argc; i++)
        if (strcmp(argv[i], "--skip-update") == 0)
            return 1;
    return 0;
}

static int SDUpdcast_HasSD(void)
{
    struct stat st;
    if (fs_stat("/sd", &st, 0) < 0) return 0;
    return S_ISDIR(st.st_mode);
}

/* ---------------- Build fake argv from running.cfg ---------------- */
static int SDUpdcast_BuildFakeArgs(int *outArgc, char ***outArgv, char **outBuffer)
{
    file_t f = fs_open(SDUPDCAST_FQFN, O_RDONLY);
    if (f < 0) return 0;

    int size = fs_total(f);
    if (size <= 0) { fs_close(f); return 0; }

    char *buffer = (char *)malloc(size + 1);
    if (!buffer) { fs_close(f); return 0; }

    int read = fs_read(f, buffer, size);
    fs_close(f);
    if (read != size) { free(buffer); return 0; }

    buffer[size] = 0;

    char **argv = (char **)malloc(sizeof(char *) * (size / 2 + 2));
    if (!argv) { free(buffer); return 0; }

    int argc = 0;
    char *p = buffer;

    while (*p)
    {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;
        if (*p) { *p = 0; p++; }
    }

    argv[argc] = NULL;

    if (argc == 0)
    {
        free(argv);
        free(buffer);
        return 0;
    }

    *outArgc = argc;
    *outArgv = argv;
    *outBuffer = buffer;

    return 1;
}

/* ---------------- Write scratch file ---------------- */
static int SDUpdcast_WriteScratch(const char *returnBin, int argc, char *argv[])
{
    fs_mkdir(SDUPDCAST_DIR);

    file_t f = fs_open(SDUPDCAST_FQFN, O_WRONLY | O_CREAT | O_TRUNC);
    if (f < 0) return 0;

    char buf[256];
    int outWritten = 0;

    if (returnBin && *returnBin)
    {
        snprintf(buf, sizeof(buf), "--return %s\n", returnBin);
        fs_write(f, buf, strlen(buf));
        outWritten++;
    }

    if (!returnBin || !*returnBin)
    {
        snprintf(buf, sizeof(buf), "--skip-update\n");
        fs_write(f, buf, strlen(buf));
        outWritten++;
    }

    for (int i = 0; i < argc; i++)
    {
        if (!argv[i] || !*argv[i]) continue;
        if (strcmp(argv[i], "--return") == 0) { i++; continue; }
        if (strcmp(argv[i], "--skip-update") == 0) continue;
        snprintf(buf, sizeof(buf), "%s\n", argv[i]);
        fs_write(f, buf, strlen(buf));
        outWritten++;
    }

    fs_close(f);
    return outWritten > 0;
}

/* ---------------- Run updater / destination binary ---------------- */
static void SDUpdcast_RunUpdater(
    int argc,
    char *argv[],
    const char *destinationBin,
    const char *returnBin,
    SDUpdcast_PreExecFunc preExecFunc)
{
    if (!destinationBin || !*destinationBin) return;
    if (!SDUpdcast_HasSD()) return;

    // Build fake args if nothing supplied
    char **fakeArgv = NULL;
    int fakeArgc = 0;
    char *fakeBuffer = NULL;
    if (!argv || argc == 0)
    {
        if (SDUpdcast_BuildFakeArgs(&fakeArgc, &fakeArgv, &fakeBuffer))
        {
            argc = fakeArgc;
            argv = fakeArgv;
        }
    }

    // recursion guard
    if (SDUpdcast_HasSkipUpdate(argc, argv))
    {
        // delete scratch file so we don't loop
        fs_unlink(SDUPDCAST_FQFN);

        // free any fake argv memory if allocated
        if (fakeArgv)
        {
            free(fakeBuffer);
            free(fakeArgv);
        }

        return;
    }

    // Write scratch (handles --return or --skip-update)
    SDUpdcast_WriteScratch(returnBin, argc, argv);

    if (fakeArgv) { free(fakeBuffer); free(fakeArgv); }

    // Load and execute destination
    void *blob;
    ssize_t length = fs_load(destinationBin, &blob);
    if (length <= 0 || !blob) return;

    if (preExecFunc) preExecFunc();

    arch_exec(blob, length);
}

#ifdef __cplusplus
}
#endif

#endif
