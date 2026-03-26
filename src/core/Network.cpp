#include "Network.h"

#include "Logger.h"
#include "Utility.h"

#include <kos.h>
#include <kos/dbgio.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <ppp/ppp.h>

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" uint32 _fs_dclsocket_get_ip(void);

bool Network::s_ethernetConnected = false;

bool Network::Init()
{
    if (s_ethernetConnected)
        return true;

    Logger::LogInfo("Initializing network...");

    uint32 ip = 0;

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

    if (la_init() < 0 && bba_init() < 0)
    {
        Logger::LogInfo("No ethernet");
        return false;
    }

    if (net_init(ip) < 0)
    {
        Logger::LogError("net_init failed");
        return false;
    }

    if (dcload_type == DCLOAD_TYPE_IP)
    {
        if (!fs_dclsocket_init())
        {
            Logger::LogInfo("Re-enabling dcload console");

            fs_dclsocket_init_console();
            dbgio_dev_select("fs_dclsocket");
        }
    }

    if (net_default_dev)
    {
        Logger::LogInfo("IP: %d.%d.%d.%d",
            net_default_dev->ip_addr[0],
            net_default_dev->ip_addr[1],
            net_default_dev->ip_addr[2],
            net_default_dev->ip_addr[3]);
    }

    s_ethernetConnected = true;
    return true;
}

void Network::Shutdown()
{
    Logger::LogInfo("Shutting down network...");

    if (s_ethernetConnected)
    {
        if (dcload_type == DCLOAD_TYPE_IP)
        {
		    dbgio_dev_select("null"); //TODO find out why DCSWAT has DS
	    }

        net_shutdown();
    }
    else
    {
        ppp_shutdown();

		if (modem_is_connected() || modem_is_connecting())
        {
			modem_shutdown();
			net_shutdown();
		}
    }

    Logger::LogInfo("Network shutdown complete");
}

bool Network::Dial(ProgressCallback cb)
{
    if (!modem_init())
    {
        Logger::LogError("modem_init failed");
        return false;
    }

    ppp_init();
    Notify(cb, 0, "Dialing connection...");

    int conn_rate = 0;
    if (ppp_modem_init("1111111", 0, &conn_rate) != 0)
    {
        Logger::LogError("Couldn't dial a connection");
        return false;
    }

    Logger::LogInfo("Connected at: %dbps", conn_rate);

    Notify(cb, 0, "Establishing PPP link...");
    ppp_set_login("dream", "dreamcast");

    if (ppp_connect() != 0)
    {
        Logger::LogError("Couldn't establish PPP link");
        return false;
    }

    if (net_init(0) < 0)
    {
        Logger::LogError("net_init failed");
        return false;
    }

    if (net_default_dev)
    {
        Logger::LogInfo("IP: %d.%d.%d.%d",
            net_default_dev->ip_addr[0],
            net_default_dev->ip_addr[1],
            net_default_dev->ip_addr[2],
            net_default_dev->ip_addr[3]);
    }

    return true;
}

Network::DownloadResult Network::Download(const char* url, const char* destPath, ProgressCallback cb, const char* localMd5)
{
    Notify(cb, 1000, "Download:\n%s", destPath);

    // Validate host
    const char* hostStart = strstr(url, "http://");
    if (!hostStart) {
        Logger::LogError("Only http:// supported");
        return DownloadResult::Failure;
    }
    hostStart += 7;

    const char* pathStart = strchr(hostStart, '/');
    if (!pathStart) {
        Logger::LogError("Invalid URL (no path)");
        return DownloadResult::Failure;
    }

    char host[64]{};
    strncpy(host, hostStart, pathStart - hostStart);
    host[pathStart - hostStart] = '\0';
    const char* path = pathStart;

    Logger::LogInfo("Host: %s, Path: %s", host, path);

    // Resolve IP
    uint32_t ip = inet_addr(host);
    if (ip == INADDR_NONE) {
        Notify(cb, 1000, "Resolving hostname:\n%s", host);

        struct hostent* he = gethostbyname(host);
        if (!he) {
            Logger::LogError("DNS lookup failed");
            return DownloadResult::Failure;
        }

        ip = *(uint32_t*)he->h_addr;

        Logger::LogInfo(
            "Resolved IP: %lu.%lu.%lu.%lu",
            (ip >> 24) & 0xFF,
            (ip >> 16) & 0xFF,
            (ip >> 8) & 0xFF,
            ip & 0xFF);
    }

    // Set up socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        Logger::LogError("Socket creation failed");
        return DownloadResult::Failure;
    }

    int bufsize = s_ethernetConnected ? 65535 : 4096;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    addr.sin_addr.s_addr = ip;

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        Logger::LogError("Connect failed");
        close(sock);
        return DownloadResult::Failure;
    }

    Notify(cb, 1000, "Connected to server:\n%s", host);

    // Prepare path with optional ?md5=
    char fullPath[192];
    if (localMd5 && localMd5[0] != '\0') {
        snprintf(fullPath, sizeof(fullPath), "%s?md5=%s", path, localMd5);
    } else {
        strncpy(fullPath, path, sizeof(fullPath));
        fullPath[sizeof(fullPath)-1] = '\0';
    }

    // Send HTTP Get request
    char request[280];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "\r\n",
             fullPath, host);
    send(sock, request, strlen(request), 0);

    // Prepare temporary file
    Utility::MakeParentDir(destPath);
    char tmpPath[256];
    snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", destPath);

    // Open SD File
    file_t f = fs_open(tmpPath, O_WRONLY | O_CREAT | O_TRUNC);
    if (f < 0) {
        Logger::LogError("File open failed: %s", tmpPath);
        close(sock);
        return DownloadResult::Failure;
    }
    Logger::LogInfo("Writing to: %s", tmpPath);

    static const int RECV_BUFFER_SIZE = 32 * 1024;
    static const int SD_BUFFER_SIZE   = 128 * 1024;

    static char recvBuffer[RECV_BUFFER_SIZE] __attribute__((aligned(32)));
    static char sdBuffer[SD_BUFFER_SIZE] __attribute__((aligned(32)));

    int recvChunkSize = s_ethernetConnected ? RECV_BUFFER_SIZE : 4 * 1024;
    int sdFlushThreshold = s_ethernetConnected ? SD_BUFFER_SIZE : 32 * 1024;

    int sdBufUsed = 0;

    int len = 0;
    bool headerDone = false;
    int totalBytes = 0;
    uint64_t lastUpdate = timer_ms_gettime64();

    // Recv loop
    while ((len = recv(sock, recvBuffer, recvChunkSize, 0)) > 0) {
        int dataOffset = 0;

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

            if (sdBufUsed + dataLen > sdFlushThreshold) {
                int written = 0;
                while (written < sdBufUsed) {
                    int ret = fs_write(f, sdBuffer + written, sdBufUsed - written);
                    if (ret <= 0) {
                        Logger::LogError("Write failed");
                        return DownloadResult::Failure;
                    }
                    written += ret;
                }
                sdBufUsed = 0;
            }

            memcpy(sdBuffer + sdBufUsed, recvBuffer + dataOffset, dataLen);
            sdBufUsed += dataLen;
            totalBytes += dataLen;
        }

        thd_sleep(1);

        uint64_t now = timer_ms_gettime64();
        if (cb && now - lastUpdate > 100) {
            char msg[64];
            snprintf(msg, sizeof(msg), "Downloading...\n%d KB", totalBytes / 1024);
            cb(msg);
            lastUpdate = now;
        }
    }

    if (sdBufUsed > 0) {
        int written = 0;
        while (written < sdBufUsed) {
            int ret = fs_write(f, sdBuffer + written, sdBufUsed - written);
            if (ret <= 0) {
                Logger::LogError("Write failed");
                return DownloadResult::Failure;
            }
            written += ret;
        }
    }

    fs_close(f);
    close(sock);

    Notify(cb, 1000, "Download complete.\nCopying file...", totalBytes / 1024);

    if (!Utility::CopyFile(tmpPath, destPath, sdBuffer, SD_BUFFER_SIZE)) {
        Logger::LogError("Failed to copy tmp file to final path");
        return DownloadResult::Failure;
    }
    fs_unlink(tmpPath);

    return DownloadResult::Success;
}

void Network::Notify(ProgressCallback cb, int sleep, const char* fmt, ...)
{
    char uiBuf[512];
    char logBuf[512];

    va_list args;

    va_start(args, fmt);
    vsnprintf(uiBuf, sizeof(uiBuf), fmt, args);
    va_end(args);

    strncpy(logBuf, uiBuf, sizeof(logBuf));
    logBuf[sizeof(logBuf) - 1] = '\0';

    for (char* p = logBuf; *p; ++p) {
        if (*p == '\n') {
            *p = ' ';
        }
    }

    if (cb) cb(uiBuf);

    Logger::LogInfo("%s", logBuf);
    thd_sleep(sleep);
}
