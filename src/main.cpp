/*******************************************************************************
    Sega Dreamcast Project

    Project name : SDUpdcast
    Created on   : 2026-03-11
*******************************************************************************/

#include "core/App.h"

#include <arch/arch.h>

int main(int argc, char *argv[])
{
    App app;
    if (app.Init(argc, argv)) app.Run();

    app.Shutdown();
    arch_abort();

    return 0;
}
