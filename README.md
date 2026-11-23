# Transporter-SF

A Star Trek-style transporter for Starfield. **Teleport** beams you and your followers out and
in again, with a countdown, a gesture, lights, shaders and sounds; the **transporter menu**
(OSF UI) beams you to your ship or to a point of interest on the planet; its options are on an
**OSF Settings** page. Split from CCFE-SF in 0.7.0 (CCFE-SF keeps the console info commands);
see "Moving from CCFE-SF 0.6.x" if you used the transporter there.

| Part | File | What it does |
|---|---|---|
| Papyrus script | `scripts/Source/Transporter_SF.psc` → `Data/Scripts/Transporter_SF.pex` | Does the work, with vanilla and Cassiopeia functions: the beam sequence, the menu's list, the settings. |
| DLL functions | `scripts/Source/Transporter_SFNative.psc` → `Data/Scripts/Transporter_SFNative.pex` | Declares the functions the DLL gives the script (open the menu, answer its list, show the countdown, find map markers). Needed in game. |
| SFSE plugin | `src/` → `Data/SFSE/Plugins/Transporter_SF.dll` | Connects the transporter menu and its hotkey to OSF UI and OSF Settings; drives the countdown box; registers the console commands with ModernCommandFramework. |
| Settings page | `dist/Data/SFSE/Plugins/OSF/Settings/schemas/transporter-sf.json` | The transporter's options and menu hotkey, shown in OSF Settings (Pause → MOD SETTINGS, or F10). |
| Transporter menu | `dist/Data/SFSE/Plugins/OSF/UI/views/transporter-sf/menu/` | The OSF UI page: your ship and local points of interest. |
| Countdown box | `dist/Data/Interface/Transporter_SF_Countdown.swf` (+ `_LRG`) | An empty menu movie that brings in the game's grav-jump countdown box; the DLL drives it. |
| INI | `dist/Data/SFSE/Plugins/Transporter_SF.ini` | The same options, used when OSF Settings isn't there; and the fast-travel probe. |

The script works on its own, so you can test a beam before building the DLL:

```
cgf "Transporter_SF.Teleport" "0001ABCD"
```

With the DLL and ModernCommandFramework installed, the same command is `Teleport 0001ABCD`.

## Requirements

- SFSE and Address Library for SFSE Plugins
- Cassiopeia Papyrus Extender (the script calls it throughout)
- ModernCommandFramework, for the short command names only
- For the transporter menu and in-game settings: OSF Settings 1.0 and OSF UI 2.0 (which needs the Microsoft Edge WebView2 Runtime; Windows 10/11 usually have it). Without OSF Settings the INI is used.
- To build: Starfield Creation Kit (Papyrus compiler); for the DLL, Visual Studio 2022 Build Tools (C++), XMake 3.0+ and Git

## Commands

Arguments are separated by spaces. A destination can be `player`, a hex form ID without `0x` (`14`, `0001ABCD`), or an EditorID. `?` as the first argument prints the usage line.

| Command | Arguments | Does |
|---|---|---|
| `Teleport` | `<reference \| cell \| quest> [solo] [nofx] [ft]`, `ship [solo] [nofx]` or `xyz <x> <y> <z> [solo] [nofx]` | Runs the transporter sequence (below) and moves you, and followers within `fPassengerRadius`, to a reference, a cell (by its EditorID), a quest's target, your home ship (behind the pilot seat), or coordinates. `solo` leaves followers; `nofx` skips countdown, gesture and effects; `ft` uses the game's own fast travel for a reference destination. |
| `TransporterMenu` | none | Opens the transporter menu (the same as its hotkey). |
| `TransporterCountdown` | `[seconds]` | Shows the grav-jump countdown box (default 3 s). Needs the DLL; handled in C++, so there's no `cgf` form. |

The `cgf` forms are `cgf "Transporter_SF.Teleport" "<arguments>"` and `cgf "Transporter_SF.TransporterMenu" ""`.

## Fix compared with the Skyrim handler

- **Teleport** never reset its loop counter before moving followers in, so followers were not brought along. Each loop now has its own counter.

## Where your settings are saved

Values you change on the OSF Settings page are saved by OSF Settings in `Documents\My Games\Starfield\OSF\Settings\transporter-sf.json`, not in `Transporter_SF.ini`. The INI is only read when OSF Settings isn't installed. To make your choices the defaults for everyone, copy them from that file into the schema's `default` values (and the INI).

## Install

1. Build or copy `Transporter_SF.pex` and `Transporter_SFNative.pex` into `Data/Scripts/`.
2. Copy everything under `dist/Data/` into `Data/`: the INI, the `SFSE/Plugins/OSF/...` folders for the settings page and the menu, and `Interface/Transporter_SF_Countdown*.swf` for the countdown box. Loose interface files need `bInvalidateOlderFiles=1` and an empty `sResourceDataDirsFinal=` under `[Archive]` in `StarfieldCustom.ini`; if other loose UI mods (such as the grav jump overhaul) work, you already have them.
3. Put `Transporter_SF.dll` in `Data/SFSE/Plugins/`. The menu, its hotkey and the countdown box need it; with ModernCommandFramework installed it also gives the short command names.

With a mod manager, make a mod (e.g. `Transporter-SF`) whose `Data` folder holds the same layout. It doesn't need CCFE-SF.

## Moving from CCFE-SF 0.6.x

Every name changed (`CCFE_SF` → `Transporter_SF`, OSF id `ccfe-sf` → `transporter-sf`), so the old files would otherwise stay around as a second, dead copy.

1. **Remove the old transporter files from the CCFE mod folder:** `Scripts\CCFE_SFNative.pex`, `Scripts\CCFE_SFBridge.pex` (if there), `Interface\CCFE_TransporterCountdown.swf` and `_LRG.swf`, `SFSE\Plugins\CCFE_SF.ini`, `SFSE\Plugins\OSF\Settings\schemas\ccfe-sf.json` and the folder `SFSE\Plugins\OSF\UI\views\ccfe-sf`. Then update CCFE-SF to 0.7.0 (its new `CCFE_SF.pex` and `CCFE_SF.dll` no longer have the transporter).
2. **Keep your settings:** rename `Documents\My Games\Starfield\OSF\Settings\ccfe-sf.json` to `transporter-sf.json`. The keys are the same, so your light, shader and sound forms and your hotkey carry over. If you edited `[Transporter]` or `[TravelHook]` in `CCFE_SF.ini`, copy those lines into `Transporter_SF.ini`.
3. **Install Transporter-SF** as above, and delete OSF UI's views cache if the menu still shows the old page (its footer should read `v0.7.0`).

The log moves too: `Documents\My Games\Starfield\SFSE\Logs\Transporter_SF.log`. No plugin file (`.esm`) is involved, so saves are unaffected.

## Compile the script

1. Install the Starfield Creation Kit from Steam.
2. Extract the `Scripts` folder from `Starfield\Tools\ContentResources.zip`. It holds the vanilla sources and `Starfield_Papyrus_Flags.flg`.
3. Edit the paths at the top of `compile_papyrus.bat` (including `MOD_SCRIPTS`, your mod manager's `Scripts` folder for this mod), then run it. It stops and says so if any script fails to compile; otherwise it also copies the new `.pex` files to `MOD_SCRIPTS`. The `.pex` files land in `dist\Data\Scripts`; `MOD_SCRIPTS` is set to `C:\Modding\Starfield\mods\Transporter-SF\Scripts`, so make that mod folder first or change the line. It needs `OSFSettings.psc`, which OSF Settings installs in `Data\Scripts\Source`; OSF UI's scripts are no longer used.

## Build the plugin

In a terminal, in this folder:

```bat
git clone https://github.com/ozooma10/commonlibsf lib/commonlibsf
git -C lib/commonlibsf checkout 42425651185684675b7d31d503f1b38eeb6753d3
git -C lib/commonlibsf submodule update --init --recursive
xmake build
```

If your CCFE-SF folder already has `lib\commonlibsf`, copy that folder here instead of cloning: both projects use the same commit. This is OSF's fork of CommonLibSF, at the commit OSF Settings 1.0 is built with. The countdown menu needs it: the fork's custom-menu support was checked against Starfield 1.16.244, and upstream's has no game address for its menu constructor.

The DLL lands under `build\windows\x64\`. If you set the environment variable `XSE_SF_GAME_PATH` to your Starfield folder (or `XSE_SF_MODS_PATH` to a mod manager's mods folder), `xmake install` copies it into place. `xmake project -k vsxmake` makes a Visual Studio solution; `xmake project -k compile_commands` gives VS Code's clangd what it needs.

## Test in this order

After each update, `cgf "Transporter_SF.StartSession" "verbose"` prints `script 0.7.0, DLL 0.7.0`. If the two differ (or the DLL shows empty, meaning `Transporter_SFNative.pex` is old), the game is running files from different builds; the transporter menu then says so in red instead of letting you beam. The menu page shows its own build in the footer (`v0.7.0`) and warns, without blocking, when it differs from the scripts'; if it keeps showing an old build after you copied the new files, delete OSF UI's views cache so it copies the page again.

1. **Script alone.** Click a nearby object with the console open, note its ID, close, then `cgf "Transporter_SF.Teleport" "<that ID> nofx"`. If nothing happens, check that Cassiopeia is installed and the `.pex` files are in `Data/Scripts`.
2. **DLL.** `Documents\My Games\Starfield\SFSE\Logs\Transporter_SF.log` lists the registered commands (or says that ModernCommandFramework wasn't found) and the menu connections below.
3. **Settings page.** Pause → MOD SETTINGS (or F10): "Transporter" should be listed. If it shows under Mod Issues instead, the details are in `OSF Settings.log` in the same Logs folder.
4. **Countdown box.** Type `cgf "Transporter_SF.Countdown"` (or `TransporterCountdown 3`). The grav-jump box should appear just below the middle of the screen, count down from 03 with its sounds, and close. If nothing shows, `Transporter_SF.log` says why (menu not registered, movie not loaded, or `DefaultGravJumpComponent` not found).
5. **Menu.** `Transporter_SF.log` should show `MenuBridge: connected to OSF UI ... targets true, beam true, reopen events true` at startup, `MenuBridge: transporter hotkey registration: status 0` and `MenuBridge: script functions bound on Transporter_SFNative`. Load a save on a planet and type `cgf "Transporter_SF.StartSession" "verbose"`: it should print `hotkey true ... menu bridge connected true`. Press F7, try a point of interest, then the ship.

## The transporter sequence

`Teleport` runs five steps. Every slot is an option on the OSF Settings page (or a line in `[Transporter]` of `Transporter_SF.ini` without OSF Settings), read each time, so you can tune the look in game: change it, beam again.

1. **Countdown:** the grav-jump countdown box for `iCountdownSeconds` (shown by the DLL), or notifications ("Transporter lock in 3, 2, 1") with `eCountdownStyle=notifications` or when the box can't open.
2. **Gesture:** `sPlayerIdle` / `sFollowerIdle` (a vanilla idle), and/or a custom animation file (`sPlayerAnimFile`) played through an animation framework. Then it waits `fGestureSeconds`.
3. **Beam out,** on you and each passenger: a real light (`sOutLight`, placed `fLightHeight` above them), an effect shader, a spell and a Wwise sound; the screen effect (`sOutImod`) once. Wait `fDelayBeforeMove`; the out lights are removed.
4. **Transit:** fade, move (or the game's fast travel with `ft`), wait until you have arrived, followers placed in a row behind you. The game moves followers on its own after a cell change, and an NPC set down off the navmesh is pushed to the nearest navmesh, so for about five seconds anyone more than a few metres off is put back (on your own spot after two tries). `Papyrus.0.log` gets a `Transporter_SF: follower ... moved back` line each time, and one with the first follower's final distance.
5. **Beam in:** the same slots with `In`; the in lights stay `fInLightSeconds`.

Forms come from the game or from your own plugin made in the Creation Kit: a blue-white light, a shimmer shader, a short sound, a bright flash imod. Fill one slot at a time and test with `Teleport <some nearby ID>`.

## The transporter menu

Press **F7** (rebind it on the OSF Settings page or in Controls), type `TransporterMenu`, or open it from OSF Settings' Launcher tab. It lists:

- **Your ship.** Usable according to *Beam to the ship when* (`eShipRule`): *landed in this area*, *on or above this planet* (default) or *anywhere* (experimental). You arrive `fShipSeatOffset` units behind the pilot seat. Aboard, the row says so, and the list below shows the points of interest around the ship, so you can beam back down.
- **Points of interest on this planet,** nearest first: map markers with a map name, only discovered ones by default (`bDiscoveredOnly`), up to *Most locations listed* (`iPoiMax`). Markers in the planet's other areas (another city or landing site) come after the ones around you, marked "elsewhere on this planet", unless *Whole planet* (`bWholePlanet`) is off. A *Search radius* (`fPoiRadius`, 0 = no limit) keeps the list to your current area.

Arrow keys and Enter, or double-click; typing filters the list; Esc clears the filter, then closes. Energize closes the menu and runs `Teleport` with the full sequence and your followers. It is greyed out only when no target is selected, the target is unavailable, you are in combat (`bBlockInCombat`), the scripts and DLL are from different builds, or for 1.5 s after a click (so a double-click doesn't beam twice).

**How the points are found.** The DLL walks every worldspace's persistent cell (where map markers are kept, loaded or not) and keeps the map markers whose location lies under your planet's location; `Transporter_SF.log` reports the counts on each scan (`FindMapMarkers: ...`). The script also runs its older two searches and keeps the markers with a map name: references of the base form `sMapMarkerBase` (default the EditorID `MapMarker`) in the loaded area, and the persistent references of the area you're in (map markers are normally persistent, which should reach unloaded ones too). It's the part most likely to need tuning; see the known limits.

**How it's wired.** The page's `targets` and `beam` endpoints are registered by the DLL through OSF UI's C++ interface, which lasts for the whole run. The DLL asks the script for the list (`Transporter_SF.AnswerTargets`), the script answers through `Transporter_SFNative.ReplyTargets`, and Energize closes the menu and calls `Transporter_SF.BeamFromMenu`. The script doesn't call OSF UI's own Papyrus functions: in OSF UI 2.0.0 they are bound only the first time the game runs its native-binding step, the game runs that step again (OSF Settings' log shows it binding twice per run), and after that every `OSFUI.*` call fails with "Unbound native function". The DLL binds its own functions every time, as OSF Settings does.

**Hotkey.** The DLL registers the hotkey with OSF Settings' C++ interface once, at startup; C++ registrations last for the whole run, so nothing has to be renewed after a load. (Earlier builds called the script after every load to re-register it; doing that while the save was still loading coincided with the after-load freezes and crashes, so nothing calls into Papyrus during a load any more.)

## The watch gesture

**Vanilla watch idle (works now).** The game has a Chronomark watch-check animation; the Useful Chronomark Watch mod (Nexus 14501) plays it from a favourited consumable. To find its Idle form, type `help watch 4 idle` in the console (it lists idles whose names contain "watch"; try `chronomark` too) and put the Idle's ID or EditorID in `sPlayerIdle`. Limits its author reports: the animation can't be changed (no walking while it plays), and with a spacesuit on it works "not reliably" because of clipping. The watch model itself only shows during the intro or with a mod that adds it to your wrist (e.g. Nexus 8092). Whether followers can play the player's idle is untested; leave `sFollowerIdle` empty or use another gesture.

**Ben 10-style activation (custom animation).** The Creation Kit can't create animations, but animation frameworks play glTF files made in Blender:

- *Native Animation Framework SF (NAFSF)* plays `.glb`/`.gltf` files from `Data/NAF`, ships a Blender scene with the rigged skeleton among its optional files, and has a Papyrus script `NAF` with `PlayAnimation(Actor, String)` (its animations loop until stopped).
- *Starfield Animation Framework (SAF)* has Papyrus functions including one-shot playback and `StopAnimation(Actor)`.
- *OSF* and the *Starfield Animation IO* Blender tools are newer options.

Transporter_SF calls the framework by name (`sAnimScript`, `sAnimPlayFunction`, `sAnimStopFunction`), so it compiles and runs without any of them installed. For NAF: `sAnimScript=NAF`, `sAnimPlayFunction=PlayAnimation`, `sPlayerAnimFile=Transporter\WatchActivate.glb`, plus its stop function if you want it cut at the move.

A choreography that reads as the Omnitrix in about two seconds: left forearm swings up level across the chest (0.3 s); right hand pinches the dial and twists it a quarter turn (0.4 s); dial pops, a short beat looking at it (0.3 s); right palm slams down on it (0.2 s); hold the pose as the shimmer starts (rest of `fGestureSeconds`). Time `fGestureSeconds` to end on the slam.

## The grav-jump countdown box

The countdown step shows the real grav-jump box from the ship HUD, on foot, with its spool-up animation and sounds (`eCountdownStyle=gravJump`, the default; `notifications` brings back the old text).

**How it works.** The box lives in `Interface/GravJumpComponent.swf`. The ship HUD imports that file and sends the box four values each frame (progress 0–1, seconds until the jump, "started", "initiated"). Transporter-SF does the same thing outside the ship:

- `Data/Interface/Transporter_SF_Countdown.swf` (and `_LRG` for large text) is an empty menu movie that imports `GravJumpComponent.swf`, the way the HUD does. It holds no art and no code; `tools/make_countdown_swf.py` writes it.
- `Transporter_SF.dll` registers that menu, creates the file's `DefaultGravJumpComponent` where the HUD puts it, and feeds it a pretend jump once per frame (`src/CountdownMenu.cpp`). It plays the box's stage sounds (load, mod, spool, calc, ready) and the per-second tick itself, because the box can only reach the game's audio from inside the ship HUD.
- `Countdown()` shows it through the DLL's `Transporter_SFNative.ShowCountdown` and waits until the box has finished (99/91 of the countdown: the box reads 00:00, then plays its closing frames). The console command `TransporterCountdown [seconds]` shows it too; `Debug.ExecuteConsole` can't reach that command ("script command not found"), hence the script function.

**Which box.** Whatever `GravJumpComponent.swf` the game loads: the vanilla one from the game's interface archive, or a replacement such as *Grav Jump Component UI Overhaul* (Nexus 15154), which adds an Abort button that the transporter hides. Nothing from that mod is included here, so it stays an optional requirement; including its file in a release would need its author's permission.

## Fast-travel hook: step 1, the probe

Delaying the game's own fast travel (map, star map, scanner) needs a C++ hook at the moment you confirm the trip. CommonLibSF names two callback types after that moment, `StarMap::Util::ConfirmFastTravelCallback` and `nsMonocle::MonocleFastTravelConfirmFunc`, but not their layout. The probe finds out:

1. Set `[TravelHook] iMode=1` and start the game. `Transporter_SF.log` should say `probe: hooked …` for each callback (or why it refused, e.g. the slot doesn't point into the game's code).
2. Fast travel once from the map, once from the star map (to another planet), once from the scanner; also open and cancel a fast travel prompt.
3. Send me the `probe:` lines from `Documents\My Games\Starfield\SFSE\Logs\Transporter_SF.log`.

If a slot logs nothing when you confirm, try `iVfuncIndex=2`, then 3. The probe always lets the call through, so the game behaves as before. From the logs (when it fires, which argument is the button, where the object keeps its destination and reference count) the next version can hold the trip, play "out", then release it.

## Known limits and untested parts

The menu, ship beam, map markers, countdown box, lights and shaders work in game; these are the parts still untested or most likely to need fixes:

- **Point-of-interest search.** Works on foot on a planet; from inside your ship it falls back to the ship's planet, which is untested. Also untested: procedurally generated planets, and how long a big area takes to scan (the menu shows "Scanning…" and waits up to 30 s). If the list is empty where your map shows markers, tell me what the area is.
- **Ship rules.** "On or above this planet" compares `GetCurrentPlanet()` of you and the ship; whether that returns the planet for a ship in orbit is untested. "Anywhere" can put you aboard a ship in another system.
- **Short console commands** (`Teleport 0001ABCD`, `TransporterCountdown 3`) depend on ModernCommandFramework adding them to the console. If the console says "script command ... not found", check `ModernCommandFramework.log`: "Delayed ScriptCommands install failed after retries" means it couldn't, for every mod. The `cgf "Transporter_SF.<function>" "<arguments>"` forms still work, and the menu, hotkey and countdown don't use the console.
- **F7** is unbound in vanilla Starfield as far as I know; if another mod uses it, rebind it.
- **Moving the player while in space**, and `Teleport` to a quest (it runs the console's `MoveToQuestTarget`), are untested.
- **Calling an animation framework by name** (`Utility.CallGlobalFunctionNoWait`) needs the framework's exact function signature; check its own script if nothing plays.
- **The probe** writes into two vtables whose slot meaning is a guess (slot 1 = run). It refuses any slot that doesn't point into the game's code, and passes four register arguments through; a callback that took floating-point or more than four arguments would misbehave under the probe.

## License

The plugin links CommonLibSF (GPL-3.0 with a modding exception), so the plugin source is GPL-3.0. `src/MCF.h` is adapted from ModernCommandFramework's `MCF_API.h`, which its header says may be copied into other plugins. `src/OSFUI.h` and `src/OSFSettings.h` are OSF UI's and OSF Settings' SDK headers.
