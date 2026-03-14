#include "ArgParser.h"

#include "../config.h"
#include "Logger.h"

#include <cstdlib>
#include <cstring>

UpdateArgs ArgParser::Parse(int argc, char* argv[])
{
    UpdateArgs args;

    Logger::LogInfo("ArgParser: argc=%d", argc);

    for (int i = 0; i < argc; i++)
    {
        Logger::LogInfo("argv[%d]=%s", i, argv[i] ? argv[i] : "(null)");
    }

    for (int i = 0; i < argc; i++)
    {
        char *arg = argv[i];
        if (!arg) continue;

        if (strcmp(arg, "--return") == 0 && i + 1 < argc && argv[i + 1])
        {
            args.returnPath = argv[++i];
            Logger::LogInfo("Return path parsed: %s", args.returnPath);
        }
        else if (strcmp(arg, "--skip-update") == 0)
        {
            Logger::LogWarn("Updater invoked with --skip-update");
            args.skipUpdate = true;
        }
    }

    return args;
}
