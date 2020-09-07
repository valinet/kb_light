# ThinkPad Keyboard Backlight Manager
Simple Windows service that maintains keyboard backlight across reboots and temporarily shuts it off when a full screen application is in use. Works on ThinkPad computers.

Download binaries from [Releases](https://github.com/valinet/kb_light/releases). There are 2 versions to choose from:
* kb_light_ibmpmdrv - this version uses the Lenovo Power Management Driver that can be installed from https://support.lenovo.com/br/en/downloads/DS539633
* kb_light_winring0 - this version uses the WinRing0 driver; this driver is bundled with the application and is loaed on demand, when the application is running

To install, from an elevated command window, run:

```
sc create kb_light binPath= "C:\...\kb_light.exe" DisplayName= "ThinkPad Keyboard Backlight Management Service"  start= auto
sc description kb_light "kb_light service (https://github.com/valinet/kb_light)"
```

The service is set to start automatically at boot. To run immediatly, type:

```
sc start kb_light
```

To save keyboard backlight state when the computer turns off, add a log off script that calls "C:\...\kb_light.exe x".

To restore keyboard backlight state when the computer turns on, add a log on script that calls "C:\...\kb_light.exe".

The application supports the following command line:

* ``C:\...\kb_light.exe 0`` - disable keyboard backlight and exit
* ``C:\...\kb_light.exe 1`` - set keyboard backlight to dim level and exit
* ``C:\...\kb_light.exe 2`` - set keyboard backlight to bright level and exit
* ``C:\...\kb_light.exe ?`` - returns the current keyboard backlight level as exit code