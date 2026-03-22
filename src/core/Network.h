#ifndef NETWORK_H
#define NETWORK_H

#include <cstdint>

class Network
{
public:
    Network() = delete;

    static bool Init();
    static void Shutdown();
    static bool IsInitialized();

    using ProgressCallback = void(*)(const char* fmt);

    static bool Download(const char* url, const char* destPath, ProgressCallback cb = nullptr);

private:
    static bool s_initialized;
};

#endif // NETWORK_H
