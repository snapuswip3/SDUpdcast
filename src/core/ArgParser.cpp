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
