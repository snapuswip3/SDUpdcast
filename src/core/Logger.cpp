#include "Logger.h"

#include "Utility.h"

#include <fcntl.h>
#include <kos/fs.h>
#include <kos.h>

#include <cstdio>
#include <cstring>
#include <ctime>

static file_t g_logFile = -1;

void Logger::Init(const char* path)
{
    if (!path || !*path)
        return;

    Utility::MakeParentDir(path);

    g_logFile = fs_open(path, O_WRONLY);

    if (g_logFile < 0)
    {
        g_logFile = fs_open(path, O_WRONLY | O_CREAT | O_TRUNC);
    }

    if (g_logFile < 0)
    {
        printf("Logger: failed to open/create %s\n", path);
        return;
    }

    fs_seek(g_logFile, 0, SEEK_END);
}

void Logger::Shutdown()
{
    if (g_logFile >= 0)
    {
        fs_close(g_logFile);
        g_logFile = -1;
    }
}

void Logger::KeepStdioAlive()
{
    static uint64_t lastSecond = 0;
    static uint64_t nextCheckMs = 0;

    uint64_t nowMs = timer_ms_gettime64();

    if (nowMs >= nextCheckMs)
    {
        putchar('\0');
        fflush(stdout);
        lastSecond++;
        nextCheckMs = (lastSecond + 1) * 1000;
    }
}

void Logger::LogInfo(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Log("INFO", fmt, args);
    va_end(args);
}

void Logger::LogWarn(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Log("WARN", fmt, args);
    va_end(args);
}

void Logger::LogError(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Log("ERROR", fmt, args);
    va_end(args);
}

void Logger::Log(const char* level, const char* fmt, va_list args)
{
    char timeBuf[32];
    GetTimestamp(timeBuf, sizeof(timeBuf));

    char msgBuf[512];
    int written = vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);

    bool wasTruncated = (written >= (int)sizeof(msgBuf));

    if (wasTruncated)
    {
        const char* note = " ... (truncated)";
        const size_t noteLen = strlen(note);
        const size_t bufSize = sizeof(msgBuf);

        if (noteLen + 1 < bufSize)
        {
            size_t start = bufSize - noteLen - 1;
            memcpy(msgBuf + start, note, noteLen);
            msgBuf[bufSize - 1] = '\0';
        }
    }

    char fullBuf[600];
    snprintf(fullBuf, sizeof(fullBuf), "[%s] [%s] %s\n", timeBuf, level, msgBuf);

    printf("%s", fullBuf);

    if (g_logFile >= 0)
    {
        size_t len = strlen(fullBuf);
        size_t written = 0;

        while (written < len) {
            ssize_t ret = fs_write(g_logFile, fullBuf + written, len - written);
            if (ret <= 0) {
                break;
            }
            written += ret;
        }
    }
}

void Logger::GetTimestamp(char* buffer, int bufferSize)
{
    time_t now = time(0);
    struct tm t;

    localtime_r(&now, &t);

    strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S", &t);
}
