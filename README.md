# Description
Dietchan is an imageboard written in C.

Features:

- small and fast
- no JS
- no caching, all page generation done on the fly.
- no bloat™
- 9000 lines of pure C
- single-threaded, event based
- stale design
- Web 1.0

Example installation: https://dietchan.org/

## Import Note

The database format may change in the future, so the software is not currently suitable for production use.

## Build-Dependencies

### Necessary:

- Linux
- GCC
- CMake
- git
- cvs

## Runtime-Dependencies

### Necessary:

- Linux
- file
- Imagemagick
- ffmpeg

### Recommended:

- pngquant

## Compile:

    cmake -DCMAKE_BUILD_TYPE=Release . && make

As simple as that!

Originally hosted at https://gitgud.io/zuse/dietchan
