The HUD sources are `layout/custom_game/hud.xml` and `styles/entwatch/hud.css`.

With CS2 Workshop Tools installed, regenerate the compiled assets from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/compile-panorama.ps1 -Cs2Root "F:\steamgames\steamapps\common\Counter-Strike Global Offensive"
```

The script stages a local `cs2fixes_hud` addon and copies its compiled VXML/CSS into `assets/panorama`. AMBuild packages those resources under `package/cs2/panorama`, alongside the existing sounds and particles. Rebuild CS2Fixes after compiling.

Install the package into `game/csgo`. Clients must also have these resources installed or mounted through an addon; installing a server plugin does not distribute files to clients. The staged local addon is available for Workshop Tools testing.

The green reload icon represents the item's EntWatch ability cooldown. The orange ammo icon represents remaining configured uses or a math-counter value, not the weapon's bullet magazine. Multiple visible handlers are separated with `|`. Items without these handlers show their normal ready state.

Player-name font size is 16px at 100% HUD size.

If server-side Panorama resources are missing or entity creation fails, the HUD temporarily falls back to world text and retries at most once every 30 seconds. The player's selected renderer is retained. This cannot detect missing client-side resources. Enable `cs2f_entwatch_panorama_debug 1` for creation diagnostics; it is off by default. Normal disconnect/spawn timing does not generate warnings.
