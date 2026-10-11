The HUD sources are `layout/custom_game/hud.xml` and `styles/entwatch/hud.css`.

With CS2 Workshop Tools installed, regenerate the compiled assets from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/compile-panorama.ps1 -Cs2Root "F:\steamgames\steamapps\common\Counter-Strike Global Offensive"
```

The script stages a local `cs2fixes_hud` addon and copies its compiled VXML/CSS into `assets/panorama`. AMBuild packages those resources under `package/cs2/panorama`, alongside the existing sounds and particles. Rebuild CS2Fixes after compiling.

Install the package into `game/csgo`. Clients must also have these resources installed or mounted through an addon; installing a server plugin does not distribute files to clients. The staged local addon is available for Workshop Tools testing.

The item HUD uses one shared `custom_hud_layout` with per-player classes and dialog variables. `src/entwatch.cpp` gathers held items; `src/entwatch_hud.cpp` implements Panorama and renderer selection, while `src/entwatch_hud_worldtext.cpp` preserves the world-text renderer without any Panorama dependency. It shows up to 64 stacked items and an overflow count. Only configured, held items with `ui: true` are shown. No client scripts are required.

`!hudstyle old`, `!hudstyle new`, or `!hudstyle default` select a renderer and persist the player's preference; bare `!hudstyle` toggles. `entwatch_hud_style 0/1` selects the server default for players without an override. `!hud` still cycles visibility and owner names. `!hudpos` and `!hudsize` configure the currently selected renderer. Each renderer keeps its own position and size. `!hudcolor` still customizes world text; Panorama colors live in the CSS.

The green reload icon represents the item's EntWatch ability cooldown. The orange ammo icon represents remaining configured uses or a math-counter value, not the weapon's bullet magazine. Multiple visible handlers are separated with `|`. Items without these handlers show their normal ready state.

`!hudpos [x y|reset]` configures the active style: world-space coordinates for old HUD, whole screen percentages (0-100) for Panorama. Panorama defaults to 2% left / 15% top; keep enough space to the right and below for the item list. `!hudpos2` remains an explicit Panorama-only alias.

`!hudsize [size|reset]` configures the active style: old HUD font size (20-255, default 60), or Panorama size as a whole percentage (50-200, default 100). Panorama scales the rows, padding, fonts and icons together while keeping its screen anchor. Bare commands report the active style's values. Client console equivalents are `c_hudpos` and `c_hudsize`.

World text retains the existing `entwatch_hudpos_x`, `entwatch_hudpos_y`, and `entwatch_hudfontsize` preference keys. Panorama uses separate `entwatch_panorama_hudpos_x`, `entwatch_panorama_hudpos_y`, and `entwatch_panorama_hudsize` keys. Both sets stay on the player independently of `entwatch_hud_style`. The user-preferences system keeps values in memory per player slot; persistence across reconnects/restarts requires the existing `cs2f_user_prefs_api` REST backend, keyed by SteamID. These commands do not write a local config file.

Panorama item and owner names share one line, with long names ellipsized to preserve the right-aligned status area. Ammo appears before the cooldown; the green reload icon sits at the far right and rotates while cooling down. Size preferences scale the compact row and icon together.

Player-name font size is 16px at 100% HUD size.

If server-side Panorama resources are missing or entity creation fails, the HUD temporarily falls back to world text and retries at most once every 30 seconds. The player's selected renderer is retained. This cannot detect missing client-side resources. Enable `cs2f_entwatch_panorama_debug 1` for creation diagnostics; it is off by default. Normal disconnect/spawn timing does not generate warnings.

Release verification still requires an in-game pass with the packaged resources on server and clients:

- Pick up, transfer, drop and exhaust items; check cooldowns and multiple handlers in both renderers.
- Switch styles, names-only mode and visibility; change/reset position and size independently for both styles.
- Reconnect and change map/round, including after disabling EntWatch; check that player slots do not inherit HUD contents.
- With the preference REST backend configured, reconnect and restart the server and verify both styles' settings. Check malformed numeric preferences and command arguments (`nan`, `inf`, overflow and trailing text).
- Remove server-side resources in a test installation and verify the warning/fallback/retry behavior; restore resources and confirm recovery.
- Exercise 64 players/items and overflow, long names, several resolutions and HUD sizes. Confirm readable text and acceptable network traffic.
