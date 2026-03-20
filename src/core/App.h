#ifndef APP_H
#define APP_H

class App
{
public:
    bool Init();
    void Run();
    void Shutdown();

private:
    char m_returnBin[256]{};
    char m_overrideBin[256]{};
    char m_updateUrl[256]{};

    static void Cleanup();
};

#endif // APP_H


