I'm not big on documentation!

SDUpdcast is a binary / drop in C header for your Dreamcast game / application.
It provides a way for your app to check online and download an update (even as a bsdiff / bspatch if you like to keep it fast on dialup).
It's been built and tested using DreamSDK by SiZiOUS, but should be easy to add a makefile if that's more your thing.
I have used FatFS by DC-SWAT to mount my SD, it might work with other options but untested.
It will only run if there is an SD mounted.
It has some weird interactions with dcload-ip (at least on my setup) but seems fine in real usage.

I am sharing now because I want to get back to working on my game, there may yet be problems to iron out, and your mileage may vary.

Check in the misc folder for

  - an updated net\_tcp.c with some backported patches by pcercuei and Bruceleeto, this is a replacement for the offline version that came with DreamSDK r4 (\~August 2025)
  - a Program.cs example of how I have my server set up to send the latest version, and possibly a patch

Big thanks to everyone who works on KallistiOS and on the Simulant discord.
I hope I've given all the appropriate credits, I would love to see this used in Dreamcast homebrew (and I don't mind if you change it).
Thanks petervas for bsdifflib, see the LICENSE test in the bspatch folder.
This project also needs zlib, libpng and libbz2 to build, you can get them as KOS Ports, but their own licenses may apply.

Please let me know of any issues, I have probably rushed it out a bit, but I'm keen to get back on with my game!!

Usage: 

```
#include "SDUpdcast.h"

static void ShutdownAndExit()
{
    fs_fat_unmount_sd();
    sd_shutdown();
    arch_abort();
}

int main(int argc, char *argv[])
{
    fs_fat_mount_sd();

    if (dcload_type != DCLOAD_TYPE_IP) // Can be flaky if you're running with DCLOAD, I would suggest Dreamshell as a good way to test your integration
    {
        SDUpdcast_RunUpdater(
            "/cd/SDUpdcast.bin",
            "/cd/your_app.bin",
            "/sd/your_app/your_app.bin",
            nullptr, // Don't look online for updates, just look for the update we already have
            fs_fat_unmount_sd // pass any last minute cleanup in here
        );
    }

    App* app = new App();

    if (!app->Init())
    {
        delete app;
        ShutdownAndExit();
        return -1;
    }

    bool runUpdater = false;

    app->Run(runUpdater); // out param
    app->Shutdown();

    delete app;

    if (dcload_type != DCLOAD_TYPE_IP && runUpdater)
    {
        SDUpdcast_RunUpdater(
            "/cd/SDUpdcast.bin",
            "/cd/your_app.bin",
            "/sd/your_app/your_app.bin",
            "http://yourwebsite.com/dc/update.bin",
            fs_fat_unmount_sd
        );
    }

    ShutdownAndExit();

    return 0;
}
```

