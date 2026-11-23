Put ffmpeg.exe in this folder.

I couldn't ship it inside the project, so you need to grab it yourself:

1. Go to https://github.com/BtbN/FFmpeg-Builds/releases
   (or https://www.gyan.dev/ffmpeg/builds/)
2. Download the win64 "gpl" static build, NOT the "shared" one and NOT "lgpl"
   (lgpl has no x264, so recording won't work).
3. Unzip it, open the bin folder, copy ffmpeg.exe here.
4. Run build.bat / build.sh, it copies ffmpeg.exe next to MotionCapture.exe.

License: the GPL builds of FFmpeg are GPL. If you hand out the app together
with ffmpeg.exe, include FFmpeg's LICENSE file (it's in the zip you downloaded)
and say where people can get the FFmpeg source: https://ffmpeg.org
