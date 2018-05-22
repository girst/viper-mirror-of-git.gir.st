# VIIper - a Snake Clone for Unicode-compatible Terminals

TODO: find a better name

## Dependencies

 - A VTE based terminal (like GNOME Terminal and a whole bunch of others)
 - Google Noto's Color Emoji Font (Fedora: `google-noto-emoji-color-fonts.noarch`)

## Keybindings

hjkl or cursor keys move the snake. 
r to restart, p to pause, q to quit. 

## TODO

 - DONE base game: fixed field size, fixed speed
 - DONE food (unicode)
 - DONE put 'sprites' into `schemes.h`
 - DONE snake elongates
 - DONE unicode chars
 - DONE input buffer (so fast 180° turns get executed)
 - DONE only redraw changing parts of the screen
 - PoC: input out of whack when stopping (^Z) and resuming
 - timer, score, increasing speed
 - keybindings for restart, pause, redraw
 - on dying: show end screen, allow restarting
 - bonus/special items: slower snake, shorter snake, etc.
 - decaying points? (more points the faster you get the food)
 - wall-wrap-around mode?
 - fix all `grep -n TODO viiper.c`

## Notes

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
