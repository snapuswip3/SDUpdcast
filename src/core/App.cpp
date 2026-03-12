#include "App.h"

#include "../config.h"
#include "../SDUpdcast.h"
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

App* App::s_instance = nullptr;

bool App::Init(int argc, char *argv[])
{
    fs_fat_mount_sd();

#ifdef DEBUG_SD_LOG
    Logger::Init(LOG_FQFN);
#endif

    char **fakeArgv = nullptr;
    int fakeArgc = 0;

    if (!argv || argc == 0)
    {
        if (SDUpdcast_BuildFakeArgs(&fakeArgc, &fakeArgv))
        {
            argc = fakeArgc;
            argv = fakeArgv;
            m_ownsArgv = true; // mark that we own this memory
        }
    }

    m_argc = argc;
    m_argv = argv;

    m_args = ArgParser::Parse(argc, argv);

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

    if (m_args.skipUpdate)
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

    Logger::LogInfo("Returning to target: %s", m_args.returnPath);

    SDUpdcast_RunUpdater(
        m_argc,
        m_argv,
        m_args.returnPath,
        nullptr,
        Cleanup
    );
}

void App::Shutdown()
{
    Cleanup();
    sd_shutdown();
}

void App::Cleanup()
{
    if (!s_instance) return;

    Logger::LogInfo("App shutting down (callback)");

    if (s_instance->m_ownsArgv && s_instance->m_argv)
    {
        free(s_instance->m_argv[0]);
        free(s_instance->m_argv);
        s_instance->m_argv = nullptr;
        s_instance->m_ownsArgv = false;
    }

    Logger::Shutdown();
#ifdef DEBUG
    malloc_stats();
#endif
    fs_fat_unmount_sd();
}
