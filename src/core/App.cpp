#include "App.h"

#include "../config.h"
#include "ArgParser.h"
#include "Logger.h"

extern "C" {
#include <fatfs.h>
}
#include <dc/sd.h>
#include <kos/fs.h>
#include <kos.h>

#include <cstdint>

#ifdef DEBUG
#include <inttypes.h>
#include <malloc.h>
#endif

bool App::Init(int argc, char *argv[])
{
    fs_fat_mount_sd();

#ifdef DEBUG_SD_LOG
    Logger::Init(LOG_FQFN);
#endif

    char** fakeArgv = nullptr;
    int fakeArgc = 0;

    if (!argv || argc == 0)
    {
        if (ArgParser::BuildFakeArgs(fakeArgc, fakeArgv))
        {
            argc = fakeArgc;
            argv = fakeArgv;
        }
    }

    m_args = ArgParser::Parse(argc, argv);

    if (fakeArgv)
    {
        free(fakeArgv[0]); // the buffer containing tokens
        free(fakeArgv);
    }

#ifndef DEBUG
    cdrom_init();
    fs_iso9660_init();
#endif

    if (m_args.returnPath)
    {
        Logger::LogInfo("Return target: %s", m_args.returnPath);
    }
    else
    {
        Logger::LogError("No return path provided. Exiting.");
        return false;
    }

    if (!m_args.skipUpdate)
    {
        Logger::LogError("Updater invoked with --skip-update. Exiting.");
        return false;
    }

    Logger::LogInfo("App initialized");

    return true;
}

void App::Run()
{
#ifdef DEBUG
    malloc_stats();
#endif

    void *blob;
    ssize_t length = fs_load(m_args.returnPath, &blob);
    if (length > 0)
    {
        Logger::LogInfo("Returning to target: %s", m_args.returnPath);
        arch_exec(blob, length);
    }
    else
    {
        Logger::LogError("Could not load binary: %s", m_args.returnPath);
    }
}

void App::Shutdown()
{
    Logger::LogInfo("App shutting down");

    Logger::Shutdown();
#ifdef DEBUG
    malloc_stats();
#endif
    fs_fat_unmount_sd();
    sd_shutdown();
}
