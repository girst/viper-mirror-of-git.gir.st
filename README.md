# VIper - a Snake Clone for Unicode-compatible Terminals

terminal + emoji = snek    
🖳 + 💩 = 🐍

## Requirements

You'll need a terminal emulator with good Unicode (Emoji) support and compatible
fonts to fully enjoy the graphics of this game. This is what I'd recommend:

 - A VTE based terminal (like GNOME Terminal and a whole bunch of others)
 - Google Noto's Color Emoji Font (Fedora: `google-noto-emoji-color-fonts.noarch`)
 - DejaVu Sans Mono with Braille characters patched in (fetch from Ubuntu)

## Keybindings

`h`, `j`, `k`, `l` or cursor keys move the snake. 
`r` to restart, `p` to pause, `q` to quit. 

## TODO

<!--
 - DONE ~~base game: fixed field size, fixed speed~~
 - DONE ~~food (unicode)~~
 - DONE ~~put 'sprites' into `schemes.h`~~
 - DONE ~~snake elongates~~
 - DONE ~~unicode chars~~
 - DONE ~~input buffer (so fast 180° turns get executed)~~
 - DONE ~~only redraw changing parts of the screen~~
 - DONE ~~input out of whack when stopping (^Z) and resuming~~
 - DONE ~~keybindings for restart, pause, redraw~~
 - DONE ~~on dying: show end screen, allow restarting~~
 - DONE ~~score, increasing speed~~
 - DONE ~~bonus/special items: slower snake, shorter snake, etc.~~
 - DONE ~~wall-wrap-around mode?~~
 - NAH: predatory animals trying to eat the snake? (🐉, 🐊, 🐆, 🐅, 🦁, 🐗, 🦊, 🦅)
-->
 - TODO even more food items (to shape points distribution), boni
 - TODO decaying points? (more points the faster you get the food)
 - TODO fix all `grep -n 'TODO\|XXX\|\([^:]\|^\)//' *.[ch]`
 - TODO: find a better name

## Notes

### Why this program is using SIGALRM instead of multithreading

One of the main reasons for making this game was to try some unconventional
programming ideas I had. Seeing how far I can get with only a single thread was
one of them. Some others were pure lazyness, though.

### terminal compat

to display emojis, we need a terminal that can handle a color emoji font (no
shit, sherlock). mlterm, xterm and urxvt didn't work in my tests (mlterm might
work if compiled correctly, the other two use bitmap fonts and i don't think
there are any w/ emoji support). 
i intend to put bonus items in the game that will only be visible for a short
time. when they get near the end of their life, SGI-5 (blink) will make them
blink. this is supported in gnome-term 3.28 (vte 0.52) which is supplied with
fedora 28. [bug report](https://bugzilla.gnome.org/show_bug.cgi?id=579964)
for KDE's konsole you'll need [this
fontconfig](https://gist.github.com/IgnoredAmbience/7c99b6cf9a8b73c9312a71d1209d9bbb)
and follow the steps inside.

### strange behaviour of SIGALRM

I'm using SIGALRM to advance the snake's position. during some refactoring I
noticed that when the signal handler returns, a STX (ASCII 0x02) byte gets
pushed onto stdin.

### DejaVu Sans Mono: Braille Characters

This font (while otherwise beautiful) does not by default include glyphs for the
braille characters in Unicode. And gnome-terminal falls back to some very ugly
rendition. 

Ubuntu patches this font to include those glyphs, so you can just fetch it from
there, or patch the font yourself. For this, open
`/usr/share/fonts/dejavu/DejaVuSansMono.ttf` and `DejaVuSans.ttf` and copy the
braille section to the Mono variant. You can save the font under a different
name in the same directory and the fallback will then work correctly. 
