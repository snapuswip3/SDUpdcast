#ifndef NETWORK_H
#define NETWORK_H

class Network
{
public:
    Network() = delete;

    static bool Init();
    static void Shutdown();

    using ProgressCallback = void(*)(const char* fmt);

    static bool Download(const char* url, const char* destPath, ProgressCallback cb = nullptr);

private:
    static void Notify(ProgressCallback cb, const char* fmt, ...);
};

#endif // NETWORK_H
