#include "Utility.h"

#include <kos.h>
#include <kos/md5.h>

#include <cstring>

Utility::MakeDirResult Utility::MakeParentDir(const char *path)
{
    if (!path || !*path)
        return MakeDirResult::InvalidPath;

    char *fullPath = strdup(path);
    if (!fullPath)
        return MakeDirResult::OutOfMemory;

    size_t fullLen = strlen(fullPath);
    while (fullLen > 0 &&
          (fullPath[fullLen - 1] == ' ' ||
           fullPath[fullLen - 1] == '\n' ||
           fullPath[fullLen - 1] == '\r'))
    {
        fullPath[--fullLen] = '\0';
    }

    char *lastSlash = strrchr(fullPath, '/');
    if (!lastSlash || lastSlash == fullPath)
    {
        free(fullPath);
        return MakeDirResult::Success;
    }

    *lastSlash = '\0';

    char *mountSlash = strchr(fullPath + 1, '/');
    if (!mountSlash)
    {
        free(fullPath);
        return MakeDirResult::Success;
    }

    size_t subdirStartPos = (mountSlash - fullPath) + 1;

    char *segment = strchr(fullPath + subdirStartPos, '/');
    while (segment)
    {
        *segment = '\0';
        struct stat st;
        if (fs_stat(fullPath, &st, 0) < 0 && fs_mkdir(fullPath) < 0)
        {
            free(fullPath);
            return MakeDirResult::MkdirFailed;
        }
        *segment = '/';
        segment = strchr(segment + 1, '/');
    }

    struct stat st;
    if (fs_stat(fullPath, &st, 0) < 0 && fs_mkdir(fullPath) < 0)
    {
        free(fullPath);
        return MakeDirResult::MkdirFailed;
    }

    free(fullPath);
    return MakeDirResult::Success;
}

bool Utility::Md5File(const char* path, char out[33])
{
    file_t fd = fs_open(path, O_RDONLY);
    if (fd == FILEHND_INVALID)
        return false;

    kos_md5_cxt_t ctx;
    kos_md5_start(&ctx);

    uint8_t buffer[8192];
    uint8_t hash[16];
    ssize_t r;

    while ((r = fs_read(fd, buffer, sizeof(buffer))) > 0)
    {
        kos_md5_hash_block(&ctx, buffer, (uint32_t)r);
    }

    fs_close(fd);

    if (r < 0)
        return false;

    kos_md5_finish(&ctx, hash);

    static const char* hex = "0123456789abcdef";

    for (int i = 0; i < 16; ++i)
    {
        out[i * 2]     = hex[(hash[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[hash[i] & 0xF];
    }

    out[32] = '\0';

    return true;
}

bool Utility::CopyFile(const char* srcPath, const char* destPath, char* buffer, int bufferSize)
{
    if (!buffer || bufferSize <= 0)
        return false;

    file_t src = fs_open(srcPath, O_RDONLY);
    if (src < 0)
        return false;

    MakeParentDir(destPath);

    file_t dest = fs_open(destPath, O_WRONLY | O_CREAT | O_TRUNC);
    if (dest < 0) {
        fs_close(src);
        return false;
    }

    int bytesRead = 0;
    while ((bytesRead = fs_read(src, buffer, bufferSize)) > 0) {
        int written = 0;
        while (written < bytesRead) {
            int ret = fs_write(dest, buffer + written, bytesRead - written);
            if (ret <= 0) {
                fs_close(src);
                fs_close(dest);
                return false;
            }
            written += ret;
        }
    }

    if (bytesRead < 0) {
        fs_close(src);
        fs_close(dest);
        return false;
    }

    fs_close(src);
    fs_close(dest);
    return true;
}
