#include "App.h"

#include "../config.h"
#include "../SDUpdcast.h"
#include "Logger.h"
#include "Network.h"
#include "Utility.h"

extern "C" {
#include <fatfs.h>
}
#include <png/png.h>
#include <dc/sd.h>
#include <kos/fs.h>
#include <kos/thread.h>

#include <cstdint>
#include <cstdlib>

extern "C" char wfont[];

App* App::s_instance = nullptr;

bool App::Init()
{
    fs_fat_mount_sd();

#ifdef DEBUG_SD_LOG
    Logger::Init(LOG_FQFN);
#endif

    Network::Init();

    strncpy(m_returnBin, "/sd/demotek.bin", sizeof(m_returnBin));
    m_returnBin[sizeof(m_returnBin) - 1] = '\0';

    strncpy(m_overrideBin, "/sd/demotek.bin", sizeof(m_overrideBin));
    m_overrideBin[sizeof(m_overrideBin) - 1] = '\0';

    strncpy(m_updateUrl, "http://ac3t1ne.co.uk/dc/demotek.bin", sizeof(m_updateUrl));
    m_updateUrl[sizeof(m_updateUrl) - 1] = '\0';

    /*if (!SDUpdcast_GetParams(m_returnBin, m_overrideBin, m_updateUrl))
    {
        Logger::LogError("Couldn't retrieve parameters. Exiting.");
        return false;
    }*/

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
    pvr_init_defaults();

    InitFont();
    InitBackground();

    DrawFrame();

    if (!Network::EthernetConnected())
    {
        Network::Dial(
        [](const char* msg) {
             App::s_instance->SetMessage(msg);
             App::s_instance->DrawFrame();
        });
    }

    char localMd5[33];
    bool hasLocalMd5 = Utility::Md5File(m_overrideBin, &localMd5);
    bool updateSuccess = Network::Download(
        m_updateUrl,
        m_overrideBin,
        [](const char* msg) {
            App::s_instance->SetMessage(msg);
            App::s_instance->DrawFrame();
        }
    );

    if (updateSuccess) SetMessage("Update successful\nlaunching...");
    else SetMessage("Something went wrong\naborting...");

    DrawFrame();
    Network::Shutdown();
    thd_sleep(500);

    char* destinationBin = updateSuccess ? m_overrideBin : m_returnBin;

    Logger::LogInfo("Launching %s", destinationBin);

    SDUpdcast_Exec(destinationBin, Cleanup);
}

void App::Shutdown()
{
    Cleanup();
    sd_shutdown();
}

void App::Cleanup()
{
    pvr_wait_ready();

    if (s_instance)
    {
       if (s_instance->m_fontTex)
          pvr_mem_free(s_instance->m_fontTex);

       if (s_instance->m_backTex)
          pvr_mem_free(s_instance->m_backTex);
    }

    Logger::Shutdown();

    SDUpdcast_SetSkip();

    fs_fat_unmount_sd();
}

void App::InitBackground()
{
    m_backTex = pvr_mem_malloc(512 * 512 * 2);
    png_to_texture("/rd/background.png", m_backTex, PNG_NO_ALPHA);
}

void App::InitFont()
{
    int i, x, y, c;
    static unsigned short temp_tex[256 * 256];

    m_fontTex = pvr_mem_malloc(256 * 256 * 2);

    c = 0;

    for (y = 0; y < 128; y += 16)
        for (x = 0; x < 256; x += 8)
        {
            for (i = 0; i < 16; i++)
            {
                temp_tex[x + (y + i) * 256 + 0] = 0xffff * ((wfont[c + i] & 0x80) >> 7);
                temp_tex[x + (y + i) * 256 + 1] = 0xffff * ((wfont[c + i] & 0x40) >> 6);
                temp_tex[x + (y + i) * 256 + 2] = 0xffff * ((wfont[c + i] & 0x20) >> 5);
                temp_tex[x + (y + i) * 256 + 3] = 0xffff * ((wfont[c + i] & 0x10) >> 4);
                temp_tex[x + (y + i) * 256 + 4] = 0xffff * ((wfont[c + i] & 0x08) >> 3);
                temp_tex[x + (y + i) * 256 + 5] = 0xffff * ((wfont[c + i] & 0x04) >> 2);
                temp_tex[x + (y + i) * 256 + 6] = 0xffff * ((wfont[c + i] & 0x02) >> 1);
                temp_tex[x + (y + i) * 256 + 7] = 0xffff * (wfont[c + i] & 0x01);
            }

            c += 16;
        }

    pvr_txr_load_ex(temp_tex, m_fontTex, 256, 256, PVR_TXRLOAD_16BPP);
}

void App::SetMessage(const char* msg)
{
    if (!msg)
    {
        m_currentMessage[0] = '\0';
        return;
    }

    strncpy(m_currentMessage, msg, MAX_MESSAGE_LEN - 1);
    m_currentMessage[MAX_MESSAGE_LEN - 1] = '\0';
}

void App::DrawBackground()
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_vertex_t vert;

    pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY,
                     PVR_TXRFMT_RGB565,
                     512, 512,
                     m_backTex,
                     PVR_FILTER_BILINEAR);

    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));

    vert.argb = PVR_PACK_COLOR(1,1,1,1);
    vert.oargb = 0;
    vert.flags = PVR_CMD_VERTEX;

    vert.x = 0;   vert.y = 0;   vert.z = 1; vert.u = 0; vert.v = 0;
    pvr_prim(&vert, sizeof(vert));

    vert.x = 640; vert.y = 0;   vert.u = 1; vert.v = 0;
    pvr_prim(&vert, sizeof(vert));

    vert.x = 0;   vert.y = 480; vert.u = 0; vert.v = 1;
    pvr_prim(&vert, sizeof(vert));

    vert.x = 640; vert.y = 480; vert.u = 1; vert.v = 1;
    vert.flags = PVR_CMD_VERTEX_EOL;
    pvr_prim(&vert, sizeof(vert));
}

void App::DrawChar(float x1, float y1, float z1,
                   float a, float r, float g, float b,
                   int c, float xs, float ys)
{
    pvr_vertex_t vert;

    int ix = (c % 32) * 8;
    int iy = (c / 32) * 16;

    float u1 = (ix + 0.5f) / 256.0f;
    float v1 = (iy + 0.5f) / 256.0f;
    float u2 = (ix + 7.5f) / 256.0f;
    float v2 = (iy + 15.5f) / 256.0f;

    vert.flags = PVR_CMD_VERTEX;
    vert.argb = PVR_PACK_COLOR(a, r, g, b);
    vert.oargb = 0;
    vert.z = z1;

    vert.x = x1; vert.y = y1 + 16 * ys; vert.u = u1; vert.v = v2;
    pvr_prim(&vert, sizeof(vert));

    vert.x = x1; vert.y = y1; vert.u = u1; vert.v = v1;
    pvr_prim(&vert, sizeof(vert));

    vert.x = x1 + 8 * xs; vert.y = y1 + 16 * ys; vert.u = u2; vert.v = v2;
    pvr_prim(&vert, sizeof(vert));

    vert.flags = PVR_CMD_VERTEX_EOL;
    vert.x = x1 + 8 * xs; vert.y = y1; vert.u = u2; vert.v = v1;
    pvr_prim(&vert, sizeof(vert));
}

void App::DrawString(float x, float y, float z,
                     float a, float r, float g, float b,
                     char* str, float xs, float ys)
{
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;

    float orig_x = x;

    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY,
                     PVR_TXRFMT_ARGB4444,
                     256, 256,
                     m_fontTex,
                     PVR_FILTER_BILINEAR);

    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));

    float lineHeight = 16 * ys + 4; // 4px padding

    while (*str)
    {
        if (*str == '\n')
        {
            x = orig_x;
            y += lineHeight;
            str++;
            continue;
        }

        DrawChar(x, y, z, a, r, g, b, *str++, xs, ys);
        x += (8 + 1) * xs; // 1px character spacing
    }
}

void App::DrawFrame()
{
    Logger::KeepStdioAlive();

    pvr_wait_ready();
    pvr_scene_begin();

    pvr_list_begin(PVR_LIST_OP_POLY);
    DrawBackground();
    pvr_list_finish();

    pvr_list_begin(PVR_LIST_TR_POLY);

    DrawString(40,  // <-- top-left X
               160, // <-- top-left Y
               3,
               1,1,1,1,
               m_currentMessage,
               2,2);

    pvr_list_finish();
    pvr_scene_finish();
}
