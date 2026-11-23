<p align="center">
  <img src="assets/logo.png" width="120" alt="Motion Capture logo">
</p>

<h1 align="center">Motion Capture</h1>

<p align="center">
  A simple, clean screen recorder for Windows.<br>
  Record your whole screen or just one window, saved as MP4.
</p>

---

Motion Capture is a small desktop app written in C. The window is plain Win32
(no frameworks, no runtime to install) and the recording itself is done by
[FFmpeg](https://ffmpeg.org). Open it, pick what to record, press Start.

## Features

- Record the **full screen** or a **single window**
- Pick which monitor to record if you have more than one
- Live preview, so you can see what you're about to record
- 15, 30 or 60 FPS
- Three quality levels: High, Balanced, Compact
- Show or hide the mouse cursor
- Choose where videos are saved
- Recordings list with Play and Open folder buttons
- Shows free disk space and the recording timer
- Dark UI, one small exe, no installer needed

## Quick start

1. Open `MotionCapture.exe`
2. Choose **Full screen** or **Window** at the top of the Dashboard
3. Press **Start**
4. Press **Stop** when you're done

Your video is saved to `C:\Users\<you>\Videos\MotionCapture` by default and
named like `motioncapture_20261123_143000.mp4`.

After you press Stop the status shows "Finalizing" for a second or two. That's
FFmpeg closing the file properly. Give it a moment before closing the app, or
the video can end up broken.

## Recording a single window

1. Click **Window** on the Dashboard
2. Pick the app from the list (use Refresh if it isn't there yet)
3. Press Start

Things to know:

- Keep the window open and not minimized while you record
- Don't resize it mid-recording
- Some GPU-heavy apps and games can come out black. Use Full screen for those
- Needs FFmpeg 5.0 or newer

## Settings

| Setting | Options |
| --- | --- |
| Display | Any connected monitor |
| Frame rate | 15, 30, 60 FPS |
| Quality | High, Balanced, Compact |
| Format | MP4 (H.264) |
| Mouse cursor | Visible / Hidden |
| Save directory | Anywhere you like |

Settings can't be changed while a recording is running.

There is no pause button. FFmpeg's screen grabber can't pause, and I'd rather
leave it out than fake it.

## Requirements

- Windows 10 or newer (64-bit)
- `ffmpeg.exe` next to `MotionCapture.exe`, or anywhere on your PATH.
  It has to be a build with x264, so grab the **GPL static** one, not LGPL
  and not "shared"

## Building from source

You need MinGW-w64. The easiest way to get it is [MSYS2](https://www.msys2.org):

```
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-binutils
```

Then:

1. Download a Windows build of FFmpeg (see `third_party/ffmpeg/README.txt`)
   and put `ffmpeg.exe` in `third_party/ffmpeg/`
2. In an MSYS2 UCRT64 shell, from the project folder, run:
   ```
   ./build.sh
   ```
   or run `build.bat` if gcc is already on your PATH
3. The finished app is in `dist/`

If you skip step 1 it still builds, you just have to copy `ffmpeg.exe` into
`dist/` yourself.

To compile by hand:

```
windres app.rc -O coff -o app_res.o
gcc src/motioncapture.c app_res.o -o MotionCapture.exe -O2 -municode -mwindows -static -lgdi32 -luser32 -lshell32 -lole32 -lshlwapi -ldwmapi
```

## Troubleshooting

**"ffmpeg.exe not found"**
It isn't next to the exe or on your PATH. Copy it into the same folder as
`MotionCapture.exe`.

**"FFmpeg exited unexpectedly"**
Usually the wrong FFmpeg build. Use the GPL one (it includes x264). Run
`ffmpeg -version` and check the output mentions `--enable-libx264`.

**The video is black**
Happens with some GPU-accelerated apps in Window mode. Switch to Full screen.

**The video won't play or is 0 KB**
The app was closed before the status went back to "Ready". Wait for
"Finalizing" to finish next time.

**Windows says "Windows protected your PC"**
The exe isn't code-signed, which is normal for small open source apps. Click
More info, then Run anyway.

## Project layout

```
src/motioncapture.c     the whole app
assets/                 logo (svg, png) and icon
app.rc                  embeds the icon in the exe
build.sh / build.bat    build scripts
third_party/ffmpeg/     put ffmpeg.exe here
```

## Ideas for later

- Pause / resume (record in segments and join them)
- Thumbnails and durations in the recordings list
- Audio recording
- Global start/stop hotkey
- Region selection

## FFmpeg and licensing

Motion Capture's own code is MIT licensed, see [LICENSE](LICENSE).

FFmpeg is a separate program and is not part of this repository. The GPL builds
are licensed under the GPL. If you share the app together with `ffmpeg.exe`,
include FFmpeg's license file and say where the source can be found
(https://ffmpeg.org).

## Author

Made by Shubham - [github.com/shubhamgtm](https://github.com/shubhamgtm)

If it's useful to you, a star on the repo is always nice.
