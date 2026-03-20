#ifndef SDUPDCAST_H
#define SDUPDCAST_H

#include <arch/arch.h>
#include <arch/exec.h>
#include <kos/fs.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================
   Config: shared memory location (top of RAM)
   ========================================================= */
#define SDUPDCAST_SHARED_ADDR ((void*)0x8CFF0000)

/* =========================================================
   Shared block definition
   ========================================================= */
#define SDUPDCAST_MAGIC 0x53445550 /* 'SDUP' */

typedef struct __attribute__((aligned(32))) {
    volatile uint32_t magic;
    volatile uint32_t skip;
    char returnBin[256];
    char overrideBin[256];
    char updateUrl[256];
} SDUpdcast_Block;

/* =========================================================
   Optional pre-exec hook
   ========================================================= */
typedef void (*SDUpdcast_PreExecFunc)(void);

/* =========================================================
   Internal: get shared block
   ========================================================= */
static volatile SDUpdcast_Block *SDUpdcast_Get(void)
{
    return (volatile SDUpdcast_Block*)SDUPDCAST_SHARED_ADDR;
}

/* =========================================================
   Internal: SH4 cache writeback for shared block
   ========================================================= */
static void SDUpdcast_Flush(void)
{
    volatile SDUpdcast_Block *b = SDUpdcast_Get();

    /* flush each 32-byte cache line */
    for (size_t i = 0; i < sizeof(SDUpdcast_Block); i += 32)
    {
        __asm__ volatile("ocbp @%0" : : "r"((uint8_t*)b + i));
    }

    /* compiler barrier */
    __asm__ volatile("" ::: "memory");
}

/* =========================================================
   Internal: clear shared state
   ========================================================= */
static void SDUpdcast_Clear(void)
{
    volatile SDUpdcast_Block *b = SDUpdcast_Get();

    b->magic = 0;
    b->skip  = 0;

    b->returnBin[0]   = 0;
    b->overrideBin[0] = 0;
    b->updateUrl[0]   = 0;

    SDUpdcast_Flush();
}

/* =========================================================
   Internal: write shared state
   ========================================================= */
static void SDUpdcast_Write(
    const char *returnBin,
    const char *overrideBin,
    const char *updateUrl)
{
    volatile SDUpdcast_Block *b = SDUpdcast_Get();

    b->magic = SDUPDCAST_MAGIC;
    b->skip  = 0;

    #define COPY(dst, src)                     \
        if ((src) && *(src)) {                \
            strncpy((char*)(dst), (src), sizeof(dst) - 1); \
            (dst)[sizeof(dst) - 1] = 0;       \
        } else {                              \
            (dst)[0] = 0;                     \
        }

    COPY(b->returnBin, returnBin);
    COPY(b->overrideBin, overrideBin);
    COPY(b->updateUrl, updateUrl);

    #undef COPY

    SDUpdcast_Flush();
}

/* =========================================================
   Internal: read shared state
   ========================================================= */
static int SDUpdcast_Read(
    int *skip,
    const volatile char **returnBin,
    const volatile char **overrideBin,
    const volatile char **updateUrl)
{
    volatile SDUpdcast_Block *b = SDUpdcast_Get();

    if (b->magic != SDUPDCAST_MAGIC)
        return 0;

    if (skip)        *skip        = b->skip;
    if (returnBin)   *returnBin   = b->returnBin;
    if (overrideBin) *overrideBin = b->overrideBin;
    if (updateUrl)   *updateUrl   = b->updateUrl;

    return 1;
}

/* =========================================================
   Internal: set skip flag
   ========================================================= */
static void SDUpdcast_SetSkip(void)
{
    volatile SDUpdcast_Block *b = SDUpdcast_Get();

    b->magic = SDUPDCAST_MAGIC;
    b->skip  = 1;

    SDUpdcast_Flush();
}

/* =========================================================
   Internal: load + exec binary
   ========================================================= */
static void SDUpdcast_Exec(
    const char *bin,
    SDUpdcast_PreExecFunc preExecFunc)
{
    if (!bin || !*bin)
        return;

    file_t fd = fs_open(bin, O_RDONLY);
    if (fd == FILEHND_INVALID)
        return;

    ssize_t length = fs_total(fd);
    if (length <= 0)
    {
        fs_close(fd);
        return;
    }

    void *blob = memalign(32, (size_t)length);
    if (!blob)
    {
        fs_close(fd);
        return;
    }

    ssize_t total = 0;
    while (total < length)
    {
        ssize_t r = fs_read(fd,
                            (uint8_t *)blob + total,
                            (size_t)(length - total));
        if (r <= 0)
        {
            fs_close(fd);
            free(blob);
            return;
        }
        total += r;
    }

    fs_close(fd);

    if (preExecFunc)
        preExecFunc();

    /* ===== ensure all memory writes are visible ===== */
    __asm__ volatile("" ::: "memory");

    arch_exec(blob, (size_t)length);
}

/* =========================================================
   PUBLIC: Run updater (main entry point)
   ========================================================= */
static void SDUpdcast_RunUpdater(
    const char *destinationBin,
    const char *returnBin,
    const char *overrideBin,
    const char *updateUrl,
    SDUpdcast_PreExecFunc preExecFunc)
{
    if (!destinationBin || !*destinationBin)
        return;

    int skip = 0;

    if (SDUpdcast_Read(&skip, NULL, NULL, NULL))
    {
        if (skip)
        {
            SDUpdcast_Clear();
            return;
        }
    }

    SDUpdcast_Write(returnBin, overrideBin, updateUrl);

    SDUpdcast_Exec(destinationBin, preExecFunc);
}

/* =========================================================
   PUBLIC: Call early in updater/main to read inputs
   ========================================================= */
static int SDUpdcast_GetParams(
    char *returnBin,
    char *overrideBin,
    char *updateUrl)
{
    volatile SDUpdcast_Block *b = SDUpdcast_Get();

    if (b->magic != SDUPDCAST_MAGIC)
        return 0;

    if (returnBin)
    {
        strncpy(returnBin, (const char*)b->returnBin, 255);
        returnBin[255] = 0;
    }

    if (overrideBin)
    {
        strncpy(overrideBin, (const char*)b->overrideBin, 255);
        overrideBin[255] = 0;
    }

    if (updateUrl)
    {
        strncpy(updateUrl, (const char*)b->updateUrl, 255);
        updateUrl[255] = 0;
    }

    return 1;
}

/* =========================================================
   PUBLIC: Call before returning to main app
   ========================================================= */
static void SDUpdcast_PrepareReturn(void)
{
    SDUpdcast_SetSkip();
}

#ifdef __cplusplus
}
#endif

#endif
