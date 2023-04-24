# DeSmuME-wasm

WebAssembly port of the DeSmuME. 

Designed for iPhone/iPad, also workable on other devices with a modern browser.

https://ds.44670.org/

Please read this guide before creating a new issue. Thanks!

# About White Screen Issue on iOS

A small number of users have experienced a white screen issue after updating their iOS, due to a bug in the recent iOS update. Please follow the steps below to resolve this issue:

1. Turn off the Service Worker: Settings > Safari > Advanced > Experimental Features > Service Workers > OFF.
2. Access DS Player with an internet connection.

To restore offline access:

1. Backup saved data on the old DS Player icon.
2. Add a new icon and restore the Service Worker.

Note: Old and new icons have independent data. Do not delete the old icon to prevent data loss. 


# Frequently Asked Questions


**Q: Can I use it offline?**

- A: Yes, as long as you have added it to the Home Screen.

**Q: There is no sound.**

- A: Your device's "Silent Switch" should be "Off." If the sound is still not working, please try to restart the app/add another icon on Home Screen/reboot the device.

**Q: The performance is too slow/laggy.**

- A: Please ensure iOS's system-wide "Low Power Mode" is disabled. The battery icon is yellow if it is enabled.

**Q: The Select/Start buttons are overlapped by the Home Bar.**

- A: That's okay. You can touch them as usual. If the buttons are not working, it means the game does not use the Select/Start.  

**Q: How do I save my progress?**

- A: Just save in the game and wait a few seconds. An auto-saving banner will appear, and your saved data will automatically be stored in the web app's local storage. On iOS, you must add the site to Home Screen first, and **your data will be deleted** when the Home Screen icon is removed.

**Q: Do I have to backup/restore the savegame in the menu?**

- A: You **don't need to backup/restore** the savegame manually since auto-saving is present, just save in the game and wait a few seconds. To prevent data loss caused by accidental deletion of the Home Screen icon or damaged device, you may connect to dropbox to enable automatic cloud backup.

**Q: After importing the savegame, it takes me back to the main menu.**

- A: It is expected behavior, the save data was imported, and you can continue playing by loading the game file again.

**Q: It can't load any files.**

- A: Only iOS >= 14.7 is supported. Please update your OS first.

**Q: How to blow on the microphone?**

- A: Pressing the 'R' button will emulate a blow on the microphone. It *may* be helpful in playing some games.


# Performance

Most 2D games could be run at 60fps on A14-based devices. 

However, the performance of 3D games varies for each game. An A15-based device could run most 3D games at nearly full speed.

By default, the 30FPS limit mode is enabled. It is strongly recommended to enable this mode on A14-based devices (or earlier) to protect battery life and keep the temperature comfortable for playing. 

On A15-based devices, this mode could be disabled if you want a smoother experience.

# Control

Gamepads are supported if your OS supports it. Please note that iOS does not support most kinds of gamepads. DualShock 4 is an officially supported one.

You may also want to control the game with a keyboard:

| Key | Gamepad |
| :--: | :--: |
| z | A |
| x | B |
| a | Y |
| s | X |
| q | L |
| w | R |
| enter | Start |
| shift | Select |
| escape | Menu |
