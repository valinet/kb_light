# kb_light
Simple Windows service that maintains keyboard backlight across reboots and temporarily shuts it off when a full screen application is in use. Works on ThinkPad computers.

Download binaries from [Releases](https://github.com/valinet/kb_light/releases). There are 2 versions to choose from:
* kb_light_ibmpmdrv - this version uses the Lenovo Power Management Driver that can be installed from https://support.lenovo.com/br/en/downloads/DS539633
* kb_light_winring0 - this version uses the WinRing0 driver; this driver is bundled with the application and is loaed on demand, when the application is running

To install, from an elevated command window, run:

```
sc create "kb_light Service" binPath=C:\...\kb_light.exe
```

To save keyboard backlight state when the computer turns off, add a log off script that calls "C:\...\kb_light.exe 1".

To restore ketboard backlight state when the computer turns on, add a log on script that calls "C:\...\kb_light.exe".
