#ifndef PATCHER_H
#define PATCHER_H

class Patcher
{
public:
    Patcher() = delete;

    static bool ApplyPatch(const char* oldPath, const char* patchPath, const char* outPath, int newSize);
};

#endif // PATCHER_H
