# Contribution review: Build 50 customizations

Proposed for upstream review at the author's invitation. This is a GitHub contribution fork only, governed by the unchanged WacOS Source-Available License. WacOS belongs to renzyxx; no claim of ownership, maintainership, affiliation or authority. No binaries, assets, personal settings or release packages are supplied. The author may selectively rewrite or adopt any proposal in their own implementation.

Base: upstream build-50 / f2528d6ad0d78d8b32a49c5731edadc54c87a182. Source snapshot: b847085aa69ebf329a6cfbbcf7d8f50d49640292. All 36 source overlay files verified byte-for-byte against the personal integration snapshot. The original application README and license remain unchanged. Independent optional helpers are under contrib/helpers, not installed or invoked automatically. The example Cairo prayer location is configurable, not a live location record.

The local experimental frame engine changes after b847085 (060e25e, dd919c4, d1cc750, a12c23e) are deliberately not in this branch: titlebar integration, global menu relocation, and exact GPU corners remain incomplete. The older window-frame fixes in this branch are also partial; do not treat this branch as a production-ready release. The author can cherry-pick or reimplement any subsystem.

## Complete functional changelog (grouped)

### WMP1 and independent metrics
- Added declarative, same-user, file-based menu-bar extension interface: UTF-8 WMP1 tab-separated snapshots, individual hit targets, native Direct2D/DirectWrite rendering, grouped read-only rows, headings and dividers, checked actions and optional submenu names. Native theme adaptation, bounded file size/item counts/TTL, sanitized identifiers, atomic producer writes and best-effort action mailbox. Host does not execute provider commands or load plugin DLLs.
- Network upload/download in decimal MB/s using physical adapters; actual CPU package and GPU core temperatures via optional protected LibreHardwareMonitor/PawnIO helper with fresh snapshot requirement and WMI/NVIDIA fallback; missing sensors explicitly unavailable. Local Codex CLI quota RPC shows remaining five-hour and weekly quota without reading authentication files.
- Prayer times via AlAdhan: configurable city/country, calculation authority, Asr school, midnight mode, Hijri calendar and adjustment, custom angles, 12/24-hour formats; yesterday/today/tomorrow schedules; next obligatory prayer countdown with independent final-15-minute warning, Fajr/sunrise and Isha deadline modes, correct rollover and gaps. Location sent to prayer API by city/country, not GPS. Menu includes timings, final deadlines, night times and settings.
- Native metrics grouping before/after utilization monitors; persistent Ctrl-drag order, fixed Control Center/Siri/date, centered clamped native popups. Independent visibility switches for network, CPU temperature, GPU temperature, prayer and Codex in Control Center settings; preserve provider data and ordering.
- Independent provider hardened with exclusive lock and atomic output; optional sensor installer pins official packages and validates hashes/signature; protected directory and read-only sensor task. Installer not certified on a clean PC.

### Glass, notifications, desktop and widgets
- Recover Windows Graphics Capture (WGC) after failures and session transitions; log HRESULTs, retry cooldown, static wallpaper fallback, recover real refraction rather than permanently disabling it. Preserve optical material even at zero tint; calibrated blur, saturation, grain, displacement and highlight treatment; fallback material/rim follows capture state. Notification Center shares backdrop lifecycle rather than covering real glass with opaque cards. Theme-specific Golden Gate/Tahoe calibration; approximations, not Apple's proprietary material.
- Restore desktop internal icon dragging, persist unstacked positions with Stacks enabled, retain OLE for external drags/explicit Alt-drag. Restore Clean Up, Clean Up By, Sort By and Snap to Grid; nearest-free-cell alignment and sorted placement. Theme-aware folder/known app icons and user choice of themed folder overrides; retain personal assets and desktop.ini. Local drive-icon fallback sourced separately; not distributed.
- Responsive isotropic widget layout with consistent pitch/margins, first-fit row packing, DPI/resolution recovery, no saved-layout overwrite, left-group alignment, native-size cap and final 80% default scale. Clock/calendar/weather preserved.
- Stage-specific desktop-item/widget visibility controls independent of ordinary settings; click-wallpaper reveal integration and correct restoration.

### Window decoration and input
- Guarded hybrid titlebar fallback, theme-aware DWM rounding and restored prior DWM preference; preserve protected-process, game, anti-cheat, duplicate-control, privilege and overlap guards. Unsafe toolbar displacement/synthetic click adapter disabled by default; Mozilla custom frame excluded. Custom GPU windows retain native input safety; theme radius included in region cache. Full universal corner/traffic-light fidelity not implemented; do not claim Zen/ChatGPT native controls replaced.
- Local Mac-style Command shortcuts and normal quit behavior; Arabic MacBook layout labeling and local verification utility. Spotlight Windows-key restoration helper. Control Center presentation refinements.
- Optional menu-bar shadow exposed in Golden Gate Appearance; independent from Stage Manager thumbnail fade.

### Finder, Dock, Launchpad and icons
- Shared premultiplied-alpha icon normalization: aspect-preserving 80% body, transparent margins, continuous-corner mask and charcoal fallback for irregular app symbols; refined contour to measured reference. Applied to Dock, Launchpad, modern Spotlight Apps, desktop app shortcuts and Finder app thumbnails. Preserve folders, drives, documents and Leopard silhouette; no original artwork replacement in source.
- Correct chooser/Dock overlap and fullscreen menu reveal; recognize native maximize and custom zoom separately from true fullscreen. Reserve work area on maximized hover, push maximized window below bar, restore full area afterward, retain original maximize/restore geometry; wallpaper backing rather than captured toolbar.
- Finder Open With > Other invokes supported Windows chooser for a single selection; multi-selection guarded.
- Device-local mounted-volume display aliases, Computer grid with two-line available/total labels and utilization bars, existing iCloud Drive integration, iCloud-only sidebar policy and Pictures label. Cosmetic AirDrop entry moved immediately below Recents; not a working Apple AirDrop protocol implementation.
- Independent calendar icon updates weekday/day against a locally supplied base PNG; atomic update and hash guard prevent overwriting manual customization; no third-party artwork or font bundled.

### File operations and trash
- Native IFileOperation compact progress for Finder/desktop local fixed-drive copy, move, duplicate and recycle/delete routes; actual icon, native callbacks, minimize/cancel, pause-aware estimates and undo/recycle/warning semantics. Native fallbacks for unsupported/virtual/removable/network/reparse paths, right-button drags, links, same-folder operations and unsupported shell data formats. No retry after a transfer begins; optimized move avoids source double-deletion.
- Native Explorer progress interception/classification refined; British English Minimise/Maximise not mistaken for conflict prompts, transient UIA retry, actual conflict fallback preserved; bounded route diagnostics omit paths.
- Compact Trash confirmation experiment superseded by restoration of original vertical dialog layout with glass-only transparency/blur adjustment. External Explorer and embedded Finder DLL routes not universally replaced.

### Mission Control / Spaces
- Guard capture and recover sustained graphics target loss after session transitions; local Windows 11 23H2 virtual-desktop ABI correction, retain validation/fallback, animate new Spaces insertion.
- Refracting Spaces pane and add-control animation followed by flat frosted material and blue selected-space treatment. Functional acceptance across all virtual-desktop configurations remains pending.

### Stage Manager (final v9)
- Bounded parking retries preserve original window geometry and restore refused groups; initial app selection fallback, exclude noninteractive shell windows; console-specific grouping and preview handling.
- Fix strip z-order relative to foremost visible desktop; persistent parked entries survive transient visibility changes; refresh groups immediately, preserve good cached previews and avoid competing rectangular DWM overlay.
- Asynchronous preview capture on a dedicated graphics device, bounded first-capture lead time and nonblocking shell UI; WGC and legacy fallback, diagnostic logging.
- Cached alpha-resampled projected preview, near-left/far-right silhouette, position-dependent vertical convergence; reference-calibrated 94 x 80 DIP maximum, horizontal projection factor 0.59, far-edge ratio 0.89, 26 DIP icon, edge placement and projected hit testing.
- v8 Gaussian external shadow experiment superseded by v9 in-card directional opacity: 94% near edge to 74% far edge, premultiplied color/alpha together, subtle edge attenuation; icon stays opaque. No cast shadow. Geometry retained.
- Desktop items hidden by default in Stage Manager, widgets independently configurable; ordinary desktop visibility preserved. Actual installed state retained during upgrades.
- Earlier controlled two-window parking/restoration passed; latest v9 logs show two successful previews, but full latency/flicker/grouping/animation and exact visual parity not certified.

### Integration and release maintenance
- Merged Builds 45, 46, 47 and 50 without discarding custom branch history; Build 50 includes upstream 48/49/50 Save dialog, menu-bar height, Siri time/weather, Dock pin-drag, Finder gallery/column resize and glass fixes. These are upstream improvements, not our authorship.
- Build 50 conflict resolved in Finder: retain richer custom two-line capacity and usage bar while accepting upstream gallery and resizable columns.
- Rebuild affected shell/Finder/Settings, hash-verify existing and replacement executables and version metadata, protect active file operations/Finder sessions, graceful shutdown and rollback. Build 50 retry excludes transient metrics.lock from preference backup, never modifies live lock. Four installed and rollback hashes verified, shell and Finder running. Seven of nine backed-up preferences byte-identical; remaining two differ only in shell runtime state and live metric readings.
- Build 50 interactive regression suite remains pending. Do not infer feature acceptance from successful build/deployment. Official auto-updater may overwrite local changes; merge and rebuild instead.

## Independent helper sources in this archive

| Path | Purpose |
|---|---|
| contrib/helpers/metrics/metrics-provider.py | Independent WMP1 producer, metrics, prayer and quota |
| contrib/helpers/metrics/sensor.ps1 | Read-only hardware snapshot helper |
| contrib/helpers/metrics/install-sensor.ps1 | Optional pinned dependency setup; administrator review required |
| contrib/helpers/calendar/calendar-date.py | Dynamic calendar icon using local user-supplied artwork |
| contrib/helpers/keyboard/verify-layout.py | Local Arabic keyboard mapping diagnostic |
| contrib/helpers/spotlight/restore-spotlight-win.py | Restore local Spotlight shortcut |

Helpers are development records, not an official supported product. Do not run administrative setup blindly. Personal configuration, API credentials, live snapshots, installed drivers, keyboard installers, copyrighted icons/fonts, screenshots and binaries are excluded. The keyboard verifier expects the locally installed layout; calendar helper requires its own local original.png. Sensor installation references official dependencies and their licenses; no dependency binaries bundled.

## All recorded commits (chronological)

- `1e09d52` — Add declarative native menu bar extension interface WMP1
- `9645c58` — Refine native plugin metrics and grouped read-only menus
- `c4f4d16` — Order metrics around native monitors and warn before prayer deadlines
- `ae9b15f` — Support persistent Ctrl-drag widget order and practical prayer reminders
- `b8c285d` — Center native plugin popups on their status widgets
- `2839225` — Recover glass captures, fix notification compositing and desktop interactions
- `5c7f316` — Preserve optical glass at zero tint and restore desktop organization commands
- `1151422` — Tune clear optical glass material and notification tint
- `ee2f9b0` — Enable guarded hybrid titlebar fallback and theme-aware DWM rounding
- `847fb45` — Protect native custom app input and reflow widgets with consistent responsive geometry
- `1653beb` — Cap responsive widgets at native size and preserve left cluster alignment
- `d8bc2bc` — Reduce default desktop widget scale to 80 percent
- `e97ca85` — Merge upstream build-45 with custom capture recovery and WMP1
- `5467f47` — Merge upstream build-46 while retaining WMP1 customizations
- `bf43179` — Normalize modern app icons and correct chooser and fullscreen bar behavior
- `08025c5` — Recognize maximized windows for menu auto-hide and normalize GoldenGate Apps grid
- `596d9ba` — Reserve menu strip on maximized hover and render wallpaper backing
- `941cd1e` — Match normalized icon contour to measured GoldenGate master
- `41c379b` — Invoke supported Open With chooser directly from Finder
- `6af09f4` — Add compact native file operation progress to Finder and desktop operations
- `c2e850e` — Route ordinary Finder and desktop transfers through compact progress and refine trash alert
- `d15254c` — Restore original trash dialog layout and adjust glass transparency only
- `f9abaf4` — Merge build-47 preserving custom dialog transparency and transfer fixes
- `b62229e` — Fix British-English progress button classification and trace transfer fallback
- `94fcfdf` — Expose independent WMP1 visibility switches in Control Center settings
- `5cefc9d` — Device-local display aliases for mounted volumes
- `967f1cf` — Refine Computer storage grid and add existing iCloud Drive to Finder sidebar
- `1c074a4` — Use iCloud-only Finder sidebar and macOS Pictures label
- `a1f93c6` — Add cosmetic AirDrop sidebar entry in modern Locations
- `a50e447` — Move cosmetic AirDrop directly below Recents
- `fabfc51` — Refine local Command shortcuts, normal quit and Control Center presentation
- `0639369` — Label local Arabic MacBook layout and guard Mission Control capture
- `421c8a5` — Fix 23H2 virtual desktop ABI and animate new Spaces insertion
- `a765103` — Recover from sustained graphics target loss after session transitions
- `05dedb2` — Render refracting Mission Control Spaces pane and animate desktop from add control
- `2feb58a` — Match native Spaces material with flat frosted strip and blue selection
- `215042e` — Improve Stage Manager parking and grouping, add optional menu bar shadow
- `e6e6a9b` — Expose shadow in GoldenGate Appearance and render stable perspective stage previews
- `83382d0` — Fix Stage Manager preview direction, capture and bounded parking
- `c6606e7` — Handle console previews and exclude noninteractive Stage Manager windows
- `265cbc1` — Cache alpha-resampled Stage Manager perspective and refresh groups immediately
- `03de366` — Keep Stage Manager strip above foremost visible desktop
- `1835149` — Move Stage capture off UI thread and add desktop visibility and perspective depth
- `13ac789` — Calibrate Stage Manager thumbnail yaw and edge placement against macOS
- `0f53027` — Apply directional translucency inside Stage Manager previews
- `b847085` — Merge upstream build-50 preserving device-local customization and Finder capacity grid



## Review priorities

WMP1 plugin interface and independent producer; graphics recovery; icon normalization; file operation progress; desktop and widget layout; Stage Manager and Spaces; Finder volume information. Build 50 shell, Finder and settings were rebuilt and installed on one Windows machine; full interactive regression is outstanding. Test against clean installs, multiple DPIs, different GPUs and all themes before adopting changes.
