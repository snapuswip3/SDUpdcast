#include "Network.h"
#include "Logger.h"

#include <kos.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

extern "C" uint32 _fs_dclsocket_get_ip(void);

// ----------------------------------------
// Static member definition
// ----------------------------------------
bool Network::s_initialized = false;

// ----------------------------------------
// Init
// ----------------------------------------
bool Network::Init()
{
    if (s_initialized)
    {
        Logger::LogInfo("Network already initialized");
        return true;
    }

    Logger::LogInfo("Initializing network...");

    uint32 ip = 0;

    // -----------------------------
    // Handle dcload-ip
    // -----------------------------
    if (dcload_type == DCLOAD_TYPE_IP)
    {
        Logger::LogInfo("Detected dcload-ip");

        ip = _fs_dclsocket_get_ip();

        Logger::LogInfo("dcload IP: %d.%d.%d.%d",
            (ip >> 24) & 0xFF,
            (ip >> 16) & 0xFF,
            (ip >> 8) & 0xFF,
            ip & 0xFF);

        dbgio_dev_select("scif");
    }

    // -----------------------------
    // Init adapters
    // -----------------------------
    la_init();
    bba_init();
    //w5500_adapter_init(NULL, true);

    // -----------------------------
    // Init network stack
    // -----------------------------
    if (net_init(ip) < 0)
    {
        Logger::LogError("net_init failed");
        return false;
    }

    // -----------------------------
    // Restore dcload console
    // -----------------------------
    if (dcload_type == DCLOAD_TYPE_IP)
    {
        if (!fs_dclsocket_init())
        {
            Logger::LogInfo("Re-enabling dcload console");

            fs_dclsocket_init_console();
            dbgio_dev_select("fs_dclsocket");
        }
    }

    // -----------------------------
    // Log IP
    // -----------------------------
    if (net_default_dev)
    {
        Logger::LogInfo("IP: %d.%d.%d.%d",
            net_default_dev->ip_addr[0],
            net_default_dev->ip_addr[1],
            net_default_dev->ip_addr[2],
            net_default_dev->ip_addr[3]);
    }

    s_initialized = true;
    return true;
}

// ----------------------------------------
// Shutdown
// ----------------------------------------
void Network::Shutdown()
{
    if (!s_initialized)
        return;

    Logger::LogInfo("Shutting down network...");

    /*if (dcload_type == DCLOAD_TYPE_IP)
    {
        dbgio_dev_select("fs_dclsocket");
    }*/

    net_shutdown();

    // small safety delay (helps stability before arch_exec)
    thd_sleep(50);

    s_initialized = false;

    Logger::LogInfo("Network shutdown complete");
}

// ----------------------------------------
// State
// ----------------------------------------
bool Network::IsInitialized()
{
    return s_initialized;
}

bool Network::Download(const char* url, const char* destPath, ProgressCallback cb)
{
    if (!s_initialized) {
        if (cb) cb("Network not initialized");
        Logger::LogError("Network not initialized");
        return false;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "Download: %s -> %s", url, destPath);
    if(cb) cb(msg);
    Logger::LogInfo("%s", msg);

    // --- Parse URL ---
    const char* hostStart = strstr(url, "http://");
    if (!hostStart) {
        if(cb) cb("Only http:// supported");
        Logger::LogError("Only http:// supported");
        return false;
    }
    hostStart += 7;

    const char* pathStart = strchr(hostStart, '/');
    if (!pathStart) {
        if(cb) cb("Invalid URL (no path)");
        Logger::LogError("Invalid URL (no path)");
        return false;
    }

    char host[64]{};
    strncpy(host, hostStart, pathStart - hostStart);
    host[pathStart - hostStart] = '\0';
    const char* path = pathStart;

    if(cb) cb(host);
    if(cb) cb(path);
    Logger::LogInfo("Host: %s, Path: %s", host, path);

    // --- Resolve host ---
    uint32_t ip = inet_addr(host);
    if (ip == INADDR_NONE) {
        snprintf(msg, sizeof(msg), "Resolving hostname: %s", host);
        if(cb) cb(msg);

        struct hostent* he = gethostbyname(host);
        if (!he) {
            if(cb) cb("DNS lookup failed");
            Logger::LogError("DNS lookup failed");
            return false;
        }

        ip = *(uint32_t*)he->h_addr;
        snprintf(msg, sizeof(msg), "Resolved IP: %lu.%lu.%lu.%lu",
                 (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                 (ip >> 8) & 0xFF, ip & 0xFF);
        if(cb) cb(msg);
    }

    // --- Open socket ---
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        if(cb) cb("Socket creation failed");
        Logger::LogError("socket failed");
        return false;
    }

    // --- KOS tuning: bigger recv buffer & TCP_NODELAY ---
    int bufsize = 128 * 1024; // 64 KB
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    addr.sin_addr.s_addr = ip;

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        if(cb) cb("Connect failed");
        Logger::LogError("connect failed");
        close(sock);
        return false;
    }

    if(cb) cb("Connected to server");

    // --- Send HTTP GET request ---
    char request[256];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "\r\n",
             path, host);
    send(sock, request, strlen(request), 0);

    // --- Open SD file ---
    file_t f = fs_open(destPath, O_WRONLY | O_CREAT | O_TRUNC);
    if (f < 0) {
        snprintf(msg, sizeof(msg), "File open failed: %s", destPath);
        if(cb) cb(msg);
        Logger::LogError("%s", msg);
        close(sock);
        return false;
    }

    if(cb) {
        snprintf(msg, sizeof(msg), "Writing to: %s", destPath);
        cb(msg);
    }

    // --- Receive + buffered SD writes ---
    const int recvBufferSize = 32 * 1024;  // 16 KB network buffer
    const int sdBufferSize   = 128 * 1024;  // 128 KB SD write buffer
    static char recvBuffer[recvBufferSize];
    static char sdBuffer[sdBufferSize];
    int sdBufUsed = 0;

    int len;
    bool headerDone = false;
    int totalBytes = 0;
    uint64_t lastUpdate = timer_ms_gettime64();

    while ((len = recv(sock, recvBuffer, sizeof(recvBuffer), 0)) > 0) {
        int dataOffset = 0;

        // --- Skip HTTP header ---
        if (!headerDone) {
            for (int i = 0; i < len - 3; i++) {
                if (recvBuffer[i] == '\r' && recvBuffer[i+1] == '\n' &&
                    recvBuffer[i+2] == '\r' && recvBuffer[i+3] == '\n') {
                    headerDone = true;
                    dataOffset = i + 4;
                    break;
                }
            }
        }

        if (headerDone) {
            int dataLen = len - dataOffset;
            if (sdBufUsed + dataLen > sdBufferSize) {
                // flush full buffer
                fs_write(f, sdBuffer, sdBufUsed);
                sdBufUsed = 0;
            }

            memcpy(sdBuffer + sdBufUsed, recvBuffer + dataOffset, dataLen);
            sdBufUsed += dataLen;
            totalBytes += dataLen;
        }

        // --- Progress callback every 100ms ---
        uint64_t now = timer_ms_gettime64();
        if (cb && now - lastUpdate > 100) {
            snprintf(msg, sizeof(msg), "Downloading... %d KB", totalBytes / 1024);
            cb(msg);
            lastUpdate = now;

            thd_sleep(5); // keep UI responsive
        }
    }

    // --- Flush any remaining SD buffer ---
    if (sdBufUsed > 0) {
        fs_write(f, sdBuffer, sdBufUsed);
    }

    fs_close(f);
    close(sock);

    snprintf(msg, sizeof(msg), "Download complete (%d KB)", totalBytes / 1024);
    if(cb) cb(msg);
    Logger::LogInfo("%s", msg);

    return true;
}
