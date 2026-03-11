#ifndef APP_H
#define APP_H

#include "UpdateArgs.h"

class App
{
public:
    bool Init(int argc, char *argv[]);
    void Run();
    void Shutdown();

private:
    UpdateArgs m_args;
};

#endif // APP_H


