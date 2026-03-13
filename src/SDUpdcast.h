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

/* ---------------- Write scratch file ---------------- */

static int SDUpdcast_WriteScratch(const char *returnBin, int argc, char *argv[])
{
    fs_mkdir(SDUPDCAST_DIR);

    file_t f = fs_open(SDUPDCAST_FQFN, O_WRONLY | O_CREAT | O_TRUNC);
    if (f < 0) return 0;

    char buf[256];

    /* write return executable first */
    if (returnBin && *returnBin)
    {
        snprintf(buf, sizeof(buf), "--return %s\n", returnBin);
        fs_write(f, buf, strlen(buf));
    }

    /* write remaining arguments */
    for (int i = 0; i < argc; i++)
    {
        if (!argv || !argv[i] || !*argv[i])
            continue;

        snprintf(buf, sizeof(buf), "%s\n", argv[i]);
        fs_write(f, buf, strlen(buf));
    }

    fs_close(f);
    return 1;
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

    /* count lines */
    int count = 0;
    for (char *p = buffer; *p; p++)
        if (*p == '\n') count++;

    if (size > 0 && buffer[size-1] != '\n')
        count++;

    if (count == 0) { free(buffer); return 0; }

    char **argv = (char **)malloc(sizeof(char *) * (count + 1));
    if (!argv) { free(buffer); return 0; }

    int idx = 0;
    char *line = buffer;
    for (char *p = buffer; *p && idx < count; line = p + 1)
    {
        while (*line == ' ' || *line == '\t' || *line == '\r') line++;
        p = line;
        while (*p && *p != '\n' && *p != '\r') p++;
        if (*p) *p = 0;
        argv[idx++] = line;
    }
    argv[idx] = NULL;

    *outArgc = count;
    *outArgv = argv;
    *outBuffer = buffer;
    return 1;
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

    /* recursion guard */
    if (SDUpdcast_HasSkipUpdate(argc, argv)) return;

    /* confirm SD mount exists */
    if (!SDUpdcast_HasSD()) return;

    /* build fake args only if argc/argv empty and returnBin is populated */
    char **fakeArgv = NULL;
    int fakeArgc = 0;
    char *fakeBuffer = NULL;
    if ((!argv || argc == 0) && returnBin && *returnBin)
    {
        if (SDUpdcast_BuildFakeArgs(&fakeArgc, &fakeArgv, &fakeBuffer))
        {
            argc = fakeArgc;
            argv = fakeArgv;
        }
    }

    /* write scratch file only if returnBin is populated */
    if (returnBin && *returnBin)
    {
        char **newArgv = (char **)malloc(sizeof(char *) * (argc + 2));
        if (!newArgv)
        {
            if (fakeArgv)
            {
                free(fakeBuffer);
                free(fakeArgv);
            }
            return;
        }

        int out = 0;

        if (!SDUpdcast_HasSkipUpdate(argc, argv))
            newArgv[out++] = (char *)"--skip-update";

        for (int i = 0; i < argc; i++)
        {
            if (!argv[i]) continue;
            if (strncmp(argv[i], "--return", 8) == 0) continue;
            if (strcmp(argv[i], "--skip-update") == 0) continue;

            newArgv[out++] = argv[i];
        }

        SDUpdcast_WriteScratch(returnBin, out, newArgv);
        free(newArgv);
    }

    /* free fake argv if allocated */
    if (fakeArgv)
    {
        free(fakeBuffer);
        free(fakeArgv);
    }

    /* load destination binary */
    void *blob;
    ssize_t length = fs_load(destinationBin, &blob);
    if (length <= 0 || !blob) return;

    /* pre-exec hook */
    if (preExecFunc) preExecFunc();

    /* exec destination binary */
    arch_exec(blob, length);
}

#ifdef __cplusplus
}
#endif

#endif
