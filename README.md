# CenterWindows
A program that re-positions windows at the center of the desktop.

## Download
Grab the exe for your machine from the [latest release](https://github.com/ryancole/CenterWindows/releases/latest):

- `CenterWindows-x64.exe` for most PCs
- `CenterWindows-arm64.exe` for Windows on ARM (e.g. Snapdragon laptops)

It's a single standalone exe, no installer or runtime needed.

## Usage
I typically assign a Windows hotkey to this application. I use it to center application windows on my desktop.

To set up a hotkey:

1. Put the exe somewhere permanent and create a shortcut to it in your Start menu folder (`%APPDATA%\Microsoft\Windows\Start Menu\Programs`)
2. Right click the shortcut, choose Properties, and set a **Shortcut key** (e.g. `Ctrl + Alt + C`)

Each window is centered within the work area of the monitor it's on (the area not covered by the taskbar). Windows that are left alone:

- minimized, maximized, snapped or fullscreen windows
- windows on other virtual desktops
- dialogs, popups, tool windows and windows without a title
- windows of apps running as administrator, unless CenterWindows is also run as administrator

It runs silently, but if you run it from a terminal it prints what it centered and skipped.

## Releasing
Push a version tag and the [release workflow](.github/workflows/release.yml) builds the exes, stamps the version into them, and publishes them as a GitHub release. Tags must look like `v1.2.3`.

```
git tag v1.0.0
git push origin v1.0.0
```
