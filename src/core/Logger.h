#ifndef LOGGER_H
#define LOGGER_H

#include <cstdarg>

class Logger
{
public:
    Logger() = delete;

    static void Init(const char* path);
    static void Shutdown();
    static void KeepStdioAlive();
    static void LogInfo(const char* fmt, ...);
    static void LogWarn(const char* fmt, ...);
    static void LogError(const char* fmt, ...);

private:
    static void Log(const char* level, const char* fmt, va_list args);
    static void GetTimestamp(char* buffer, int bufferSize);
};

#endif // LOGGER_H
