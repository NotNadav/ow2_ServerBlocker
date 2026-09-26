# OW2 Server Manager

A small Windows tool for manually blocking specific Overwatch 2 server regions using the Windows Firewall, so the matchmaker is forced to connect somewhere else.

Built in C with the Win32 API — no dependencies, no installer, just a single exe.

## Features

- Toggle switches for 5 regions: Middle East, Europe, US West, US East, Asia
- Shows current block status for each region on startup
- Optional: restrict the block to the Overwatch process only (`program=` in the firewall rule), so you don't accidentally block other apps that happen to share the same IP ranges
- IP lists pulled from community-maintained sources ([foryVERX/Overwatch-Server-Selector](https://github.com/foryVERX/Overwatch-Server-Selector), [Qiiks/ow2-server-list](https://github.com/Qiiks/ow2-server-list))

## How it works

The app runs `netsh advfirewall` commands to add/remove inbound and outbound block rules against known IP ranges per region. Since the lists can get long, rules are automatically split into chunks to stay under Windows' command-line length limits.

## Usage

1. Run the exe — it'll prompt for admin rights (needed to edit firewall rules).
2. Click **Select Overwatch.exe** and point it to your install (usually under `Battle.net/Overwatch/_retail_/Overwatch.exe`). This is optional but recommended — it scopes the block to just the game.
3. Flip the switch for whichever region(s) you want blocked.
4. Restart Overwatch for the change to take effect.

## Building

```
gcc main.c -o ow2_manager.exe -mwindows -lcomctl32 -ladvapi32 -lshell32 -lcomdlg32
```

Requires `main.h` (the IP list header) in the same folder.

## Notes / disclaimer

- A lot of these IP ranges are shared Google Cloud infrastructure, not Blizzard-exclusive ranges — that's why the process-scoping option exists. Without it you could theoretically affect other software using the same address space.
- This works by manipulating your own network routing, not the game client or servers. That said, forcing region selection like this may go against Overwatch's Terms of Service — use at your own risk.
- IP lists are community-sourced and may go stale. If a region stops responding to blocks, the ranges probably need updating.