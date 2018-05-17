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
 - food (unicode)
 - put 'sprites' into `schemes.h`
 - snake elongates
 - unicode chars
 - timer, score, increasing speed
 - special items: slower snake, shorter snake, etc.
 - input buffer (so fast 180° turns get executed)
 - wall-wrap-around mode?

## Notes

### terminal compat

to display emojis, we need a terminal that can handle a color emoji font (no
shit, sherlock). mlterm, xterm and urxvt didn't work in my tests (mlterm might
work if compiled correctly, the other two use bitmap fonts and i don't think
there are any w/ emoji support). konsole will have to be tested in the future.
i intend to put bonus items in the game that will only be visible for a short
time. when they get near the end of their life, SGI-5 (blink) will make them
blink. this is supported in gnome-term 3.28 (vte 0.52) which is supplied with
fedora 28. [bug report](https://bugzilla.gnome.org/show_bug.cgi?id=579964)

### drawing the snake

to draw the snake with box chars, we need to know which glyph to draw.

our current position is X, and our predecessor and successor are on one of
`[NESW]` of us (we need to compute the relpos from the absolute). we need to
draw a line between them. note that head and tail have `NULL` as pre-/successor.

```
   [N]
[W][X][E]
   [S]
```

6 combinations (+ reverse):
 - N-S -> `||`
 - W-E -> `==`
 - N-E -> `'=`
 - E-S -> `,=`
 - S-W -> `=,`
 - W-N -> `='`
