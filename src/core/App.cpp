#include "App.h"

#include "../config.h"
#include "../SDUpdcast.h"
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

bool App::Init()
{
    fs_fat_mount_sd();

#ifdef DEBUG_SD_LOG
    Logger::Init(LOG_FQFN);
#endif

    if (!SDUpdcast_GetParams(m_returnBin, m_overrideBin, m_updateUrl))
    {
        Logger::LogError("Couldn't retrieve parameters. Exiting.");
        return false;
    }

#ifndef DEBUG
    cdrom_init();
    fs_iso9660_init();
#endif

    Logger::LogInfo("Return bin: %s", m_returnBin);
    Logger::LogInfo("Override bin: %s", m_overrideBin);
    Logger::LogInfo("Update URL: %s", m_updateUrl);

    Logger::LogInfo("App initialized");

    return true;
}

void App::Run()
{
#ifdef DEBUG
    malloc_stats();
#endif

    Logger::LogInfo("Returning to target: %s", m_returnBin);

    SDUpdcast_RunUpdater(
        m_returnBin,
        nullptr,
        nullptr,
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
    SDUpdcast_PrepareReturn();

    Logger::LogInfo("App shutting down (callback)");

    Logger::Shutdown();
#ifdef DEBUG
    malloc_stats();
#endif
    fs_fat_unmount_sd();
}
