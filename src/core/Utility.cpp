#include "Utility.h"

#include <kos.h>

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
