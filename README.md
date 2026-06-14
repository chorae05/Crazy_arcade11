# Console BnB

Visual Studio 2022 / Windows console game inspired by Crazy Arcade BnB.

This is a Windows console-subsystem project that opens a separate Win32 game
window with `CreateWindow`. The black console remains available for internal
logs and errors, while the actual game is drawn in the dedicated 1000x800 window.
The game window uses GDI double buffering to reduce flicker.

## Build

Open `ConsoleBnB.sln` in Visual Studio 2022 and build the `ConsoleBnB` project.

## Controls

- Menu: mouse click, or `Up/Down` + `Enter`
- Player 1: `WASD`, `Shift` or `Ctrl`
- Player 2: Arrow keys, `Space`
- Needle escape: press `N` while trapped if you have a needle item.
- Exit: `ESC`

The game opens to a start screen. Choose `START` to begin the 3-2-1 countdown,
or `HOW TO PLAY` to read the detailed rules. After a match ends, `Enter` or
`Space` returns to the main menu; the next match starts only after selecting
`START` again.
