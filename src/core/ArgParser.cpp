#include "ArgParser.h"

#include "../config.h"
#include "Logger.h"
#include "Utility.h"

#include <kos/fs.h>

#include <cstdlib>
#include <cstring>

// Memory-allocated argv builder from /sd/SDUpdcast/running.cfg
bool ArgParser::BuildFakeArgs(int& outArgc, char**& outArgv)
{
    file_t f = fs_open(ARGS_FQFN, O_RDONLY);
    if (f < 0)
    {
        Logger::LogWarn("No running.cfg found, cannot build fake argv");
        return false;
    }

    int size = fs_total(f);
    if (size <= 0)
    {
        Logger::LogWarn("running.cfg is empty");
        fs_close(f);
        return false;
    }

    char* buffer = (char*)malloc(size + 1);
    if (!buffer)
    {
        Logger::LogError("Failed to allocate buffer for running.cfg");
        fs_close(f);
        return false;
    }

    int read = fs_read(f, buffer, size);
    fs_close(f);

    if (read != size)
    {
        Logger::LogError("Failed to read full running.cfg (%d/%d bytes)", read, size);
        free(buffer);
        return false;
    }

    buffer[size] = 0; // null-terminate

    // Count tokens (space/tab/newline separated)
    int tokenCount = 0;
    char* p = buffer;
    char* end = buffer + size;

    while (p < end)
    {
        Utility::SkipWhitespace((const char*&)p, end);
        if (p >= end) break;

        tokenCount++;
        while (p < end && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
    }

    if (tokenCount == 0)
    {
        free(buffer);
        return false;
    }

    char** argv = (char**)malloc(sizeof(char*) * (tokenCount + 1));
    if (!argv)
    {
        Logger::LogError("Failed to allocate argv array");
        free(buffer);
        return false;
    }

    // Fill argv array with pointers into buffer
    p = buffer;
    int idx = 0;
    while (p < end && idx < tokenCount)
    {
        Utility::SkipWhitespace((const char*&)p, end);
        if (p >= end) break;

        char* start = p;
        while (p < end && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') ++p;

        if (p < end) *p++ = 0; // null-terminate
        argv[idx++] = start;
    }

    argv[idx] = nullptr;
    outArgc = tokenCount;
    outArgv = argv;

    Logger::LogInfo("Built fake argv from running.cfg with %d tokens", tokenCount);
    return true;
}

UpdateArgs ArgParser::Parse(int argc, char* argv[])
{
    UpdateArgs args;

    Logger::LogInfo("ArgParser: argc=%d", argc);

    for (int i = 0; i < argc; i++)
    {
        Logger::LogInfo("argv[%d]=%s", i, argv[i]);
    }

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--return") == 0 && i + 1 < argc)
        {
            args.returnPath = argv[++i];
            Logger::LogInfo("Return path parsed: %s", args.returnPath);
        }
        else if (strcmp(argv[i], "--skip-update") == 0)
        {
            Logger::LogWarn("Updater invoked with --skip-update");
            args.skipUpdate = true;
        }
    }

    return args;
}
