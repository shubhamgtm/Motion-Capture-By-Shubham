# Motion Capture

A small screen recorder for Windows, written in C. It uses the normal Win32 API
for the window and ffmpeg to do the actual recording.

![logo](assets/logo.png)

## What it does

- Records your whole screen, or just one window, to an MP4 (H.264)
- Pick the display if you have more than one monitor
- Window mode: click Window, pick an app from the list, hit Start
- 15, 30 or 60 FPS
- Three quality levels (High, Balanced, Compact)
- Cursor on or off
- Choose where recordings are saved (default is `Videos\MotionCapture`)
- Live preview, timer, free disk space
- List of your recordings with Play / Open folder buttons

No pause button. ffmpeg's screen grabber can't pause, and I didn't want to fake it.

## Building

You need MinGW-w64 (the easiest way is [MSYS2](https://www.msys2.org), then
`pacman -S mingw-w64-ucrt-x86_64-gcc`).

1. Get `ffmpeg.exe` and put it in `third_party/ffmpeg/` (read the txt file in there)
2. In an MSYS2 UCRT64 shell run `./build.sh`, or run `build.bat` if gcc is on your PATH
3. Your app is in `dist/`

If you skip step 1 it still builds, but you'll have to put ffmpeg.exe next to
`MotionCapture.exe` yourself.

## Using it

Open `MotionCapture.exe`, hit **Start**, hit **Stop** when you're done. The
status goes to "Finalizing" for a second while ffmpeg closes the file, so don't
kill the app right then.

## Recording a single window

Pick **Window** on the dashboard and choose the app from the list. A few things:

- Keep the window open and not minimized while recording
- Don't resize it mid-recording, ffmpeg won't like that
- Some GPU-heavy apps and games can come out black. Use full screen for those
- It needs ffmpeg 5.0 or newer (the build we recommend is fine)

## Problems

- **"ffmpeg.exe not found"**: it isn't next to the exe or on PATH.
- **"FFmpeg exited unexpectedly"**: usually the wrong ffmpeg build (needs x264,
  so use the gpl one) or an odd display setup.
- Windows SmartScreen may warn about the exe because it isn't signed. More info
  -> Run anyway.

## Project layout

```
src/motioncapture.c   the whole app
assets/               logo and icon
app.rc                puts the icon into the exe
build.bat / build.sh  build scripts
third_party/ffmpeg/   drop ffmpeg.exe here
```

## Author

Made by Shubham, https://github.com/shubhamgtm

## License

My code is MIT, see `LICENSE`. FFmpeg has its own license, see
`third_party/ffmpeg/README.txt`.
