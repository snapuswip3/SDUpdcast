I'm not big on documentation!



SDUpdcast is a binary / drop in C header for your Dreamcast game / application.

It provides a way for your app to check online and download an update (even as a bsdiff / bspatch if you like to keep it fast on dialup).

It's been built and tested using DreamSDK by SiZiOUS, but should be easy to add a makefile if that's more your thing.

I have used FatFS by DC-SWAT to mount my SD, it might work with other options but untested.

It will only run if there is an SD mounted.

It has some weird interactions with dcload-ip (at least on my setup) but seems fine in real usage.

I am sharing now because I want to get back to working on my game, 



Check in the misc folder for

&#x20;- an updated net\_tcp.c with some backported patches by pcercuei and Bruceleeto, this is a replacement for the offline version that came with DreamSDK r4 (\~August 2025)

&#x20;- a Program.cs example of how I have my server set up to send the latest version, and possibly a patch



Big thanks to everyone who works on KallistiOS and on the Simulant discord.

I hope I've given all the appropriate credits, I would love to see this used in Dreamcast homebrew!

Please let me know of any issues, I have probably rushed it out a bit, but I'm keen to get back on with my game.



Usage: 



\#include "SDUpdcast.h"



static void ShutdownAndExit()

{

&#x20;   fs\_fat\_unmount\_sd();

&#x20;   sd\_shutdown();

&#x20;   arch\_abort();

}



int main(int argc, char \*argv\[])

{

&#x20;   fs\_fat\_mount\_sd();



&#x20;   if (dcload\_type != DCLOAD\_TYPE\_IP) // Can be flaky if you're running with DCLOAD, I would suggest Dreamshell as a good way to test your integration

&#x20;   {

&#x20;       SDUpdcast\_RunUpdater(

&#x20;           "/cd/SDUpdcast.bin",

&#x20;           "/cd/your\_app.bin",

&#x20;           "/sd/your\_app/your\_app.bin",

&#x20;           nullptr, // Don't look online for updates, just look for the update we already have

&#x20;           fs\_fat\_unmount\_sd // pass any last minute cleanup in here

&#x20;       );

&#x20;   }



&#x20;   App\* app = new App();

&#x20;   if (!app->Init())

&#x20;   {

&#x20;       delete app;

&#x20;       ShutdownAndExit();



&#x20;       return -1;

&#x20;   }



&#x20;   bool runUpdater = false;

&#x20;   app->Run(runUpdater); // out param

&#x20;   app->Shutdown();

&#x20;   delete app;



&#x20;   if (dcload\_type != DCLOAD\_TYPE\_IP \&\& runUpdater)

&#x20;   {

&#x20;       SDUpdcast\_RunUpdater(

&#x20;           "/cd/SDUpdcast.bin",

&#x20;           "/cd/your\_app.bin",

&#x20;           "/sd/your\_app/your\_app.bin",

&#x20;           "http://yourwebsite.com/dc/update.bin",

&#x20;           fs\_fat\_unmount\_sd

&#x20;       );

&#x20;   }



&#x20;   ShutdownAndExit();



&#x20;   return 0;

}

