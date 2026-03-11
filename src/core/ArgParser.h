#ifndef ARGPARSER_H
#define ARGPARSER_H

#include "UpdateArgs.h"

class ArgParser
{
public:
    ArgParser() = delete;

    static bool BuildFakeArgs(int& outArgc, char**& outArgv);
    static UpdateArgs Parse(int argc, char* argv[]);
};

#endif
