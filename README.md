<div align="center">

<img src="docs/logo-256.png" alt="WacOS logo" width="128" height="128">

# WacOS

**macOS on top of Windows 11: menu bar, Dock, Finder, Spotlight, Control Center and Liquid Glass, in native C++.**

[![Stars](https://img.shields.io/github/stars/renzyxx/WacOS?style=flat&logo=github&label=Stars)](https://github.com/renzyxx/WacOS/stargazers)
[![Forks](https://img.shields.io/github/forks/renzyxx/WacOS?style=flat&logo=github&label=Forks)](https://github.com/renzyxx/WacOS/network/members)
[![Downloads](https://img.shields.io/github/downloads/renzyxx/WacOS/total?style=flat&label=Downloads&color=brightgreen)](https://github.com/renzyxx/WacOS/releases)
[![Latest release](https://img.shields.io/github/v/release/renzyxx/WacOS?style=flat&label=Release)](https://github.com/renzyxx/WacOS/releases/latest)
[![Build](https://img.shields.io/github/actions/workflow/status/renzyxx/WacOS/build.yml?branch=main&style=flat&label=Build)](https://github.com/renzyxx/WacOS/actions/workflows/build.yml)
![Windows 11](https://img.shields.io/badge/Windows-11-0078D4?style=flat&logo=windows11&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-native-00599C?style=flat&logo=cplusplus&logoColor=white)

[![Download WacOS](https://img.shields.io/badge/DOWNLOAD-WacOS_for_Windows_11-0A84FF?style=for-the-badge&logo=windows11&logoColor=white)](https://github.com/renzyxx/WacOS/releases/latest)
[![Join the WacOS Discord](https://img.shields.io/badge/DISCORD-Join_us-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/bDVydgSKPv)
[![Buy me a coffee](https://img.shields.io/badge/BUY_ME_A_COFFEE-renzyx-FFDD00?style=for-the-badge&logo=buymeacoffee&logoColor=black)](https://buymeacoffee.com/renzyx)

</div>

WacOS runs on top of Windows (same kernel, drivers and apps) and replaces what you see and touch: the menu bar,
the Dock, Finder, Spotlight, Control Center, Notification Center, Mission Control, Launchpad, window title bars and a
set of built-in apps. Pick the new macOS 27 **Golden Gate** look, the macOS Tahoe look with Liquid Glass, the
macOS Sequoia look, or the classic **Mac OS X Leopard** (2007) Aqua look.

Everything is native C++ (Win32, Direct2D, DirectComposition). No Electron, no web views. It uses about 750 MB of RAM
and under 1% CPU on a 3440×1440 desktop.

![WacOS desktop in the macOS 27 Golden Gate theme](docs/screenshots/golden-gate-desktop.png)

**Contents:** [Screenshots](#screenshots) · [Features](#features) · [Install](#install) · [Is it safe?](#is-it-safe) ·
[Roadmap](#roadmap) · [Support the project](#support-the-project) · [Build](#build) · [License](#license)

## Screenshots

Four themes, in System Settings > Appearance > Theme. Switching themes never changes your own wallpaper.

### 1. macOS 27 Golden Gate (new)

The newest look, matched to Apple's macOS 27 screenshots: frosted Liquid Glass on windows, menus and the Dock,
unified toolbars, the macOS 27 icons and the Golden Gate wallpaper.


| | |
|---|---|
| ![Quick Look, Golden Gate](docs/screenshots/golden-gate-quick-look.png) **Quick Look** plays video and music, with a frosted header and an animated open/close | ![Photos, Golden Gate](docs/screenshots/golden-gate-photos.png) **Photos** with search |
| ![Finder, Golden Gate](docs/screenshots/golden-gate-finder.png) **Finder** | ![System Settings, Golden Gate](docs/screenshots/golden-gate-system-settings.png) **System Settings** > Appearance, for every theme |
| ![Notes, Golden Gate](docs/screenshots/golden-gate-notes.png) **Notes** | ![Calendar, Golden Gate](docs/screenshots/golden-gate-calendar.png) **Calendar** |
| ![Reminders, Golden Gate](docs/screenshots/golden-gate-reminders.png) **Reminders** | ![Music, Golden Gate](docs/screenshots/golden-gate-music.png) **Music** with search |
| ![Clock, Golden Gate](docs/screenshots/golden-gate-clock.png) **Clock** with sunrise and sunset times | ![Weather, Golden Gate](docs/screenshots/golden-gate-weather.png) **Weather** |
| ![Activity Monitor, Golden Gate](docs/screenshots/golden-gate-activity-monitor.png) **Activity Monitor** | ![Preview, Golden Gate](docs/screenshots/golden-gate-preview.png) **Preview** |
| ![Calculator, Golden Gate](docs/screenshots/golden-gate-calculator.png) **Calculator** | ![Terminal, Golden Gate](docs/screenshots/golden-gate-terminal.png) **Terminal** |
| ![TextEdit, Golden Gate](docs/screenshots/golden-gate-textedit.png) **TextEdit** | |

Also new with this theme: QuickTime Player with a frosted title bar over the video, the option to hide desktop icons,
**Edit Widgets…** on the desktop menu, clicking the empty strip beside the Dock to show the desktop, Win+D handled by
the shell, grouped notifications that open on click, and smoother glass over animated wallpapers.

### 2. macOS Tahoe (Liquid Glass)

The macOS 26 look: clear Liquid Glass with real refraction and chromatic aberration.

![WacOS desktop in the Tahoe theme](docs/screenshots/desktop.png)

| | |
|---|---|
| ![Control Center in Liquid Glass](docs/screenshots/control-center.png) **Control Center** in Liquid Glass, with real refraction and chromatic aberration | ![Notification Center](docs/screenshots/notification-center.png) **Notification Center** with widgets |
| ![Spotlight](docs/screenshots/spotlight.png) **Spotlight** | ![Spotlight Applications mode](docs/screenshots/spotlight-apps.png) **Applications** (Spotlight's Apps mode, with categories) |
| ![Finder](docs/screenshots/finder.png) **Finder** | ![System Settings](docs/screenshots/system-settings.png) **System Settings** |
| ![Activity Monitor](docs/screenshots/activity-monitor.png) **Activity Monitor** | ![Force Quit Applications](docs/screenshots/force-quit.png) **Force Quit Applications** (Ctrl+Alt+Esc) |
| ![macOS alert](docs/screenshots/alert.png) Windows message boxes become **macOS alerts** | ![Dock icon styles](docs/screenshots/dock-icon-styles.png) **Dock icon styles**: Default, Dark, Clear and Tinted |

### 3. macOS Sequoia

The same shell with the macOS 15 look (System Settings > Appearance > Theme).

![WacOS desktop in the Sequoia theme](docs/screenshots/sequoia-desktop.png)

| | |
|---|---|
| ![Control Center, Sequoia](docs/screenshots/sequoia-control-center.png) **Control Center** | ![Notification Center, Sequoia](docs/screenshots/sequoia-notification-center.png) **Notification Center** |
| ![Launchpad](docs/screenshots/sequoia-launchpad.png) **Launchpad** | ![Spotlight, Sequoia](docs/screenshots/sequoia-spotlight.png) **Spotlight** |
| ![Finder, Sequoia](docs/screenshots/sequoia-finder.png) **Finder** | ![System Settings, Sequoia](docs/screenshots/sequoia-system-settings.png) **System Settings** with the Sequoia wallpapers |

### 4. Mac OS X Leopard (new)

The 2007 Aqua look, drawn from scratch (nothing reused from the modern themes): the glossy 3D Dock shelf with
reflections, the translucent menu bar, gel buttons, brushed-metal windows, Cover Flow in Finder, System Preferences
laid out like 10.5, Quick Look as a dark HUD, Dashboard, Spaces, Time Machine and the Leopard cursor (your previous cursor comes back when you switch
themes).

![Dock, Leopard](docs/screenshots/leopard-dock.png)

| | |
|---|---|
| ![Finder with Cover Flow, Leopard](docs/screenshots/leopard-finder.png) **Finder** with Cover Flow | ![System Preferences, Leopard](docs/screenshots/leopard-system-preferences.png) **System Preferences** |
| ![Quick Look, Leopard](docs/screenshots/leopard-quick-look.png) **Quick Look** HUD | ![About This Mac, Leopard](docs/screenshots/leopard-about.png) **About This Mac** with your PC's real specs |

## Features

- **Menu bar** with the active app's real menus, the Apple menu, status icons and the system tray.
- **Dock** with magnification, running indicators, stacks (Downloads), minimized windows and the Trash. Put it on
  the **bottom, left or right**, hide it always or only when a window touches it (**smart hide**), and customize
  spacing, distance from the edge, corner rounding, glass color and opacity, labels, indicators and the divider.
- **Change Icon…**: right-click an app in the Dock or Launchpad to use your own PNG/ICO (and restore the original).
- **Genie** minimize animation drawn by the compositor, with smooth edges all the way into the Dock.
- **Finder** (file manager), **Spotlight** (search with Applications, Files, Actions and Clipboard modes),
  **Control Center**, **Notification Center**, **Mission Control**, **Launchpad / Apps**, **Quick Look**,
  **Stage Manager** and **Hot Corners**.
- **Mac title bars** (traffic lights, rounded corners, shadows) on other apps' windows. Chrome, Discord, Spotify and
  other Chromium/Electron apps keep their own bar but get rounded corners and a Mac-style border that matches the
  app's own light or dark look.
- **macOS alerts** in place of Windows message boxes, and the Mac Open/Save panel through Finder.
- **Built-in apps**: System Settings, Activity Monitor (with memory pressure), Terminal, Calculator, Notes,
  Reminders, Calendar, TextEdit, Preview, Photos, Music, QuickTime Player, Clock, Weather, Contacts,
  Time Machine, Screenshot and Force Quit.
- **Memory management like macOS**: memory pressure, App Nap and purgeable memory.
- **Themes**: macOS 27 Golden Gate, macOS Tahoe, macOS Sequoia and Mac OS X Leopard.
- **Dynamic Island** (optional, System Settings > Control Center): music, timers, notifications and volume in a pill
  or a notch at the top of the screen.
- **Game Mode** like macOS: when a game comes to the front, WacOS turns off what costs the most (real glass, mouse
  effects, Hot Corners, file indexing) until the game quits. System Settings > Energy.
- **Launchpad**: hide any app (right-click > Hide from Launchpad), or all of Windows' system tools at once.
- **Icon styles** (Default, Dark, Clear, Tinted), accent colors, light and dark appearance.
- **Customize the desktop** (icon size, grid spacing, text size, widgets), **notifications** (banner duration,
  Notification Center widgets) and the **menu bar** (show it on every display or only the main one).
- **Calendar** with your country's holidays and Monday/Sunday start, **Weather** with any city as your location,
  **Clock** with custom cities and alarms.
- **12 languages**: English (default), Portuguese, Spanish, French, German, Italian, Japanese, Chinese, Korean,
  Russian, Polish and Turkish (System Settings > General > Language). Dates follow the language too.

## Install

1. Download the `WacOS-<date>.zip` from the [latest release](https://github.com/renzyxx/WacOS/releases/latest).
2. Extract it anywhere and open **`WacOSSetup.exe`**: one app that installs, updates, changes and uninstalls WacOS,
   in the style of the macOS Installer.

The release zip includes these fonts, sounds and cursors (they aren't in the repository, and `WacOSSetup.exe`
doesn't download them). They belong to their owners and aren't covered by the WacOS license (see [License](#license)):

- **SF Pro and SF Mono**, Apple's fonts (`fontes` folder; [Apple's license](https://developer.apple.com/fonts/) applies).
  Installing them is optional.
- **macOS alert and interface sounds**, Apple's (`sons` folder). They are copied along with WacOS.
- **macOS Tahoe cursor** by [jimmyxd2 on DeviantArt](https://www.deviantart.com/jimmyxd2/art/1208233550) (`cursor` folder).
  Optional, and only offered with the full WacOS.
- **Mac OS X cursor**, from a pack by RaZcaL inSIDe that credits Apple as the probable original author
  (`cursor-leopard` folder). It is copied along with WacOS and used by the Leopard theme.

If the fonts, alert sounds or Tahoe cursor are missing (for example, in a package you built yourself), the old
`instalar.cmd` can offer to get them: the fonts from Apple (unpacked with 7-Zip, installed through `winget` if needed), 14 alert
sounds from [extratone/macOSsystemsounds](https://github.com/extratone/macOSsystemsounds), and the cursor from its
DeviantArt page, which you download yourself.

WacOS installs for your user only (no administrator rights) in `%LOCALAPPDATA%\Programs\WacOS`.
Everything it changes is logged in `instalacao.log` there. To uninstall, use **Settings > Apps > Installed apps >
WacOS > Uninstall** (or open `WacOSSetup.exe` again). The old `instalar.cmd` / `desinstalar.cmd` scripts are still in
the package for advanced use.

- Quit WacOS: `Ctrl+Alt+Shift+F12`. Emergency exit (brings Explorer back): `Ctrl+Alt+Shift+F11`.

### Use only the apps, or only the title bars

Have your own setup? `WacOSSetup.exe` asks what to install:

1. **Full WacOS** (menu bar, Dock, Finder...).
2. **Only the apps you pick** (Finder, Notes, Calculator, Music, Preview...): shortcuts go to a **WacOS** folder in
   the Start menu. Each app draws its own Mac title bar, with or without the shell running.
3. **Only the Mac title bars** on your other apps: `WacOSTitleBars.exe`, with an optional start at sign-in.
   Turn it off with `WacOSTitleBars.exe --sair` (it also stops when the full shell starts, which takes over).
4. **Apps + title bars**.

Options 2 to 4 keep the Windows taskbar, Explorer and your folders as they are, and don't add the shell's sign-in
task. Themes and language are read from System Settings (install it as one of the apps to change them).
Apps that should never get the Mac title bar go one per line in `%APPDATA%\Shell\janelas-excluidas.txt`.
The built-in apps keep the window corners Windows draws, so mods that change the corner radius (like Windhawk's
rounded corners) should no longer leave a square frame around them.

## Updates

New versions are published as numbered releases (`build-N`). In **System Settings > General > Software Update**,
WacOS checks GitHub on its own, shows the installed version, the latest one and the commits you don't have yet, and
**Update Now** opens `WacOSSetup.exe`, which downloads the new package, checks its SHA-256 against the one GitHub
publishes and installs it (only programs, icons and sounds are replaced; your settings stay).
GitHub Actions checks that every commit to `main` builds.

## Is it safe?

Fair question for anything that replaces your taskbar. You don't have to trust the author:

- **Source available.** Every file is in this repository: read it, and build it yourself (see [Build](#build)).
- **Network.** No telemetry, no accounts. These are the hosts WacOS's own code contacts:
  - `api.open-meteo.com`: the forecast. Receives the coordinates of your chosen cities (until you pick one, a city
    guessed from your Windows time zone). The shell asks at startup and every 15 minutes, even with the widget
    hidden (not in "Dock and menu bar only" mode); the Weather app does the same while it's open.
  - `geocoding-api.open-meteo.com`: city search in Weather and Clock. Receives the name you search for and your
    language.
  - `date.nager.at`: public holidays in Calendar (on by default; untick that calendar to stop it). Receives the year
    and the country code from your Windows region.
  - `suggestqueries.google.com`: Spotlight suggestions. Receives what you type in Spotlight and your language, as
    you type. On by default; `spotlight_google=0` in `%APPDATA%\Shell\ajustes.txt` turns it off.
  - `api.github.com`, with `github.com` as the fallback: updates. The shell checks 90 seconds after it starts and every
    6 hours (turning off the update notification doesn't stop the check), and so do the Software Update page and
    `WacOSSetup.exe` when you update. The check also sends the installed commit's ID, to list what changed.
  - `github.com` and `release-assets.githubusercontent.com`: the package download, when you choose to update.
  - Only in the old `instalar.cmd`, for files missing from the package and only if you say yes:
    `devimages-cdn.apple.com` (fonts), `raw.githubusercontent.com` (sounds) and `winget`, which uses its own hosts
    (7-Zip, to unpack the fonts).

  Links, web searches and the DeviantArt cursor page open in your browser. Run WacOS behind a firewall like
  simplewall or GlassWire and watch for yourself (the update check shows up as `powershell.exe`).
- **What it installs.** One scheduled task to start at sign-in, with normal user rights. No services, no drivers,
  no administrator rights. Uninstalling (Installed apps > WacOS) removes everything.
- **Games are left alone.** The title bar DLL never loads into games, launchers or anything with anti-cheat: it
  checks the program's folder, engine and anti-cheat files next to it, engine window classes and exclusive
  fullscreen, and even if loaded it refuses to start inside a game. Add your own exceptions in
  `%APPDATA%\Shell\janelas-excluidas.txt`.
- **Antivirus.** A shell has to do things that look suspicious to heuristics: start at sign-in, use keyboard and
  mouse hooks for shortcuts like the Win key and hot corners, and load its title bar DLL into other apps. The
  build-12 package shows [1/66 on VirusTotal](https://www.virustotal.com/gui/file/310ab4f16824500a8d77225fef66a62e499c32d6d5484a8752b119b7f24e67f3),
  a single AI-based heuristic. The install scripts are plain PowerShell you can read: no inline C#, no hidden
  windows, and the updater checks the package's SHA-256 against the one GitHub publishes. If Microsoft Defender flags it on your PC, it's a false positive that has been
  reported to Microsoft; please [open an issue](https://github.com/renzyxx/WacOS/issues) with the threat name.

## Testing and stability

WacOS is tested on the author's PC after every change, and the [change log](docs/registro-mudancas.txt) (Portuguese)
records what was changed, how it was checked and what wasn't. Some of the checks:

- **Stress test** (15 to 30 minutes): test programs open, minimize and close windows non-stop while Mission Control
  and Launchpad open and close, and the shell's memory, handles and GDI/USER objects are sampled. They go up and down
  with Mission Control instead of growing; the test apps with the title bar DLL stay flat after 60+ cycles.
- **Watchdog:** the shell restarts itself if it really freezes. A false alarm under heavy animation (it restarted a
  working shell) was found in the stress test and fixed: zero restarts in the following runs.
- **Animations frame by frame:** the genie and other compositor animations are recorded with Desktop Duplication
  (a normal screenshot can't see them) and checked frame by frame. That's how the "pixelated" genie was traced to
  stair-stepped slices and fixed with trapezoid slices.
- **Title bars across updates:** the title bars load through a small fixed loader (`WacOSGancho.dll`), so updating or
  restarting WacOS doesn't use up Windows' limited hook slots anymore; tested by swapping the title bar DLL three
  times in one sign-in.
- **Reported bugs** from Reddit, GitHub and Discord are reproduced when possible (videos help a lot!).

Known limits: apps running as administrator can't get the Mac title bar from a shell that isn't elevated, and the
first install of build-18 or later needs one sign-out so the new title bar loader takes over.

## Roadmap

- **Dynamic Island**: more activities (inspired by [Atoll](https://github.com/Ebullioscopic/Atoll); no code copied).
- **Signed releases**, so antivirus heuristics stop flagging the installer scripts.
- Your idea? [Open an issue](https://github.com/renzyxx/WacOS/issues).

## Support the project

WacOS is free and made in my spare time. If you like it, a coffee helps a lot and keeps the updates coming ☕

<a href="https://buymeacoffee.com/renzyx"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me a Coffee" height="50"></a>

Starring the repository and sharing it also helps.

## Build

Requires Visual Studio 2022 (or Build Tools 2022) with the C++ workload.

```
build.cmd            everything
build.cmd shell      the shell only (WacOSShell.exe: menu bar, Dock, Spotlight...)
build.cmd finder     Finder only
build.cmd janelas    Mac title bars for other apps (WacOSWindow.dll and its fixed loader WacOSGancho.dll,
                     32 and 64 bit, and WacOSTitleBars.exe)
build.cmd titulos    WacOSTitleBars.exe only (the title bars without the shell)
tools\empacotar.ps1  the installer package (dist\WacOS-<date>.zip)
```

## Layout

- `src/shell`: the shell (menu bar, Dock, Spotlight, Control Center, Mission Control, Liquid Glass...).
- `src/finder`, `src/ajustes` (System Settings), `src/monitor` (Activity Monitor) and the other apps in `src/*`.
- `src/janelas`: Mac title bars injected into other apps' windows (`gancho.cpp` is the fixed loader Windows hooks
  in; it loads the current `macjanela.cpp` DLL); `src/titulos`: the title bars without the shell.
- `src/menu`: context menus on top of the real Windows shell extensions.
- `src/comum`: shared code (names, language, glass, shadows).
- `instalador`: install, update and uninstall scripts. `tools`: build and packaging tools.
- `icones`: icon packs (Sequoia, Tahoe) and SF Symbols.

## Performance

Measured on the author's PC (i9-14900K, RTX 3070 Ti, 3440×1440 at 174 Hz). At rest the shell uses about 1% of one
CPU core and no GPU time; with a game in front (Game Mode on) about 2% of one core and no GPU time. Memory: about
350 to 500 MB for the shell (most of it is the Spotlight file index), plus around 100 MB for the hidden Finder that
makes Finder windows open instantly. Animations run on the display's refresh clock, not timers, and nothing polls while
idle. Minimizing a window takes about 55 ms of preparation (window snapshot, detail levels and ~400 animated slices)
before the genie starts; the rest runs on the GPU in the Windows compositor.

On laptops with two GPUs, set `WacOSShell.exe` to **High performance** in Windows Settings > System > Display >
Graphics. If animations still stutter, System Settings > Appearance > Transparency & Effects > **Reduce transparency**
turns off the Liquid Glass blur, the heaviest effect.

## License

**WacOS is source-available, not open source.** Copyright (c) 2026 renzyxx, all rights reserved.
See [LICENSE](LICENSE) for the full terms.

- ✅ You can read the code, build it and run WacOS on your own PCs for personal use, and send pull requests.
- ❌ You can't copy the code (or parts of it) into another project, redistribute WacOS (re-uploads, mirrors,
  repacks, modded builds) or use it commercially without written permission.

Want to use something from WacOS in your own project, or port it? Open an issue and ask first.

Apple's icons, fonts, sounds and wallpapers, and the third-party icon and cursor packs bundled in the release package,
are not covered by the license: they belong to their owners and are included for personal use only. One Windhawk mod
in `docs/windhawk/bloqueio-sequoia/` is derived from GPL-3.0 code and keeps that license.

## Note

Apple, macOS, Finder and related names and icons belong to Apple Inc. This is a personal, non-commercial project
and is not affiliated with Apple. The bundled icons, fonts and sounds are for personal use only.
