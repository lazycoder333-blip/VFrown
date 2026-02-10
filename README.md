# V.Frown
 A modified version of the experimental emulator for the V.Smile

 This fork doesn't aim to improve emulation accuracy and only adds quality of life features. If a game doesn't boot on the original fork, it isn't going to boot here.

 ![VFrown Logo](images/icon.png)

**NOTE: this emulator is still a work in progress.**

## Building and Running
Building is best done on Linux or MacOS.

Run the following commands:
```
cd <path/to/project>
make platform=<platform> build=<build>
./VFrown <path/to/game>
```
Replace `<platform>` with `windows`, `macos` or `linux` depending on your platform,
and replace `<build>` with `debug` or `release`. Without these, it will default to building a linux debug build.

After running V.Frown for the first time, put your sysrom (`sysrom.bin`) in the sysrom folder. This is required for some games to boot.

## Controls
You have to configure the controls when you start this version of V.Frown for the first time.

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
