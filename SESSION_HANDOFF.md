# Session Handoff

## Current status

The project is a working **single-player** arcade traffic racer with a compiling but
**unverified** online session layer on top. Solo Traffic Sprint is complete and validated
in play: countdown, 6 km route, pooled traffic, near-pass scoring, collisions, finish,
results, restart, soft pause, menus, settings and local saves all work.

Engine is UE 5.8.0 at `E:\EPIC_GAMES-games\UE_5.8`; the project is `E:\Overlane`
(ASCII-only path is mandatory — MSVC cannot handle `Masaüstü` in PCH paths).

`OverlaneEditor Win64 Development` builds clean.

## Version control

The repository now has history. First commit `269c75e` on `main`, pushed to the private
remote `https://github.com/erenulutas0/overlane.git` with 247 assets in Git LFS (162 MB).
Before this the tree had **zero commits** despite `TASKS.md` P0-002 claiming otherwise.

Watch the GitHub free LFS quota: 1 GB storage and 1 GB/month bandwidth. Every `.umap` or
`.uasset` edit stores a full new copy, so level editing will consume it quickly.

## What was done this session

1. **Finished the interrupted practice-bot work.** The previous session wrote a
   `PracticeBot` actor tag and was cut off before writing any reader, leaving the bot
   actively corrupting the human's score. Replaced with a replicated `bIsAIRacer` flag and
   guarded all three scoring paths. Bot is now destroyed when a client joins.
2. **Fixed a blocking UI defect.** `AOverlaneHUD::DrawHUD` early-returned without a
   vehicle pawn, with the menu blocks below that guard — a player with no pawn saw a blank
   screen with no way in.
3. **Added the session layer (P5-002).** `UOverlaneSessionSubsystem` over the classic
   `IOnlineSubsystem`, using `OnlineSubsystemNull` for LAN discovery. Host creates a listen
   session and server-travels; join resolves the connect string and client-travels.
4. Main menu extended to SOLO YARIS / ONLINE - OYUN KUR / ONLINE - OYUN BUL / AYARLAR,
   plus a session browser and a host lobby.

## Exact next action

**Run the LAN host/join test.** Nothing in the session flow has been executed. PIE will not
exercise it properly — `OnlineSubsystemNull` discovers over LAN broadcast, so it needs two
standalone or packaged instances, ideally on two machines on the same network.

Steps: launch both, on machine A pick `ONLINE - OYUN KUR` and confirm the lobby appears
with `OYUNCULAR: 1 / 4`; on machine B pick `ONLINE - OYUN BUL` and confirm machine A is
listed with no address typed; join, confirm A shows `2 / 4`, press Enter on A, confirm both
count down and drive.

## Known blockers for real multiplayer

These are documented in `KNOWN_ISSUES.md` and must be addressed before online play is
actually enjoyable, not merely connectable:

- **NET-001**: remote clients do not simulate their own vehicle at all. Movement arrives as
  a hard teleport from `OnRep_OwnerServerTransform`, two transform channels replicate at
  once, and the input RPCs are unreliable and edge-triggered — a dropped release packet
  strands the throttle on. The handling integrator is also variable-dt Euler, so
  deterministic replay across machines is currently impossible. This needs a fixed-timestep
  prediction and reconciliation rework.
- **UI-002**: a joined client still has no menus at all; every menu is gated on
  `GetAuthGameMode`, which is nullptr on clients. This blocks the Steam lobby flow.
- Multiplayer traffic is six cars with lane changes disabled, every actor
  `bAlwaysRelevant`. Dense networked traffic is a redesign, not a tuning pass.
- Scoring is still global rather than per-racer.
