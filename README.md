# Beschreibung
Dietchan ist eine in C geschriebene Imageboard-Software.

Features:

- klein, schnell
- kein JS
- kein Caching, alles wird on-the-fly generiert
- kein Bloat™
- 9000 Zeilen reines C
- single-threaded, asynchron
- altbackenes Design
- Web 1.0

Beispiel-Installation:
https://li691-139.members.linode.com/

## Wichtiger Hinweis

Das Datenbankformat könnte sich in Zukunft noch ändern, daher ist die Software momentan nicht für den Produktivbetrieb geeignet.

## Build-Abhängigkeiten

### Notwendig:

- Linux
- GCC
- CMake
- git
- cvs

## Laufzeit-Abhängigkeiten

### Notwendig:

- Linux
- file
- Imagemagick
- ffmpeg
- colord

### Empfohlen:

- pngquant

## Kompilieren

    cmake -DCMAKE_BUILD_TYPE=Release . && make

So ein Fach ist das!
