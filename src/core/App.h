#ifndef APP_H
#define APP_H

#include "UpdateArgs.h"

class App
{
public:
    App() { s_instance = this; }

    bool Init(int argc, char *argv[]);
    void Run();
    void Shutdown();

private:
    UpdateArgs m_args{};
    int m_argc{};
    char **m_argv{};
    bool m_ownsArgv{};

    static App *s_instance;

    static void Cleanup();
};

#endif // APP_H


