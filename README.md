## Nestopia

This is a fork of https://github.com/0ldsk00l/nestopia . The purpose of this fork is to provide x64 build for Windows.

You can download the latest build from [the Release page](https://github.com/qwinsi/nestopia/releases).

The original content of README.md is below. I modified only the "Win32 Build" section.

------

## About
This project is a fork of the original Nestopia source code, plus the 
Linux port. The purpose of the project is to make sure people who want
a standalone GUI for the Nestopia emulator have this option available.

Project Goals:
* Maintain a functional GUI release of the Nestopia emulator on major desktop platforms

Contributing/Issues:
* Contributions will be reviewed for anything related to the standalone GUI builds
* Issues related to core emulation will be closed. This project no longer maintains the core emulator. Please submit issues about core emulation upstream at https://gitlab.com/jgemu/nestopia
* When not using a tagged release, please understand that the code is volatile and nothing is set in stone.

The following platforms are supported:
* Linux, BSD, macOS, Windows

This project depends on the following libraries:
FLTK 1.3 (1.4 on macOS), SDL2, libarchive, libepoxy, libsamplerate, zlib

## Installing Dependencies
Install dependencies required for building on Debian-based Linux distributions:
```
apt-get install build-essential autoconf autoconf-archive automake autotools-dev libarchive-dev libepoxy-dev libfltk1.3-dev libsamplerate0-dev libsdl2-dev zlib1g-dev
```

## FLTK Build
To build using Autotools (optional arguments in square brackets):
```
autoreconf -vif
./configure [--enable-doc]
make
```
Optionally:
```
make install
```

## macOS Build
```
# Install dependencies
brew install autoconf automake autoconf-archive pkg-config libarchive libepoxy libsamplerate fltk sdl2

# Build
autoreconf -vif
export PKG_CONFIG_PATH=/usr/local/opt/libarchive/lib/pkgconfig/
./configure [--enable-doc]
make [install]
```

## Win32 Build
To build the win32 solution with Visual Studio 2022:
1. Ensure you have the DirectX 9 SDK - (June 2010) https://www.microsoft.com/en-US/download/details.aspx?id=6812
2. Manually zip NstDatabase.xml to the destination source/core/database/NstDatabase.zip
3. Place 7zxa.dll, kailleraclient.dll, and unrar.dll into dll/. Place releasenotes.txt into doc/. These files are required by the build process.
4. Open projects/nestopia.sln
5. Build in release mode
