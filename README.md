# kb_light
Simple Windows service that maintains keyboard backlight across reboots and temporarily shuts it off when a full screen application is in use. Works on ThinkPad computers.

From an elevated command window, register using:

```
sc create "kb_light Service" binPath=C:\...\kb_light.exe
```

To save keyboard backlight state when the computer turns off, add a log off script that calls "C:\...\kb_light.exe 1".

To restore ketboard backlight state when the computer turns on, add a log on script that calls "C:\...\kb_light.exe".
