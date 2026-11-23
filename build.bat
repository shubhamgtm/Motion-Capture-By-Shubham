@echo off
rem Needs gcc and windres on PATH (MinGW-w64 / MSYS2 UCRT64 bin folder).
if not exist dist mkdir dist
windres app.rc -O coff -o dist\app_res.o || goto fail
gcc src\motioncapture.c dist\app_res.o -o dist\MotionCapture.exe -O2 -municode -mwindows -static -lgdi32 -luser32 -lshell32 -lole32 -lshlwapi -ldwmapi || goto fail

if exist third_party\ffmpeg\ffmpeg.exe (
    copy /y third_party\ffmpeg\ffmpeg.exe dist\ >nul
    echo Done. dist\ has MotionCapture.exe and ffmpeg.exe
) else (
    echo Built dist\MotionCapture.exe
    echo No ffmpeg.exe in third_party\ffmpeg yet. Copy it there before recording.
)
goto :eof

:fail
echo Build failed.
exit /b 1
