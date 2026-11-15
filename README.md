# IceTetris

A small, portable Tetris clone written in plain C89. It builds with
most C compilers, including older ones.

## Files

- `tetris.c`   - the whole game (single file)
- `Makefile`   - for gcc/clang and friends

## Build

Linux / macOS / BSD:

    make
    # or directly:
    cc -O2 -o tetris tetris.c

Windows:

    gcc -O2 -o tetris.exe tetris.c      (MinGW)
    cl /O2 tetris.c                      (MSVC)

DOS / legacy:

    tcc tetris.c                         (Turbo C / Borland)
    gcc -O2 -o tetris.exe tetris.c       (DJGPP)
    wcl386 tetris.c                      (Watcom)

## Controls

    a / left     move left
    d / right    move right
    w / up       rotate
    f / down     soft drop
    s / space    instant drop
    p            pause
    q            quit

## Platform notes

- Unix: uses `termios` + ANSI escapes, so any ANSI terminal works.
- Windows: uses the console API + `conio`.
- DOS: uses `conio` + `dos.h`; `delay()` is provided by Turbo C/Borland
  and Watcom.
