#ifndef UTILITY_H
#define UTILITY_H

#include <cstdbool>
#include <cstdint>
#include <cstdlib>

class Utility
{
public:
    Utility() = delete;

    enum class MakeDirResult
    {
        Success = 0,
        InvalidPath,
        OutOfMemory,
        MkdirFailed
    };

    static MakeDirResult MakeParentDir(const char* path);
    static bool Md5File(const char* path, char out[33]);
    static bool CopyFile(const char* srcPath, const char* destPath, char* buffer, int bufferSize);

    static inline void SkipWhitespace(const char*& p, const char* end)
    {
        while (p < end && (*p == ' ' || *p == '\t')) ++p;
    }

    static inline float ParseFloat(const char*& p, const char* end)
    {
        char* next = nullptr;
        float f = strtof(p, &next);
        p = next;
        return f;
    }

    static inline int ParseInt(const char*& p, const char* end)
    {
        char* next = nullptr;
        int i = strtol(p, &next, 10);
        p = next;
        return i;
    }

    static inline void SkipLine(const char*& p, const char* end)
    {
        while (p < end && *p != '\n' && *p != '\r') ++p;
        if (p < end && *p == '\n') ++p;
    }

    static inline bool ParseToken(const char*& p, const char* end, char* out, int outSize)
    {
        SkipWhitespace(p, end);

        if (p >= end)
            return false;

        int i = 0;
        while (p < end && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
        {
            if (i + 1 < outSize) out[i++] = *p;
            ++p;
        }

        out[i] = 0;
        return i > 0;
    }

    static inline bool Contains(const char* haystack, const char* needle)
    {
        if (!haystack || !needle || *needle == '\0') return false;

        for (const char* h = haystack; *h != '\0'; h++)
        {
            const char* hh = h;
            const char* nn = needle;

            while (*hh != '\0' && *nn != '\0' && *hh == *nn)
            {
                hh++;
                nn++;
            }

            if (*nn == '\0')
                return true;
        }

        return false;
    }
};

#endif // UTILITY_H
