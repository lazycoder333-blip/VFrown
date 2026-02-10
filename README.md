# V.Frown With Cheats
 A modified version of the experimental emulator for the V.Smile

 This fork doesn't aim to improve emulation accuracy and only adds quality of life features. If a game doesn't boot on the original fork, it isn't going to boot here.

 ![VFrown Logo](images/icon.png)

**NOTE: this emulator is still a work in progress.**

## Building and Running
Building is easiest on MacOS or Linux.

Run the following commands:
```
cd <path/to/project>
make platform=<platform> build=<build>
./VFrown <path/to/game>
```
Replace `<platform>` with `windows`, `macos` or `linux` depending on your platform,
and replace `<build>` with `debug` or `release`. Without these, it will default to building a linux debug build.

After running V.Frown for the first time, put your sysrom (`sysrom.bin`) in the sysrom folder. This is required for some games to boot.

## Default Controls
```
Arrow keys: directional movement
Space: Enter/OK
A: Help
S: Exit
D: Learning Zone
Z: Red
X: Yellow
C: Blue
V: Green
```

## Examples
![VTech Logo](images/Logo1.png)
![VSmile Logo](images/Logo2.png)
![Alphabet Park Adventure](images/AlphabetPark1.png)
![Scooby-Doo! Funland Frenzy](images/ScoobyDoo1.png)

## TODO / Roadmap
Currently...
- Improve Sound (Top priority)

Later on...
- Add support for more controllers
- ...and more...
