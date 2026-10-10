# TeeTycoon bots

This is the bot code used by TeeTycoon: pet helpers, pet blockers, and the admin blocker bots. I keep it here so the movement and combat work can be changed without digging through the whole game server.

The small headers in `include/bot_ai/` are the reusable part. They only use the C++ standard library. `adapter/teetycoon/` is the DDNet 20.1.1 adapter from TeeTycoon. It reads the map and character state, runs the DDNet movement core for short predictions, and turns decisions into player input. The adapter also contains the older map graph. It still depends on TeeTycoon's bot IDs, teams, pet data, and server changes; copying it into an unmodified DDNet checkout will not compile.

## Put this version into TeeTycoon

Use the `dev` branch of [TeeTycoon](https://github.com/Yairbd2004/TeeTycoon). From PowerShell, in this repository:

```powershell
./install-teetycoon.ps1 -ServerDir ../TeeTycoon/TeeTycoon/src/game/server
```

The script copies the adapter and the reusable headers into the server source tree. It replaces those bot source files, so check `git diff` in TeeTycoon before building. It does not touch accounts, maps, configs, or the build directory.

Build the server in TeeTycoon's existing `TeeTycoon Server` build directory. The full command and Windows toolchain setup are in TeeTycoon's `README-INSTALL-AND-STANDARD.md`. The client needs no bot code changes.

## Use it in another DDNet mod

Start with `include/bot_ai/brain.h` and `skills.h`. They do not need TeeTycoon. Implement the observations and movement predictor expected by `ChooseMovement` in your server, then feed the returned direction, jump, hook, aim, and weapon choices to your normal bot input path.

For a full tee bot, read [PORTING.md](PORTING.md) before taking code from `adapter/teetycoon/`. That adapter is an example of the required DDNet wiring, including collision sampling, wall hooks, frozen-player rescues, and bot snapshots. The code under `adapter/teetycoon/ai/` is part of its old map navigation system.

Skills run from level 1 to 10. A skill upgrade changes the next bot decision; it does not grant a weapon. Pets use their saved weapon tier, while admin blockers use level 10. Hard map geometry and player fights still need live checks after each change; a successful compile cannot prove a rescue or throw will work on every map.

Blockers stop pursuing a tee after that tee has stayed frozen on a freeze tile for one second. This includes airborne freeze. When the tee leaves the tile, the bot can target them again during the remaining freeze timer. The TeeTycoon game context owns that timer, so all bots see the same result.

The adapter contains modified DDNet/Teeworlds code. Keep the original notices when redistributing it; see [LICENSE-DDNet.txt](LICENSE-DDNet.txt).
