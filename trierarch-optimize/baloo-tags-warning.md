# “Could not enter folder tags:/” and related Baloo pop-ups in KDE

## Why it happens

1. **What Baloo is** — KDE’s **file indexer**. It walks the filesystem, keeps an SQLite database, and backs Dolphin, KRunner, the virtual **`tags:/`** location, etc.  
2. **Why you see `tags:/` / “could not enter folder”** — On real desktops, failures often tie to **Baloo DB state** and **KIO/`tags:/`** (there are long-standing KDE reports around Baloo DB and tag URLs).  
   Under **proot** (user namespaces, bind-heavy layouts, different **`/proc`**, **inotify** limits), Baloo is **more likely to mis-index or error**, and virtual **`tags:/`** access can fail repeatedly. On a phone + rootfs setup you usually **don’t need** full desktop-style indexing anyway.  
3. **Takeaway** — This is usually **KDE’s indexer + container/proot constraints**, not “Arch is broken” in isolation.

## What to do

Run inside the **proot** shell (Plasma **6** uses `balooctl6`; on Plasma **5** try the same subcommands with `balooctl`):

```bash
balooctl6 status
balooctl6 purge
balooctl6 suspend
balooctl6 disable
```

Check status, clear the index, pause, then turn the indexer off—pop-ups from this path usually stop.

> If only `balooctl` exists on your system, substitute it for `balooctl6`.

## Why this works

- **`purge`** removes a corrupted or unsuitable Baloo DB for this environment.  
- **`suspend` / `disable`** stop background indexing and ongoing access to **`tags:/`**, so proot-unfriendly code paths are no longer hit constantly.  
- For Trierarch, **disabling Baloo** is usually a good trade-off: little downside for running Plasma and apps, much less noise and CPU.

For Baloo in general, see [ArchWiki: Baloo](https://wiki.archlinux.org/title/Baloo) and [KDE UserBase: Baloo](https://userbase.kde.org/Baloo).
