#ifndef APP_H
#define APP_H

#include <kos.h>

class App
{
public:
    App() { s_instance = this; }

    bool Init();
    void Run();
    void Shutdown();

private:
    // --- Existing updater params ---
    char m_returnBin[256]{};
    char m_overrideBin[256]{};
    char m_updateUrl[256]{};

    // --- Rendering state ---
    pvr_ptr_t m_fontTex{};
    pvr_ptr_t m_backTex{};
    char* m_textData{};
    int m_scrollY{0};
    bool m_done{false};

    // --- Init helpers ---
    void InitFont();
    void InitBackground();
    void InitText();

    // --- Rendering ---
    void DrawFrame();
    void DrawBackground();
    void DrawChar(float x1, float y1, float z1,
                  float a, float r, float g, float b,
                  int c, float xs, float ys);
    void DrawString(float x, float y, float z,
                    float a, float r, float g, float b,
                    char* str, float xs, float ys);

    static App *s_instance;

    static void Cleanup();
};

#endif // APP_H
