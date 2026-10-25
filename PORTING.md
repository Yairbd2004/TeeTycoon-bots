# Connecting the bots to a DDNet server

The working adapter targets TeeTycoon's DDNet 20.1.1 fork. Use the same DDNet revision when comparing it with another mod. The pure policy headers are portable; the adapter needs the host server's types and lifecycle.

1. Add `bot.cpp`, `bot_navigation.cpp`, `botengine.cpp`, and the three `.cpp` files in `ai/` to the server build. Put the policy and navigation headers under a `bot_ai/` include directory. TeeTycoon's CMake file already lists these sources.
2. Own a `CBotEngine` from `CGameContext`. Initialize it after the collision map is loaded and destroy it before the game context goes away. `CBotEngine::InitializeForBot` prepares navigation for a new bot.
3. Give each bot a `CPlayer` and a character. In `CGameContext::OnTick`, call `CBot::GetInputData()` once per bot and pass the returned input to `CPlayer::OnPredictedInput()` before the normal world tick. TeeTycoon's `gamecontext.cpp` is the reference for this order.
4. Wire bot creation and removal. TeeTycoon's `CGameContext::AddBot` in `teetycoon.cpp` is the current example. A pet stores its owner ID; an admin blocker uses owner `-1`. Keep a bot in the owner's DDRace team when spawning, respawning, or moving to a house.
5. Decide how IDs work in your server. TeeTycoon has server and snapshot changes for virtual bot IDs above the human connection range. An ordinary DDNet server cannot simply create ID 64 and send packets to it: network recipient IDs are real client slots. Either use an ordinary slot or port TeeTycoon's virtual ID, snapshot mapping, and network recipient guards together. When every visible ID is occupied, the virtual tee must be hidden from a standard client.
6. Supply the game-specific state used by `bot.cpp`: the player's pet skill array, owned weapon tier, owner and target relations, freeze timeout, and `IsBotFrozen`. TeeTycoon stores pet values in SQLite and refreshes a spawned bot with `CBot::OnSkillUpgrade()` after a purchase. If your mod does not have accounts, replace those reads with your own settings.
7. Keep the DDNet physics rules in sync. The adapter calls `CCharacterCore::Tick`, `Move`, and `Quantize` to assess some hook and movement choices. Hammer eligibility mirrors the ordinary `CCharacter::FireWeapon` hit query; don't change the server hammer radius for bots. Team collision and `CanKeepHook` must agree with who the bot is allowed to pull or hit.
8. Keep a per-character freeze-tile dwell timer in the game context and expose `IsPlayerFreezeLocked` to the adapter. TeeTycoon samples each character's center before bot inputs every server tick. A continuously frozen tee on a freeze tile becomes a completed block after one second; leaving the tile or respawning resets it. This works for airborne freeze and speed tiles too.

The complete TeeTycoon integration also changes `gamecontext.cpp`, `gamecontext.h`, `player.cpp`, `player.h`, `teams.cpp`, `playermapping.cpp`, `character.cpp`, the engine server, and account code. Those files are in the TeeTycoon repository, because they are shared game and protocol behavior rather than standalone bot policy. Compare those files when porting the full adapter. Take the ID and packet handling as one unit; partial copies can crash the server.

For a smaller first port, use slot-backed bots and the pure policy headers. Add virtual IDs only after the slot-backed path and snapshots are working.

## Navigation in v0.3.0

`navigation.h` is a resumable sparse graph search with no game-engine dependency.
`bot_navigation.cpp` supplies collision, hazards, directed teleporter edges and
short movement predictions using DDNet's `CCharacterCore`. It is part of the
server build, not an optional tool. Copy the matching `botengine` scheduler too:
searches and movement predictions share a fixed allowance across all bots.

The graph describes body-clear corridors, while the controller checks concrete
jump and hook inputs against physics. It is not a proof that every corridor is
traversable: unusual mechanisms, timing puzzles and other tees still need tests.
A portal exit inside freeze is accepted only if its simulated fall reaches a
safe cell. That behavior is tested on TeeTycoon's teleporter 249.

TeeTycoon's `tools/test_bot_navigation.py` runs a real server with a temporary
database and forces a navigation destination through the admin `tt_bot_nav`
command. The full mod owns that command; the copy script does not install it.
See `navigation-validation/README.md` in the main repository for coordinates,
measurements and known limits. Keep `sv_bot_draw_target` off in normal play;
turn it on when you want to see the route segments.
