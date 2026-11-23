/*
 * Motion Capture
 * A small screen recorder for Windows. The window is plain Win32/GDI,
 * the actual recording is done by ffmpeg (gdigrab + x264).
 *
 * Build: see build.bat (or build.sh from an MSYS2 shell).
 * ffmpeg.exe has to sit next to the exe, or be somewhere on PATH.
 *
 * Full screen or a single window (gdigrab with hwnd=).
 * gdigrab can't pause, so there's no pause button. Maybe later with
 * segments + concat.
 */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>

/* colours */
#define C_BG     RGB(16,17,20)
#define C_SIDE   RGB(21,22,26)
#define C_CARD   RGB(28,30,35)
#define C_BORDER RGB(42,45,52)
#define C_TEXT   RGB(232,234,240)
#define C_MUTED  RGB(138,143,156)
#define C_ACCENT RGB(124,108,255)
#define C_RED    RGB(235,87,87)
#define C_GREEN  RGB(74,200,140)

#define GITHUB_URL L"https://github.com/shubhamgtm"
enum { P_DASH, P_REC, P_SET, P_ABOUT };
enum { ID_NAV = 1, ID_START = 10, ID_STOP, ID_DISPLAY = 20, ID_FPS, ID_QUAL, ID_CURSOR,
       ID_BROWSE, ID_OPENDIR, ID_PLAY = 100, ID_FOLDER = 200 };

typedef struct { RECT r; int id; } Btn;
typedef struct { WCHAR name[MAX_PATH]; WCHAR path[MAX_PATH]; ULONGLONG size; FILETIME ft; } Rec;

static HWND g_wnd;
static HFONT fN, fB, fH, fS, fT;
static Btn btns[128]; static int nb;
static POINT mouse;
static int page = P_DASH;

enum { ID_MODE_SCREEN = 30, ID_MODE_WIN, ID_PICK, ID_PICKCLOSE, ID_PICKREF, ID_GITHUB, ID_WIN = 300 };

/* what we record: 0 = a whole display, 1 = one window */
static int mode, showPicker;
static HWND selWnd;
static WCHAR selTitle[128];
typedef struct { HWND h; WCHAR title[128]; } WinItem;
static WinItem wins[24]; static int nwin;

/* settings */
static int fpsList[3] = {15, 30, 60}, fpsIdx = 1;
static const WCHAR *qName[3] = {L"High", L"Balanced", L"Compact"};
static int qCrf[3] = {18, 23, 28}, qIdx = 1;
static int showCursor = 1;
static WCHAR saveDir[MAX_PATH];
static RECT mons[8]; static int nmon, monIdx;

/* recording state */
static int recording, finalizing;
static PROCESS_INFORMATION pi;
static HANDLE hStdin;
static ULONGLONG t0;
static WCHAR curFile[MAX_PATH], errMsg[256];
static Rec recs[64]; static int nrec;

/* helpers */
static BOOL CALLBACK MonCb(HMONITOR m, HDC d, LPRECT r, LPARAM l) {
    if (nmon < 8) mons[nmon++] = *r; return TRUE;
}
static BOOL CALLBACK WinCb(HWND h, LPARAM l) {
    WCHAR t[128]; RECT r; int cloaked = 0;
    if (nwin >= 24 || h == g_wnd || !IsWindowVisible(h) || IsIconic(h)) return TRUE;
    if (GetWindowLongW(h, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
    DwmGetWindowAttribute(h, 14 /* DWMWA_CLOAKED */, &cloaked, sizeof cloaked);
    if (cloaked) return TRUE;
    if (!GetWindowTextW(h, t, 128) || !wcscmp(t, L"Program Manager")) return TRUE;
    GetWindowRect(h, &r);
    if (r.right - r.left < 120 || r.bottom - r.top < 80) return TRUE;
    wins[nwin].h = h; wcscpy_s(wins[nwin].title, 128, t); nwin++;
    return TRUE;
}
static void RefreshWins(void) { nwin = 0; EnumWindows(WinCb, 0); }
static void GetSrcSize(int *w, int *h) {
    RECT r; *w = *h = 0;
    if (mode == 1) { if (selWnd && IsWindow(selWnd) && GetWindowRect(selWnd, &r)) { *w = r.right - r.left; *h = r.bottom - r.top; } }
    else { *w = mons[monIdx].right - mons[monIdx].left; *h = mons[monIdx].bottom - mons[monIdx].top; }
}
static void RR(HDC dc, RECT r, int rad, COLORREF fill, COLORREF border) {
    HBRUSH b = CreateSolidBrush(fill); HPEN p = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b); DeleteObject(p);
}
static void T(HDC dc, const WCHAR *s, RECT r, COLORREF c, HFONT f, UINT fl) {
    SelectObject(dc, f); SetTextColor(dc, c); SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, s, -1, &r, fl | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
}
static void Icon(HDC dc, int k, int cx, int cy, COLORREF c) {
    HPEN p = CreatePen(PS_SOLID, 2, c); HBRUSH b = CreateSolidBrush(c);
    HGDIOBJ op = SelectObject(dc, p), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    switch (k) {
    case 0: for (int i = 0; i < 4; i++) { int x = cx - 8 + (i % 2) * 9, y = cy - 8 + (i / 2) * 9;
                RoundRect(dc, x, y, x + 7, y + 7, 2, 2); } break;
    case 1: for (int i = 0; i < 3; i++) { MoveToEx(dc, cx - 8, cy - 6 + i * 6, 0); LineTo(dc, cx + 8, cy - 6 + i * 6); } break;
    case 2: Ellipse(dc, cx - 8, cy - 8, cx + 8, cy + 8); Ellipse(dc, cx - 3, cy - 3, cx + 3, cy + 3); break;
    case 3: Ellipse(dc, cx - 8, cy - 8, cx + 8, cy + 8); MoveToEx(dc, cx, cy - 1, 0); LineTo(dc, cx, cy + 4);
            MoveToEx(dc, cx, cy - 4, 0); LineTo(dc, cx, cy - 3); break;
    case 4: SelectObject(dc, b); Ellipse(dc, cx - 6, cy - 6, cx + 6, cy + 6); break;
    case 5: SelectObject(dc, b); Rectangle(dc, cx - 5, cy - 5, cx + 5, cy + 5); break;
    case 6: { POINT t[3] = {{cx - 4, cy - 7}, {cx - 4, cy + 7}, {cx + 7, cy}}; SelectObject(dc, b); Polygon(dc, t, 3); } break;
    case 7: Rectangle(dc, cx - 8, cy - 5, cx + 8, cy + 6); Rectangle(dc, cx - 8, cy - 8, cx - 1, cy - 5); break;
    }
    SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(p); DeleteObject(b);
}
/* style: 0 normal, 1 primary, 2 danger, 3 disabled */
static void Button(HDC dc, RECT r, const WCHAR *s, int id, int style, int icon) {
    int hov = PtInRect(&r, mouse) && style != 3;
    COLORREF fill = C_CARD, bord = C_BORDER, fg = C_TEXT;
    if (style == 1) { fill = hov ? RGB(142,128,255) : C_ACCENT; bord = fill; fg = RGB(255,255,255); }
    else if (style == 2) { fill = hov ? RGB(245,105,105) : C_RED; bord = fill; fg = RGB(255,255,255); }
    else if (style == 3) { fill = RGB(24,25,29); fg = RGB(88,92,102); }
    else if (hov) fill = RGB(38,41,48);
    RR(dc, r, 10, fill, bord);
    int cx = r.left + (r.right - r.left) / 2, cy = (r.top + r.bottom) / 2;
    if (icon >= 0) {
        int w = (r.right - r.left);
        Icon(dc, icon, r.left + (s[0] ? w / 2 - 38 : w / 2), cy, fg);
        RECT tr = r; tr.left += 24; if (s[0]) T(dc, s, tr, fg, fB, DT_CENTER);
    } else T(dc, s, r, fg, fB, DT_CENTER);
    if (style != 3 && nb < 128) { btns[nb].r = r; btns[nb++].id = id; }
}
static void Card(HDC dc, RECT r) { RR(dc, r, 14, C_CARD, C_BORDER); }

static void FmtDur(WCHAR *o, ULONGLONG ms) {
    ULONGLONG s = ms / 1000; swprintf(o, 16, L"%02u:%02u:%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
}
static void FmtSize(WCHAR *o, ULONGLONG b) {
    if (b >= (1ULL << 30)) swprintf(o, 24, L"%.2f GB", b / 1073741824.0);
    else swprintf(o, 24, L"%.1f MB", b / 1048576.0);
}
static int CmpRec(const void *a, const void *b) {
    return CompareFileTime(&((Rec *)b)->ft, &((Rec *)a)->ft);
}
static void Scan(void) {
    WCHAR pat[MAX_PATH]; WIN32_FIND_DATAW fd; nrec = 0;
    swprintf(pat, MAX_PATH, L"%s\\motioncapture_*.mp4", saveDir);
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (nrec >= 64) break;
        Rec *r = &recs[nrec++];
        wcscpy_s(r->name, MAX_PATH, fd.cFileName);
        swprintf(r->path, MAX_PATH, L"%s\\%s", saveDir, fd.cFileName);
        r->size = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow; r->ft = fd.ftLastWriteTime;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    qsort(recs, nrec, sizeof(Rec), CmpRec);
}

/* recording */
static int FfmpegFound(void) { WCHAR b[MAX_PATH]; return SearchPathW(NULL, L"ffmpeg.exe", NULL, MAX_PATH, b, NULL) > 0; }

static void StartRec(void) {
    errMsg[0] = 0;
    if (!FfmpegFound()) { wcscpy_s(errMsg, 256, L"ffmpeg.exe not found. Place it next to motioncapture.exe or add it to PATH."); return; }
    CreateDirectoryW(saveDir, NULL);
    SYSTEMTIME st; GetLocalTime(&st);
    swprintf(curFile, MAX_PATH, L"%s\\motioncapture_%04d%02d%02d_%02d%02d%02d.mp4", saveDir,
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    WCHAR src[64], extra[96] = L"", vf[64] = L"";
    if (mode == 1) {
        if (!selWnd) { wcscpy_s(errMsg, 256, L"Choose a window first."); return; }
        if (!IsWindow(selWnd)) { wcscpy_s(errMsg, 256, L"That window was closed. Pick another one."); return; }
        if (IsIconic(selWnd)) { wcscpy_s(errMsg, 256, L"That window is minimized. Restore it first."); return; }
        swprintf(src, 64, L"hwnd=%lu", (unsigned long)(ULONG_PTR)selWnd);
        wcscpy_s(vf, 64, L"-vf scale=trunc(iw/2)*2:trunc(ih/2)*2 ");   /* x264 wants even sizes */
    } else {
        RECT m = mons[monIdx];
        swprintf(extra, 96, L"-offset_x %d -offset_y %d -video_size %dx%d ", m.left, m.top,
                 (m.right - m.left) & ~1, (m.bottom - m.top) & ~1);
        wcscpy_s(src, 64, L"desktop");
    }
    WCHAR cmd[2048];
    swprintf(cmd, 2048, L"ffmpeg.exe -y -hide_banner -loglevel error -f gdigrab -framerate %d -draw_mouse %d "
             L"%s-i %s %s-c:v libx264 -preset veryfast -crf %d -pix_fmt yuv420p -movflags +faststart \"%s\"",
             fpsList[fpsIdx], showCursor, extra, src, vf, qCrf[qIdx], curFile);

    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE rd, wr, nul;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return;
    SetHandleInformation(wr, HANDLE_FLAG_INHERIT, 0);          /* we keep the write end */
    nul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    STARTUPINFOW si = { sizeof si }; si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; si.hStdInput = rd; si.hStdOutput = nul; si.hStdError = nul;
    BOOL ok = CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(rd); CloseHandle(nul);
    if (!ok) { CloseHandle(wr); wcscpy_s(errMsg, 256, L"Failed to launch ffmpeg."); return; }
    hStdin = wr; t0 = GetTickCount64(); recording = 1;
}
/* Sending "q" makes ffmpeg finish the file properly (otherwise the mp4 can be broken).
   Done in a thread so the window doesn't freeze while it waits. */
static DWORD WINAPI StopThread(LPVOID p) {
    DWORD n;
    if (hStdin) { WriteFile(hStdin, "q\n", 2, &n, NULL); CloseHandle(hStdin); hStdin = NULL; }
    if (WaitForSingleObject(pi.hProcess, 15000) == WAIT_TIMEOUT) TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    PostMessageW(g_wnd, WM_APP, 0, 0);
    return 0;
}
static void StopRec(void) {
    if (!recording || finalizing) return;
    finalizing = 1; CreateThread(NULL, 0, StopThread, NULL, 0, NULL);
}

/* drawing */
static void RecRow(HDC dc, int x, int y, int w, int i) {
    RECT r = { x, y, x + w, y + 56 }; Card(dc, r);
    RECT th = { x + 10, y + 8, x + 74, y + 48 }; RR(dc, th, 8, RGB(20,21,25), C_BORDER);
    Icon(dc, 6, (th.left + th.right) / 2, (th.top + th.bottom) / 2, C_MUTED);
    RECT n = { x + 88, y + 8, x + w - 230, y + 30 }; T(dc, recs[i].name, n, C_TEXT, fB, DT_LEFT);
    WCHAR sz[24], meta[96]; SYSTEMTIME st, lt; FileTimeToSystemTime(&recs[i].ft, &st);
    SystemTimeToTzSpecificLocalTime(NULL, &st, &lt); FmtSize(sz, recs[i].size);
    swprintf(meta, 96, L"%s   \u2022   %04d-%02d-%02d %02d:%02d", sz, lt.wYear, lt.wMonth, lt.wDay, lt.wHour, lt.wMinute);
    RECT m = { x + 88, y + 29, x + w - 230, y + 50 }; T(dc, meta, m, C_MUTED, fS, DT_LEFT);
    RECT b1 = { x + w - 214, y + 12, x + w - 114, y + 44 }, b2 = { x + w - 104, y + 12, x + w - 12, y + 44 };
    Button(dc, b1, L"Play", ID_PLAY + i, 1, 6); Button(dc, b2, L"Folder", ID_FOLDER + i, 0, 7);
}
static void Header(HDC dc, int x, int w, const WCHAR *title, const WCHAR *sub) {
    RECT t = { x, 22, x + w, 52 }; T(dc, title, t, C_TEXT, fH, DT_LEFT);
    RECT s = { x, 50, x + w, 70 }; T(dc, sub, s, C_MUTED, fS, DT_LEFT);
}
static void Info(HDC dc, int x, int y, int w, const WCHAR *k, const WCHAR *v) {
    RECT a = { x, y, x + 100, y + 22 }, b = { x + 100, y, x + w, y + 22 };
    T(dc, k, a, C_MUTED, fS, DT_LEFT); T(dc, v, b, C_TEXT, fS, DT_RIGHT);
}
static void Dashboard(HDC dc, int x0, int cw, int H) {
    Header(dc, x0, cw, L"Dashboard", L"Pick what to record, then press Start");
    /* status pill */
    RECT pill = { x0 + cw - 150, 26, x0 + cw, 56 };
    const WCHAR *st = finalizing ? L"Finalizing" : recording ? L"Recording" : L"Ready";
    COLORREF sc = finalizing ? C_ACCENT : recording ? C_RED : C_GREEN;
    RR(dc, pill, 30, C_CARD, C_BORDER);
    HBRUSH b = CreateSolidBrush(sc); HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, pill.left + 14, pill.top + 10, pill.left + 24, pill.top + 20);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b);
    RECT pt = { pill.left + 32, pill.top, pill.right, pill.bottom }; T(dc, st, pt, C_TEXT, fB, DT_LEFT);

    int lw = cw - 296, top = 90;
    /* source bar: full screen / window */
    RECT s1 = { x0, top, x0 + 124, top + 38 }, s2 = { x0 + 132, top, x0 + 256, top + 38 };
    Button(dc, s1, L"Full screen", ID_MODE_SCREEN, mode == 0 ? 1 : 0, -1);
    Button(dc, s2, L"Window", ID_MODE_WIN, mode == 1 ? 1 : 0, -1);
    RECT s3 = { x0 + 268, top, x0 + lw, top + 38 };
    WCHAR lab[160];
    if (mode == 0) { swprintf(lab, 160, L"Display %d  (%d\u00D7%d)  \u2022  change", monIdx + 1,
                     mons[monIdx].right - mons[monIdx].left, mons[monIdx].bottom - mons[monIdx].top);
                     Button(dc, s3, nmon > 1 ? lab : L"Primary display", ID_DISPLAY, 0, -1); }
    else { swprintf(lab, 160, L"%s", selWnd ? selTitle : L"Choose a window\u2026");
           Button(dc, s3, lab, ID_PICK, 0, -1); }

    int ctop = top + 52, ph = lw * 9 / 16; if (ph > H - ctop - 250) ph = H - ctop - 250;
    RECT card = { x0, ctop, x0 + lw, ctop + ph + 84 }; Card(dc, card);
    RECT pv = { x0 + 12, ctop + 12, x0 + lw - 12, ctop + ph };
    RR(dc, pv, 10, RGB(10,11,13), C_BORDER);

    /* live preview */
    int mw, mh; GetSrcSize(&mw, &mh);
    int live = mw > 0 && mh > 0 && !(mode == 1 && IsIconic(selWnd));
    if (live) {
        int aw = pv.right - pv.left - 4, ah = pv.bottom - pv.top - 4;
        int dw = aw, dh = dw * mh / mw; if (dh > ah) { dh = ah; dw = dh * mw / mh; }
        int dx = pv.left + 2 + (aw - dw) / 2, dy = pv.top + 2 + (ah - dh) / 2;
        SetStretchBltMode(dc, HALFTONE); SetBrushOrgEx(dc, 0, 0, NULL);
        if (mode == 0) {
            HDC scr = GetDC(NULL);
            StretchBlt(dc, dx, dy, dw, dh, scr, mons[monIdx].left, mons[monIdx].top, mw, mh, SRCCOPY);
            ReleaseDC(NULL, scr);
        } else {
            HDC tmp = CreateCompatibleDC(dc); HBITMAP bm = CreateCompatibleBitmap(dc, mw, mh);
            HGDIOBJ old = SelectObject(tmp, bm);
            PrintWindow(selWnd, tmp, 2);
            StretchBlt(dc, dx, dy, dw, dh, tmp, 0, 0, mw, mh, SRCCOPY);
            SelectObject(tmp, old); DeleteObject(bm); DeleteDC(tmp);
        }
    } else {
        const WCHAR *msg = mode == 1 && selWnd && IsWindow(selWnd) ? L"Window is minimized" :
                           mode == 1 ? L"Choose a window to record" : L"No display found";
        T(dc, msg, pv, C_MUTED, fN, DT_CENTER);
    }
    if (errMsg[0]) {
        RECT er = { pv.left + 8, pv.bottom - 40, pv.right - 8, pv.bottom - 8 };
        RR(dc, er, 8, RGB(40,20,22), RGB(90,40,44)); T(dc, errMsg, er, C_RED, fS, DT_CENTER);
    }

    /* timer + buttons */
    int by = ctop + ph + 4;
    WCHAR tm[16]; FmtDur(tm, recording ? GetTickCount64() - t0 : 0);
    RECT tr = { x0 + 20, by, x0 + 190, by + 56 }; T(dc, tm, tr, recording ? C_TEXT : C_MUTED, fT, DT_LEFT);
    RECT bs = { x0 + lw - 300, by + 8, x0 + lw - 160, by + 48 }, be = { x0 + lw - 150, by + 8, x0 + lw - 16, by + 48 };
    Button(dc, bs, L"Start", ID_START, (recording || finalizing) ? 3 : 1, 4);
    Button(dc, be, L"Stop", ID_STOP, (recording && !finalizing) ? 2 : 3, 5);

    /* info card */
    RECT ic = { x0 + lw + 16, ctop, x0 + cw, card.bottom }; Card(dc, ic);
    int ix = ic.left + 18, iw = ic.right - ic.left - 36, iy = ic.top + 16;
    RECT ih = { ix, iy, ix + iw, iy + 22 }; T(dc, L"Recording info", ih, C_TEXT, fB, DT_LEFT); iy += 34;
    WCHAR v[MAX_PATH]; ULARGE_INTEGER fr = {0}, tot, tf;
    Info(dc, ix, iy, iw, L"Source", mode == 1 ? (selWnd ? selTitle : L"-") : L"Full screen"); iy += 28;
    swprintf(v, 64, L"%d \u00D7 %d", mw & ~1, mh & ~1); Info(dc, ix, iy, iw, L"Resolution", v); iy += 28;
    swprintf(v, 32, L"%d FPS", fpsList[fpsIdx]); Info(dc, ix, iy, iw, L"Frame rate", v); iy += 28;
    Info(dc, ix, iy, iw, L"Format", L"MP4 (H.264)"); iy += 28;
    Info(dc, ix, iy, iw, L"Quality", qName[qIdx]); iy += 28;
    GetDiskFreeSpaceExW(saveDir, &fr, &tot, &tf); swprintf(v, 32, L"%.1f GB free", fr.QuadPart / 1073741824.0);
    Info(dc, ix, iy, iw, L"Disk space", v); iy += 28;
    RECT sd = { ix, iy, ix + iw, iy + 22 }; T(dc, L"Saving to", sd, C_MUTED, fS, DT_LEFT);
    RECT sp = { ix, iy + 22, ix + iw, iy + 44 }; T(dc, saveDir, sp, C_TEXT, fS, DT_LEFT | DT_PATH_ELLIPSIS);

    /* recent */
    int ry = card.bottom + 18;
    RECT rh = { x0, ry, x0 + cw, ry + 24 }; T(dc, L"Recent recordings", rh, C_TEXT, fB, DT_LEFT);
    if (!nrec) { RECT e = { x0, ry + 30, x0 + cw, ry + 54 }; T(dc, L"Nothing yet. Your recordings will show up here.", e, C_MUTED, fS, DT_LEFT); }
    for (int i = 0; i < nrec && i < 3 && ry + 32 + (i + 1) * 62 <= H; i++) RecRow(dc, x0, ry + 32 + i * 62, cw, i);
}
static void Recordings(HDC dc, int x0, int cw, int H) {
    Header(dc, x0, cw, L"Recordings", L"Files saved in your recording folder");
    RECT ob = { x0 + cw - 170, 26, x0 + cw, 60 }; Button(dc, ob, L"Open folder", ID_OPENDIR, 0, 7);
    if (!nrec) { RECT e = { x0, 90, x0 + cw, 114 }; T(dc, L"No recordings yet.", e, C_MUTED, fN, DT_LEFT); }
    for (int i = 0; i < nrec && 84 + (i + 1) * 62 <= H; i++) RecRow(dc, x0, 84 + i * 62, cw, i);
}
static void SetRow(HDC dc, int x0, int cw, int y, const WCHAR *label, const WCHAR *hint, const WCHAR *val, int id) {
    RECT r = { x0, y, x0 + cw, y + 64 }; Card(dc, r);
    RECT l = { x0 + 20, y + 10, x0 + cw - 260, y + 32 }; T(dc, label, l, C_TEXT, fB, DT_LEFT);
    RECT h = { x0 + 20, y + 32, x0 + cw - 260, y + 54 }; T(dc, hint, h, C_MUTED, fS, DT_LEFT);
    RECT b = { x0 + cw - 240, y + 14, x0 + cw - 16, y + 50 };
    Button(dc, b, val, id, id ? 0 : 3, -1);
}
static void Settings(HDC dc, int x0, int cw, int H) {
    Header(dc, x0, cw, L"Settings", L"Click a value to change it");
    WCHAR v[MAX_PATH]; int y = 90;
    swprintf(v, 64, L"Display %d  (%d\u00D7%d)", monIdx + 1, mons[monIdx].right - mons[monIdx].left, mons[monIdx].bottom - mons[monIdx].top);
    SetRow(dc, x0, cw, y, L"Display", L"Full-screen capture of the selected monitor", v, ID_DISPLAY); y += 74;
    swprintf(v, 32, L"%d FPS", fpsList[fpsIdx]);
    SetRow(dc, x0, cw, y, L"Frame rate", L"15, 30 or 60 frames per second", v, ID_FPS); y += 74;
    SetRow(dc, x0, cw, y, L"Output quality", L"Higher quality produces larger files", qName[qIdx], ID_QUAL); y += 74;
    SetRow(dc, x0, cw, y, L"Output format", L"H.264 video in an MP4 container", L"MP4", 0); y += 74;
    SetRow(dc, x0, cw, y, L"Mouse cursor", L"Include the cursor in the recording", showCursor ? L"Visible" : L"Hidden", ID_CURSOR); y += 74;
    SetRow(dc, x0, cw, y, L"Save directory", saveDir, L"Browse\u2026", ID_BROWSE);
}
static void About(HDC dc, int x0, int cw, int H) {
    Header(dc, x0, cw, L"About", L"Motion Capture 1.0");
    RECT c = { x0, 90, x0 + cw, 330 }; Card(dc, c);
    const WCHAR *ln[] = { L"A simple screen recorder for Windows.",
        L"Record your whole screen or just one window, saved as MP4.",
        L"Written in C. Recording is done by FFmpeg.",
        L"Free and open source. Source code and updates:" };
    for (int i = 0; i < 4; i++) { RECT r = { x0 + 24, 106 + i * 32, x0 + cw - 24, 134 + i * 32 }; T(dc, ln[i], r, i ? C_MUTED : C_TEXT, i ? fN : fB, DT_LEFT); }
    RECT gb = { x0 + 24, 250, x0 + 344, 290 }; Button(dc, gb, L"github.com/shubhamgtm", ID_GITHUB, 1, -1);
}
static void Picker(HDC dc, int W, int H) {
    nb = 0;                                   /* only the picker is clickable while it's open */
    int shown = nwin > 8 ? 8 : nwin, pw = 580, ph = 118 + (shown ? shown : 1) * 46 + 56;
    RECT r = { (W - pw) / 2, (H - ph) / 2, (W + pw) / 2, (H + ph) / 2 };
    RR(dc, r, 16, RGB(24,26,31), C_ACCENT);
    RECT t = { r.left + 24, r.top + 18, r.right - 24, r.top + 44 }; T(dc, L"Choose a window", t, C_TEXT, fH, DT_LEFT);
    RECT st = { r.left + 24, r.top + 52, r.right - 24, r.top + 74 };
    T(dc, L"Keep it open and not minimized while recording.", st, C_MUTED, fS, DT_LEFT);
    int y = r.top + 90;
    if (!shown) { RECT e = { r.left + 24, y, r.right - 24, y + 40 }; T(dc, L"No windows found. Open the app you want to record.", e, C_MUTED, fN, DT_LEFT); }
    for (int i = 0; i < shown; i++, y += 46) {
        RECT row = { r.left + 16, y, r.right - 16, y + 40 };
        RR(dc, row, 8, PtInRect(&row, mouse) ? RGB(40,43,52) : C_CARD, C_BORDER);
        RECT tx = { row.left + 14, row.top, row.right - 10, row.bottom }; T(dc, wins[i].title, tx, C_TEXT, fN, DT_LEFT);
        btns[nb].r = row; btns[nb++].id = ID_WIN + i;
    }
    RECT rf = { r.right - 232, r.bottom - 56, r.right - 124, r.bottom - 16 }, cl = { r.right - 112, r.bottom - 56, r.right - 16, r.bottom - 16 };
    Button(dc, rf, L"Refresh", ID_PICKREF, 0, -1); Button(dc, cl, L"Cancel", ID_PICKCLOSE, 0, -1);
}
/* logo: purple tile, four corner brackets, dot in the middle */
static void Logo(HDC dc, int x, int y, int sz) {
    RECT tile = { x, y, x + sz, y + sz }; RR(dc, tile, sz / 4, C_ACCENT, C_ACCENT);
    int pad = sz / 5, L = sz / 5, cx = x + sz / 2, cy = y + sz / 2;
    HPEN p = CreatePen(PS_SOLID, 2, RGB(255,255,255)); HGDIOBJ op = SelectObject(dc, p);
    int l = x + pad, t = y + pad, r = x + sz - pad, b = y + sz - pad;
    MoveToEx(dc, l, t + L, 0); LineTo(dc, l, t); LineTo(dc, l + L, t);
    MoveToEx(dc, r - L, t, 0); LineTo(dc, r, t); LineTo(dc, r, t + L);
    MoveToEx(dc, r, b - L, 0); LineTo(dc, r, b); LineTo(dc, r - L, b);
    MoveToEx(dc, l + L, b, 0); LineTo(dc, l, b); LineTo(dc, l, b - L);
    SelectObject(dc, op); DeleteObject(p);
    HBRUSH br = CreateSolidBrush(RGB(255,255,255)); HGDIOBJ ob = SelectObject(dc, br), np = SelectObject(dc, GetStockObject(NULL_PEN));
    int d = sz / 7; Ellipse(dc, cx - d, cy - d, cx + d, cy + d);
    SelectObject(dc, ob); SelectObject(dc, np); DeleteObject(br);
}
static void Paint(HDC dc, int W, int H) {
    nb = 0;
    RECT all = { 0, 0, W, H }; HBRUSH bg = CreateSolidBrush(C_BG); FillRect(dc, &all, bg); DeleteObject(bg);
    RECT sb = { 0, 0, 220, H }; HBRUSH sbr = CreateSolidBrush(C_SIDE); FillRect(dc, &sb, sbr); DeleteObject(sbr);
    HPEN lp = CreatePen(PS_SOLID, 1, C_BORDER); HGDIOBJ op = SelectObject(dc, lp);
    MoveToEx(dc, 220, 0, 0); LineTo(dc, 220, H); SelectObject(dc, op); DeleteObject(lp);
    /* brand */
    Logo(dc, 20, 22, 36);
    RECT bt = { 66, 22, 214, 42 }; T(dc, L"Motion Capture", bt, C_TEXT, fB, DT_LEFT);
    RECT bs = { 66, 40, 214, 58 }; T(dc, L"Screen Recorder", bs, C_MUTED, fS, DT_LEFT);
    const WCHAR *nav[] = { L"Dashboard", L"Recordings", L"Settings", L"About" };
    for (int i = 0; i < 4; i++) {
        RECT r = { 14, 92 + i * 48, 206, 132 + i * 48 };
        int hov = PtInRect(&r, mouse);
        if (i == page) RR(dc, r, 10, RGB(38,34,66), RGB(38,34,66));
        else if (hov) RR(dc, r, 10, RGB(30,32,38), RGB(30,32,38));
        COLORREF c = i == page ? C_ACCENT : C_MUTED;
        Icon(dc, i, r.left + 24, (r.top + r.bottom) / 2, c);
        RECT tr = { r.left + 48, r.top, r.right, r.bottom }; T(dc, nav[i], tr, i == page ? C_TEXT : C_MUTED, fB, DT_LEFT);
        btns[nb].r = r; btns[nb++].id = ID_NAV + i;
    }
    {   /* github link at the bottom of the sidebar */
        RECT g = { 14, H - 52, 206, H - 16 }; int hov = PtInRect(&g, mouse);
        RR(dc, g, 10, hov ? RGB(30,32,38) : C_SIDE, hov ? C_BORDER : C_SIDE);
        RECT gt = { g.left + 12, g.top, g.right - 6, g.bottom };
        T(dc, L"github.com/shubhamgtm", gt, hov ? C_ACCENT : C_MUTED, fS, DT_LEFT);
        btns[nb].r = g; btns[nb++].id = ID_GITHUB;
    }
    int x0 = 244, cw = W - x0 - 24;
    switch (page) { case P_DASH: Dashboard(dc, x0, cw, H); break; case P_REC: Recordings(dc, x0, cw, H); break;
                    case P_SET: Settings(dc, x0, cw, H); break; default: About(dc, x0, cw, H); }
    if (showPicker) Picker(dc, W, H);
}

/* button actions */
static void Browse(void) {
    BROWSEINFOW bi = { g_wnd, NULL, NULL, L"Choose where recordings are saved", BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS };
    LPITEMIDLIST id = SHBrowseForFolderW(&bi);
    if (id) { SHGetPathFromIDListW(id, saveDir); CoTaskMemFree(id); Scan(); }
}
static void OpenFolder(const WCHAR *file) {
    WCHAR a[MAX_PATH + 32];
    if (file) { swprintf(a, MAX_PATH + 32, L"/select,\"%s\"", file); ShellExecuteW(NULL, L"open", L"explorer.exe", a, NULL, SW_SHOW); }
    else ShellExecuteW(NULL, L"open", saveDir, NULL, NULL, SW_SHOW);
}
static void Click(int id) {
    if (id >= ID_NAV && id < ID_NAV + 4) { page = id - ID_NAV; if (page != P_DASH) Scan(); return; }
    if (id >= ID_PLAY && id < ID_PLAY + 64) { ShellExecuteW(NULL, L"open", recs[id - ID_PLAY].path, NULL, NULL, SW_SHOW); return; }
    if (id >= ID_FOLDER && id < ID_FOLDER + 64) { OpenFolder(recs[id - ID_FOLDER].path); return; }
    if (id >= ID_WIN && id < ID_WIN + 24) {
        selWnd = wins[id - ID_WIN].h; wcscpy_s(selTitle, 128, wins[id - ID_WIN].title);
        mode = 1; showPicker = 0; errMsg[0] = 0; return;
    }
    switch (id) {
    case ID_MODE_SCREEN: if (!recording) { mode = 0; errMsg[0] = 0; } break;
    case ID_MODE_WIN: if (!recording) { mode = 1; errMsg[0] = 0; if (!selWnd || !IsWindow(selWnd)) { RefreshWins(); showPicker = 1; } } break;
    case ID_PICK: if (!recording) { RefreshWins(); showPicker = 1; } break;
    case ID_PICKREF: RefreshWins(); break;
    case ID_PICKCLOSE: showPicker = 0; break;
    case ID_GITHUB: ShellExecuteW(NULL, L"open", GITHUB_URL, NULL, NULL, SW_SHOWNORMAL); break;
    case ID_START: if (!recording && !finalizing) StartRec(); break;
    case ID_STOP: StopRec(); break;
    case ID_DISPLAY: if (!recording && nmon > 1) monIdx = (monIdx + 1) % nmon; break;
    case ID_FPS: if (!recording) fpsIdx = (fpsIdx + 1) % 3; break;
    case ID_QUAL: if (!recording) qIdx = (qIdx + 1) % 3; break;
    case ID_CURSOR: if (!recording) showCursor = !showCursor; break;
    case ID_BROWSE: if (!recording) Browse(); break;
    case ID_OPENDIR: OpenFolder(NULL); break;
    }
}

/* window stuff */
static LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: SetTimer(h, 1, 100, NULL); return 0;
    case WM_TIMER:
        if (recording && !finalizing && WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) {
            wcscpy_s(errMsg, 256, L"FFmpeg exited unexpectedly. Check display and settings.");
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread); if (hStdin) CloseHandle(hStdin); hStdin = NULL;
            recording = 0; Scan();
        }
        if (page == P_DASH || recording) InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_APP: recording = finalizing = 0; Scan(); InvalidateRect(h, NULL, FALSE); return 0;
    case WM_MOUSEMOVE: mouse.x = (short)LOWORD(l); mouse.y = (short)HIWORD(l); InvalidateRect(h, NULL, FALSE); return 0;
    case WM_LBUTTONUP: { POINT p = { (short)LOWORD(l), (short)HIWORD(l) };
        for (int i = 0; i < nb; i++) if (PtInRect(&btns[i].r, p)) { Click(btns[i].id); break; }
        InvalidateRect(h, NULL, FALSE); return 0; }
    case WM_GETMINMAXINFO: ((MINMAXINFO *)l)->ptMinTrackSize = (POINT){ 1000, 700 }; return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
        HDC md = CreateCompatibleDC(dc); HBITMAP bm = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
        HGDIOBJ ob = SelectObject(md, bm); Paint(md, rc.right, rc.bottom);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, md, 0, 0, SRCCOPY);
        SelectObject(md, ob); DeleteObject(bm); DeleteDC(md); EndPaint(h, &ps); return 0; }
    case WM_CLOSE:
        if (recording && !finalizing) { StopRec(); }
        if (finalizing) { WaitForSingleObject(pi.hProcess, 16000); }   /* give ffmpeg time to close the file */
        DestroyWindow(h); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
static HFONT MkFont(int h, int wt) {
    return CreateFontW(-h, 0, 0, 0, wt, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
}
int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, LPWSTR cl, int sw) {
    SetProcessDPIAware(); CoInitialize(NULL);
    EnumDisplayMonitors(NULL, NULL, MonCb, 0);
    if (!nmon) { mons[0] = (RECT){ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) }; nmon = 1; }
    WCHAR vid[MAX_PATH]; SHGetFolderPathW(NULL, CSIDL_MYVIDEO, NULL, 0, vid);
    swprintf(saveDir, MAX_PATH, L"%s\\MotionCapture", vid); CreateDirectoryW(saveDir, NULL); Scan();
    fN = MkFont(15, FW_NORMAL); fB = MkFont(15, FW_SEMIBOLD); fH = MkFont(26, FW_SEMIBOLD);
    fS = MkFont(13, FW_NORMAL); fT = MkFont(34, FW_SEMIBOLD);
    WNDCLASSW wc = { 0 }; wc.lpfnWndProc = Proc; wc.hInstance = hi; wc.lpszClassName = L"MotionCaptureWnd";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(1)); RegisterClassW(&wc);
    g_wnd = CreateWindowW(L"MotionCaptureWnd", L"Motion Capture", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                          CW_USEDEFAULT, CW_USEDEFAULT, 1180, 760, NULL, NULL, hi, NULL);
    MSG msg; while (GetMessageW(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return 0;
}
