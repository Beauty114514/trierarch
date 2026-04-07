# Third-party material in `pulse-android/`

The **patches** and **module `.c` files** under `patches/` and `modules/` are taken from the Termux distribution’s **pulseaudio** recipe and are meant to stay **in-tree** so nothing at build time pulls from GitHub.

- Upstream packaging: https://github.com/termux/termux-packages/tree/master/packages/pulseaudio  
- PulseAudio itself: https://www.freedesktop.org/wiki/Software/PulseAudio/ (GPL-2.0+)

When you update these files, either refresh them deliberately from that directory (and bump this note) or replace them with your own ports. Keep license notices compatible with your APK distribution.
