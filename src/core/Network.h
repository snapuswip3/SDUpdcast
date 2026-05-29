#ifndef NETWORK_H
#define NETWORK_H

class Network
{
public:
    Network() = delete;

    enum class DownloadResult
    {
        Success = 0,
        NoNeed,
        Failure
    };

    static bool Init();
    static bool EthernetConnected() { return s_ethernetConnected; }
    static void Shutdown();

    using ProgressCallback = void(*)(const char* fmt);

    static bool Dial(ProgressCallback cb = nullptr);
    static DownloadResult Download(const char* url, const char* localPath, const char* destPath, ProgressCallback cb = nullptr, const char* localMd5 = nullptr);

private:
    static void ParseHeaders(const char* buffer, int len, int& httpStatus, int& contentLength, char* serverMd5, int serverMd5Size, int& patchedSize, char* nextFile, int nextFileSize);
    static bool ApplyPatch(const char* oldPath, const char* patchPath, const char* outPath, int newSize);
    static void Notify(ProgressCallback cb, int sleep, const char* fmt, ...);

    static bool s_ethernetConnected;
};

#endif // NETWORK_H
