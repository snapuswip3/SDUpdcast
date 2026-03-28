#include "Patcher.h"

#include "Logger.h"
#include "../bspatch/bspatchlib.h"

#include <kos.h>

#include <cstdint>

bool Patcher::ApplyPatch(const char* oldPath, const char* patchPath, const char* outPath, int newSize)
{
    if (!oldPath || !patchPath || !outPath) return false;

    file_t fOld = -1, fPatch = -1, fOut = -1;
    uint8_t *oldBuf = nullptr, *patchBuf = nullptr, *newBuf = nullptr;
    int oldSize = 0, patchSize = 0;
    char *err = nullptr;

    auto read_full = [](file_t f, uint8_t* buf, int len) -> bool {
        int total = 0;
        while (total < len) {
            int r = fs_read(f, buf + total, len - total);
            if (r <= 0) return false;
            total += r;
        }
        return true;
    };

    auto write_full = [](file_t f, const uint8_t* buf, int len) -> bool {
        int total = 0;
        while (total < len) {
            int w = fs_write(f, buf + total, len - total);
            if (w <= 0) return false;
            total += w;
        }
        return true;
    };

    auto get_file_size = [](file_t f) -> int {
        int cur = fs_seek(f, 0, SEEK_CUR);
        int end = fs_seek(f, 0, SEEK_END);
        fs_seek(f, cur, SEEK_SET);
        return end;
    };

    // Load old file into memory
    fOld = fs_open(oldPath, O_RDONLY);
    if (fOld < 0) { Logger::LogError("ApplyPatch: open old failed"); goto cleanup; }
    oldSize = get_file_size(fOld);
    fs_seek(fOld, 0, SEEK_SET);
    oldBuf = (uint8_t*)malloc(oldSize);
    if (!oldBuf) { Logger::LogError("ApplyPatch: alloc oldBuf failed"); goto cleanup; }
    if (!read_full(fOld, oldBuf, oldSize)) { Logger::LogError("ApplyPatch: read old failed"); goto cleanup; }
    fs_close(fOld); fOld = -1;

    // Load patch file into memory
    fPatch = fs_open(patchPath, O_RDONLY);
    if (fPatch < 0) { Logger::LogError("ApplyPatch: open patch failed"); goto cleanup; }
    patchSize = get_file_size(fPatch);
    fs_seek(fPatch, 0, SEEK_SET);
    patchBuf = (uint8_t*)malloc(patchSize);
    if (!patchBuf) { Logger::LogError("ApplyPatch: alloc patchBuf failed"); goto cleanup; }
    if (!read_full(fPatch, patchBuf, patchSize)) { Logger::LogError("ApplyPatch: read patch failed"); goto cleanup; }
    fs_close(fPatch); fPatch = -1;

    // Apply patch in memory
    err = bspatch_mem(oldBuf, oldSize, &newBuf, &newSize,
                            patchBuf, patchSize,
                            -1, -1, -1);
    if (err) { Logger::LogError(err); goto cleanup; }

    // Write new file
    fOut = fs_open(outPath, O_WRONLY | O_CREAT | O_TRUNC);
    if (fOut < 0) { Logger::LogError("ApplyPatch: open out failed"); goto cleanup; }
    if (!write_full(fOut, (uint8_t*)newBuf, newSize)) { Logger::LogError("ApplyPatch: write failed"); goto cleanup; }
    fs_close(fOut); fOut = -1;

    free(oldBuf); free(patchBuf); free(newBuf);
    return true;

cleanup:
    if (fOld  >= 0) fs_close(fOld);
    if (fPatch>= 0) fs_close(fPatch);
    if (fOut  >= 0) fs_close(fOut);
    free(oldBuf); free(patchBuf); free(newBuf);
    return false;
}
