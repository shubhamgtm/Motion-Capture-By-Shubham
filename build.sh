#!/bin/sh
# Run this from an MSYS2 UCRT64 shell.
set -e
mkdir -p dist
windres app.rc -O coff -o dist/app_res.o
gcc src/motioncapture.c dist/app_res.o -o dist/MotionCapture.exe \
    -O2 -municode -mwindows -static \
    -lgdi32 -luser32 -lshell32 -lole32 -lshlwapi -ldwmapi

if [ -f third_party/ffmpeg/ffmpeg.exe ]; then
    cp third_party/ffmpeg/ffmpeg.exe dist/
    echo "Done. dist/ has MotionCapture.exe and ffmpeg.exe"
else
    echo "Built dist/MotionCapture.exe"
    echo "No ffmpeg.exe in third_party/ffmpeg/ yet, copy it there (or into dist/) before recording."
fi
