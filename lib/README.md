## How to make x64 version of zlibstat.lib

1. Download zlib source code from https://github.com/madler/zlib
2. Open zlibstat.sln in Visual Studio 2017
3. Change Configuration to Release x64
4. Open properties of zlibstat project, go to C/C++ -> Code Generation -> Runtime Library and change it to Multi-threaded (/MT)
5. Build zlibstat project
6. Copy zlibstat.lib from zlibstat\x64\Release to this-repo\lib\x64 folder