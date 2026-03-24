#ifndef NETWORK_H
#define NETWORK_H

class Network
{
public:
    Network() = delete;

    static bool Init();
    static bool IsInitialized() { return s_initialized; }
    static void Shutdown();

    using ProgressCallback = void(*)(const char* fmt);

    static bool Dial(ProgressCallback cb = nullptr);
    static bool Download(const char* url, const char* destPath, ProgressCallback cb = nullptr);

private:
    static void Notify(ProgressCallback cb, const char* fmt, ...);

    static bool s_initialized;
};

#endif // NETWORK_H
