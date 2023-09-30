These dll files are extracted from https://sourceforge.net/projects/nestopiaue/files/1.52/

They are all 32-bit x86 dll files. So technically, they should not work for x64 build. However, your may build and run the x64 binary successfully. This is very likely because the dll files are not actually used. The program are designed to only loads the dll files on demand.

TODO: Add 64-bit dll files in the future. And make the build process automatically choose 32-bit or 64-bit dll files.