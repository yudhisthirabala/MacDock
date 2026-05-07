// DockWindow.cpp
// Phase 2: loads pinned apps, extracts icons, paints, launches on click.
// Phase 3: drag-to-pin (DEC-011), drag-off unpin (DEC-012), running indicators.
// Phase 4: fish-eye hover magnification (DEC-014/015/016).
// Phase 6: DirectComposition rendering (DEC-027) — replaces WS_EX_LAYERED.

#include "DockWindow.h"
#include "DockDropTarget.h"
#include "../config/ConfigManager.h"
#include "../system/AppLauncher.h"
#include "../system/ProcessMonitor.h"
#include "../system/CompositionHelper.h"
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <commoncontrols.h>
#include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM
#include <gdiplus.h>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <unordered_map>

static const wchar_t* DOCK_CLASS_NAME = L"macOSWin_Dock";

static constexpr int  DOCK_EMPTY_WIDTH  = 120;  // px: placeholder width when no icons pinned
static constexpr int  DRAG_THRESHOLD   = 8;     // px: min drag distance before unpin gesture starts
static constexpr UINT FLASH_DURATION_MS = 200;  // ms: duration of the red reject flash
static constexpr UINT PROCESS_POLL_MS   = 1500; // ms: running-app polling interval
static constexpr UINT ENTRANCE_DURATION_MS = 256; // ms: slide-up animation duration (DEC-023)

// ─── Constructor / Destructor ─────────────────────────────────────────────────

DockWindow::DockWindow(HINSTANCE hInstance)
    : m_hInstance(hInstance), m_hwnd(nullptr)
{
}

DockWindow::~DockWindow()
{
    m_dcomp.Release();
    m_tooltipDcomp.Release();
    if (m_tooltipWnd) DestroyWindow(m_tooltipWnd);
    if (m_hwnd)
    {
        RevokeDragDrop(m_hwnd);
        DestroyWindow(m_hwnd);
    }
    if (m_dropTarget)
    {
        m_dropTarget->Release();
        m_dropTarget = nullptr;
    }
}

// ─── Create ──────────────────────────────────────────────────────────────────

bool DockWindow::Create()
{
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = DockWindow::WndProc;
    wc.hInstance     = m_hInstance;
    wc.lpszClassName = DOCK_CLASS_NAME;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);

    m_dpiScale = GetDpiForSystem() / 96.0f;

    SIZE mon    = Composition::GetPrimaryMonitorSize();
    int screenW = mon.cx;
    int screenH = mon.cy;
    int dockW   = S(DOCK_EMPTY_WIDTH);
    int x       = (screenW - dockW) / 2;
    int y       = screenH - S(DOCK_WINDOW_HEIGHT) - S(BOTTOM_GAP);

    // DEC-027 reverted: layered window + premultiplied DIB + UpdateLayeredWindow.
    // The DComp path produced unrecoverable white-box artefacts on Win11 24H2.
    // No acrylic backdrop, but icons render reliably.
    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        DOCK_CLASS_NAME, L"Dock",
        WS_POPUP,
        x, y, dockW, S(DOCK_WINDOW_HEIGHT),
        nullptr, nullptr, m_hInstance, this);

    if (!m_hwnd) return false;

    // Real OS acrylic backdrop (Win11 22H2+). Fails silently on older builds
    // and the dock still renders (just without blur behind it).
    // ApplySystemBackdrop disabled — incompatible with WS_EX_LAYERED.

    // Initialise the DComp target + visual + GDI-interop surface.
    if (!m_dcomp.Init(m_hwnd, dockW, S(DOCK_WINDOW_HEIGHT)))
        return false;

    // OLE drop target for drag-to-pin (DEC-011).
    m_dropTarget = new DockDropTarget(this);
    RegisterDragDrop(m_hwnd, m_dropTarget);

    const auto pinned = ConfigManager::Load();
    for (const auto& app : pinned)
        AddIcon(app.path, app.name);

    Reposition();

    SetTimer(m_hwnd, TIMER_PROCESS_MONITOR, PROCESS_POLL_MS, nullptr);

    // Tooltip: custom layered popup (macOS-style dark pill with app name)
    {
        static bool tipClassReg = false;
        if (!tipClassReg)
        {
            WNDCLASSEXW wc = {};
            wc.cbSize        = sizeof(wc);
            wc.lpfnWndProc   = DefWindowProcW;
            wc.hInstance     = m_hInstance;
            wc.lpszClassName = L"macOSWin_DockTip";
            wc.hbrBackground = nullptr;
            RegisterClassExW(&wc);
            tipClassReg = true;
        }
        m_tooltipWnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
            L"macOSWin_DockTip", L"",
            WS_POPUP,
            0, 0, 1, 1,
            nullptr, nullptr, m_hInstance, nullptr);
    }

    return true;
}

// ─── Show (DEC-023 entrance animation) ────────────────────────────────────────

void DockWindow::Show()
{
    // Slide-up entrance: show the window at its target position immediately,
    // but offset the DComp visual downward and animate it back to 0 over
    // ENTRANCE_DURATION_MS milliseconds. Because the DComp surface is rendered
    // once and the compositor handles the position each frame, no per-frame
    // GDI re-render is needed — only a SetWindowPos per tick.
    m_targetY = [&]() {
        SIZE mon = Composition::GetPrimaryMonitorSize();
        return mon.cy - S(DOCK_WINDOW_HEIGHT) - S(BOTTOM_GAP);
    }();

    // Position window at final Y immediately (DWMSBT acrylic follows the
    // window rect, so we keep the window at its destination and animate
    // the visual offset instead via SetWindowPos offscreen trick).
    // We place the window below the screen edge and slide it up with
    // SetWindowPos each timer tick — same as old approach but the DComp
    // surface only needs to be rendered once (no UpdateLayeredWindow per tick).
    int startY = m_targetY + S(DOCK_WINDOW_HEIGHT) + 20;

    RECT wr;
    GetWindowRect(m_hwnd, &wr);
    int curX = wr.left;
    int curW = wr.right - wr.left;

    SetWindowPos(m_hwnd, HWND_TOPMOST, curX, startY, curW, S(DOCK_WINDOW_HEIGHT),
                 SWP_NOACTIVATE | SWP_NOSIZE);
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);

    m_entranceActive  = true;
    m_entranceStartMs = GetTickCount();
    RenderDComp();
    SetTimer(m_hwnd, TIMER_ENTRANCE, 16, nullptr);
}

// ─── Reposition ──────────────────────────────────────────────────────────────

void DockWindow::Reposition()
{
    const int count    = static_cast<int>(m_icons.size());
    const int iconSize = S(ICON_SIZE);
    const int iconPad  = S(ICON_PADDING);

    // Extra width for the separator line at the end of the dock
    int sepExtra = (count > 0) ? iconPad / 2 : 0;
    int dockW = (count == 0)
        ? S(DOCK_EMPTY_WIDTH)
        : (count + 1) * iconPad + count * iconSize + sepExtra;

    SIZE      mon     = Composition::GetPrimaryMonitorSize();
    const int screenW = mon.cx;
    const int screenH = mon.cy;
    const int x       = (screenW - dockW) / 2;
    const int y       = screenH - S(DOCK_WINDOW_HEIGHT) - S(BOTTOM_GAP);
    m_targetY = y;

    if (m_hwnd && !m_entranceActive)
    {
        SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, dockW, S(DOCK_WINDOW_HEIGHT),
                     SWP_NOACTIVATE);
    }
    else if (m_hwnd && m_entranceActive)
    {
        // During entrance: update width/x but preserve current animated Y.
        SetWindowPos(m_hwnd, HWND_TOPMOST, x, 0, dockW, S(DOCK_WINDOW_HEIGHT),
                     SWP_NOMOVE | SWP_NOACTIVATE);
    }

    // Assign each icon its normal-size bounding rect in client coords.
    const int iconY = S(ICON_BOTTOM_Y) - iconSize;
    for (int i = 0; i < count; ++i)
    {
        RECT r;
        r.left   = iconPad + i * (iconSize + iconPad);
        r.top    = iconY;
        r.right  = r.left + iconSize;
        r.bottom = r.top  + iconSize;
        m_icons[i]->SetBounds(r);
    }

    // Resize the DComp surface to match the new window dimensions.
    m_dcomp.Resize(dockW, S(DOCK_WINDOW_HEIGHT));

    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

// ─── Icon extraction helpers ──────────────────────────────────────────────────

static std::wstring ResolveLnkTarget(const std::wstring& lnkPath)
{
    IShellLinkW* pLink = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&pLink))))
        return {};
    IPersistFile* pFile = nullptr;
    if (FAILED(pLink->QueryInterface(IID_PPV_ARGS(&pFile))))
    { pLink->Release(); return {}; }
    if (FAILED(pFile->Load(lnkPath.c_str(), STGM_READ)))
    { pFile->Release(); pLink->Release(); return {}; }
    wchar_t target[MAX_PATH] = {};
    pLink->GetPath(target, MAX_PATH, nullptr, 0);
    pFile->Release();
    pLink->Release();
    return target;
}

static HICON BitmapToIcon(HBITMAP hbm)
{
    if (!hbm) return nullptr;
    BITMAP bm = {};
    GetObject(hbm, sizeof(bm), &bm);
    ICONINFO ii = {};
    ii.fIcon    = TRUE;
    ii.hbmColor = hbm;
    ii.hbmMask  = CreateBitmap(bm.bmWidth, bm.bmHeight, 1, 1, nullptr);
    HICON hIcon = CreateIconIndirect(&ii);
    DeleteObject(ii.hbmMask);
    return hIcon;
}

static HICON FetchJumboIcon(const std::wstring& path)
{
    SHFILEINFOW sfi = {};
    DWORD_PTR ok = SHGetFileInfoW(path.c_str(), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX);
    if (!ok) return nullptr;
    IImageList* piml = nullptr;
    if (FAILED(SHGetImageList(SHIL_JUMBO, IID_PPV_ARGS(&piml)))) return nullptr;
    HICON hIcon = nullptr;
    piml->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hIcon);
    piml->Release();
    return hIcon;
}

static HBITMAP FetchShellBitmap(const std::wstring& path)
{
    IShellItem* pItem = nullptr;
    if (FAILED(SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&pItem))))
        return nullptr;
    IShellItemImageFactory* pFactory = nullptr;
    HBITMAP hBitmap = nullptr;
    if (SUCCEEDED(pItem->QueryInterface(IID_PPV_ARGS(&pFactory))))
    {
        SIZE sz = { 256, 256 };
        HRESULT hr = pFactory->GetImage(sz, SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK, &hBitmap);
        if (FAILED(hr) || !hBitmap)
        {
            hBitmap = nullptr;
            pFactory->GetImage(sz, SIIGBF_BIGGERSIZEOK, &hBitmap);
        }
        pFactory->Release();
    }
    pItem->Release();
    return hBitmap;
}

void DockWindow::AddIcon(const std::wstring& appPath, const std::wstring& appName)
{
    HICON   hIcon   = nullptr;
    HBITMAP hBitmap = nullptr;

    const std::wstring shellPrefix = L"shell:AppsFolder\\";
    if (appPath.size() > shellPrefix.size() &&
        appPath.substr(0, shellPrefix.size()) == shellPrefix)
    {
        HBITMAP tile = FetchShellBitmap(appPath);
        if (tile) { hIcon = BitmapToIcon(tile); DeleteObject(tile); }
        if (!hIcon) hIcon = FetchJumboIcon(appPath);
    }
    else
    {
        std::wstring iconSourcePath = appPath;
        {
            std::wstring ext = appPath;
            if (ext.size() >= 4)
            {
                ext = ext.substr(ext.size() - 4);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                if (ext == L".lnk")
                {
                    std::wstring target = ResolveLnkTarget(appPath);
                    if (!target.empty()) iconSourcePath = target;
                }
            }
        }
        hIcon = FetchJumboIcon(iconSourcePath);
        if (!hIcon) hBitmap = FetchShellBitmap(iconSourcePath);
        if (!hIcon && !hBitmap)
        {
            SHFILEINFOW sfi = {};
            DWORD_PTR ok = SHGetFileInfoW(iconSourcePath.c_str(), 0, &sfi, sizeof(sfi),
                                          SHGFI_ICON | SHGFI_LARGEICON);
            if (ok && sfi.hIcon) hIcon = sfi.hIcon;
        }
    }

    if (!hIcon && !hBitmap)
    {
        hIcon = LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
        if (hIcon) hIcon = CopyIcon(hIcon);
    }

    m_icons.emplace_back(std::make_unique<DockIcon>(appPath, appName, hIcon, hBitmap));
    Reposition();
}

void DockWindow::RemoveIcon(int index)
{
    if (index < 0 || index >= static_cast<int>(m_icons.size())) return;
    m_icons.erase(m_icons.begin() + index);
    Reposition();
}

bool DockWindow::HasIcon(const std::wstring& appPath) const
{
    std::wstring lowerNew = appPath;
    std::transform(lowerNew.begin(), lowerNew.end(), lowerNew.begin(), ::towlower);
    for (const auto& icon : m_icons)
    {
        std::wstring lowerEx = icon->GetPath();
        std::transform(lowerEx.begin(), lowerEx.end(), lowerEx.begin(), ::towlower);
        if (lowerEx == lowerNew) return true;
    }
    return false;
}

void DockWindow::SaveConfig()
{
    std::vector<PinnedApp> apps;
    apps.reserve(m_icons.size());
    for (const auto& icon : m_icons)
        apps.push_back({ icon->GetName(), icon->GetPath() });
    ConfigManager::Save(apps);
}

void DockWindow::FlashReject()
{
    m_flashActive = true;
    InvalidateRect(m_hwnd, nullptr, FALSE);
    SetTimer(m_hwnd, TIMER_FLASH_REJECT, FLASH_DURATION_MS, nullptr);
}

// ─── WndProc ─────────────────────────────────────────────────────────────────

LRESULT CALLBACK DockWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DockWindow* self = nullptr;
    if (msg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<DockWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    }
    else
    {
        self = reinterpret_cast<DockWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg)
    {
    case WM_PAINT:
        // With WS_EX_NOREDIRECTIONBITMAP there is no GDI redirection surface.
        // Validate the update region immediately to stop WM_PAINT firing again,
        // then push content to the DComp surface.
        ValidateRect(hwnd, nullptr);
        self->RenderDComp();
        return 0;

    case WM_ERASEBKGND:
        return 1; // suppress; DComp owns the background

    // WM_NCHITTEST: removed HTTRANSPARENT path — it blocked drag-drop on the
    // headroom above the pill. Layered windows with alpha=0 are already
    // mouse-transparent in those pixels.

    case WM_MOUSEMOVE:   self->OnMouseMove(LOWORD(lParam), HIWORD(lParam)); return 0;
    case WM_MOUSELEAVE:  self->OnMouseLeave();                              return 0;
    case WM_LBUTTONDOWN: self->OnLButtonDown(LOWORD(lParam), HIWORD(lParam)); return 0;
    case WM_LBUTTONUP:   self->OnLButtonUp(LOWORD(lParam), HIWORD(lParam));   return 0;
    case WM_TIMER:       self->OnTimer(wParam);                             return 0;
    case WM_DISPLAYCHANGE:
        self->Reposition();
        return 0;
    case WM_DESTROY:
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ─── Tooltip (macOS-style dark pill above hovered icon) ──────────────────────

void DockWindow::ShowTooltip(int iconIndex)
{
    if (!m_tooltipWnd || iconIndex < 0 || iconIndex >= static_cast<int>(m_icons.size()))
        return;

    const std::wstring& name = m_icons[iconIndex]->GetName();
    if (name.empty()) { HideTooltip(); return; }

    // Measure text to determine tooltip size
    HDC screenDC = GetDC(nullptr);
    Gdiplus::Graphics gMeasure(screenDC);
    Gdiplus::FontFamily family(L"Segoe UI");
    float fontSize = 11.0f * m_dpiScale;
    Gdiplus::Font font(&family, fontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::RectF textBounds;
    gMeasure.MeasureString(name.c_str(), -1, &font, Gdiplus::PointF(0, 0), &textBounds);
    ReleaseDC(nullptr, screenDC);

    int padX = S(10);
    int padY = S(5);
    int tipW = static_cast<int>(textBounds.Width + 0.5f) + padX * 2;
    int tipH = static_cast<int>(textBounds.Height + 0.5f) + padY * 2;

    // Position above the icon, centered
    RECT iconBounds = m_icons[iconIndex]->GetBounds();
    int iconCenterX = (iconBounds.left + iconBounds.right) / 2;
    RECT dockRect;
    GetWindowRect(m_hwnd, &dockRect);
    int tipX = dockRect.left + iconCenterX - tipW / 2;
    int tipY = dockRect.top - tipH - S(4);

    // Init or resize the DComp surface
    if (m_tooltipDcomp.w == 0)
        m_tooltipDcomp.Init(m_tooltipWnd, tipW, tipH);
    else
        m_tooltipDcomp.Resize(tipW, tipH);

    SetWindowPos(m_tooltipWnd, HWND_TOPMOST, tipX, tipY, tipW, tipH,
                 SWP_NOACTIVATE);

    // Render the tooltip
    POINT offset;
    HDC hdc = m_tooltipDcomp.BeginDraw(&offset);
    if (hdc)
    {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

        g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));
        g.SetCompositingMode(Gdiplus::CompositingModeSourceOver);

        float r = static_cast<float>(S(6));
        Gdiplus::GraphicsPath path;
        path.AddArc(0.0f, 0.0f, r * 2, r * 2, 180.0f, 90.0f);
        path.AddArc(static_cast<float>(tipW) - r * 2, 0.0f, r * 2, r * 2, 270.0f, 90.0f);
        path.AddArc(static_cast<float>(tipW) - r * 2, static_cast<float>(tipH) - r * 2, r * 2, r * 2, 0.0f, 90.0f);
        path.AddArc(0.0f, static_cast<float>(tipH) - r * 2, r * 2, r * 2, 90.0f, 90.0f);
        path.CloseFigure();

        Gdiplus::SolidBrush bg(Gdiplus::Color(220, 30, 30, 34));
        g.FillPath(&bg, &path);

        Gdiplus::SolidBrush fg(Gdiplus::Color(255, 245, 245, 247));
        Gdiplus::StringFormat sf;
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);
        sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::RectF box(0.0f, 0.0f, static_cast<float>(tipW), static_cast<float>(tipH));
        g.DrawString(name.c_str(), -1, &font, box, &sf, &fg);
    }
    m_tooltipDcomp.EndDraw();
    m_tooltipDcomp.Commit();

    ShowWindow(m_tooltipWnd, SW_SHOWNOACTIVATE);
}

void DockWindow::HideTooltip()
{
    if (m_tooltipWnd)
        ShowWindow(m_tooltipWnd, SW_HIDE);
}

// ─── OnPaint / RenderDComp ────────────────────────────────────────────────────

void DockWindow::OnPaint()
{
    // Handled entirely inside WndProc (ValidateRect + RenderDComp).
    // This stub is here in case it's called directly.
    ValidateRect(m_hwnd, nullptr);
    RenderDComp();
}

void DockWindow::RenderDComp()
{
    POINT offset;
    HDC hdc = m_dcomp.BeginDraw(&offset);
    if (!hdc) return;

    const int w = m_dcomp.w;
    const int h = m_dcomp.h;

    {   // inner scope: Graphics must be destroyed before EndDraw
        Gdiplus::Graphics g(hdc);
        g.TranslateTransform(static_cast<Gdiplus::REAL>(offset.x),
                             static_cast<Gdiplus::REAL>(offset.y));
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

        // Clear to fully transparent so non-pill areas are invisible and
        // click-through (complemented by WM_NCHITTEST HTTRANSPARENT above).
        // CompositingModeSourceCopy writes alpha=0 without blending.
        g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));
        g.SetCompositingMode(Gdiplus::CompositingModeSourceOver);

        if (m_flashActive)
        {
            Gdiplus::SolidBrush fb(Gdiplus::Color(160, 200, 60, 60));
            g.FillRectangle(&fb, 0, 0, w, h);
        }

        const int n = static_cast<int>(m_icons.size());

        // Seelen-style rounded dock pill — always drawn so the dock is visible
        // even when empty (serves as drop target and visual indicator).
        if (!m_flashActive)
        {
            int pillPadX = S(ICON_PADDING) / 2;
            int pillPadY = S(6);
            int pillL, pillR;
            if (n > 0)
            {
                RECT first = m_icons.front()->GetBounds();
                RECT last  = m_icons.back()->GetBounds();
                pillL = first.left  - pillPadX;
                pillR = last.right  + pillPadX;
            }
            else
            {
                pillL = pillPadX;
                pillR = w - pillPadX;
            }
            int pillT = (S(ICON_BOTTOM_Y) - S(ICON_SIZE)) - pillPadY;
            int pillB = S(ICON_BOTTOM_Y) + pillPadY;
            float radius = (pillB - pillT) / 2.0f;

            Gdiplus::GraphicsPath path;
            path.AddArc((Gdiplus::REAL)pillL,                (Gdiplus::REAL)pillT,
                        radius * 2, radius * 2, 180.0f, 90.0f);
            path.AddArc((Gdiplus::REAL)(pillR - radius * 2), (Gdiplus::REAL)pillT,
                        radius * 2, radius * 2, 270.0f, 90.0f);
            path.AddArc((Gdiplus::REAL)(pillR - radius * 2), (Gdiplus::REAL)(pillB - radius * 2),
                        radius * 2, radius * 2,   0.0f, 90.0f);
            path.AddArc((Gdiplus::REAL)pillL,                (Gdiplus::REAL)(pillB - radius * 2),
                        radius * 2, radius * 2,  90.0f, 90.0f);
            path.CloseFigure();

            Gdiplus::SolidBrush body(Gdiplus::Color(130, 22, 22, 26));
            g.FillPath(&body, &path);

            Gdiplus::Pen hilite(Gdiplus::Color(40, 255, 255, 255), 0.5f);
            g.DrawPath(&hilite, &path);
        }

        if (n > 0)
        {
            const int MAGNIFY_RADIUS = S(120);
            std::vector<float> scales(n, 1.0f);
            if (m_cursorPos.x >= 0)
            {
                for (int i = 0; i < n; ++i)
                {
                    RECT b  = m_icons[i]->GetBounds();
                    float cx   = (b.left + b.right) * 0.5f;
                    float dist = fabsf(static_cast<float>(m_cursorPos.x) - cx);
                    if (dist < static_cast<float>(MAGNIFY_RADIUS))
                    {
                        float t = dist / static_cast<float>(MAGNIFY_RADIUS);
                        scales[i] = 1.0f + (MAGNIFY_MAX - 1.0f) *
                                    0.5f * (1.0f + cosf(3.14159265f * t));
                    }
                }
            }

            // Draw smallest-scale first so the most-magnified icon renders on top.
            std::vector<int> order(n);
            std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(),
                      [&scales](int a, int b){ return scales[a] < scales[b]; });

            Gdiplus::SolidBrush dotBrush(Gdiplus::Color(200, 255, 255, 255));

            for (int idx : order)
            {
                const auto& icon = m_icons[idx];
                if (!icon->GetIcon() && !icon->GetBitmap()) continue;
                if (m_dragging && idx == m_dragIndex) continue;

                RECT b = icon->GetBounds();
                int slotCx   = (b.left + b.right) / 2;
                int scaledSz = static_cast<int>(S(ICON_SIZE) * scales[idx]);
                int drawLeft = slotCx - scaledSz / 2;
                int drawTop  = S(ICON_BOTTOM_Y) - scaledSz;

                // Bounce offset: 3 bounces with decay (macOS-style)
                int bounceOff = 0;
                if (idx == m_bounceIndex && m_bounceStartMs != 0)
                {
                    DWORD elapsed = GetTickCount() - m_bounceStartMs;
                    if (elapsed < BOUNCE_DURATION_MS)
                    {
                        float t = static_cast<float>(elapsed) / static_cast<float>(BOUNCE_DURATION_MS);
                        float decay = (1.0f - t) * (1.0f - t);
                        float bounce = fabsf(sinf(t * 3.14159265f * 3.0f)) * decay;
                        bounceOff = static_cast<int>(S(BOUNCE_HEIGHT) * bounce);
                    }
                }
                drawTop -= bounceOff;

                Gdiplus::Bitmap* gbmp = nullptr;
                if (icon->GetBitmap())
                    gbmp = Gdiplus::Bitmap::FromHBITMAP(icon->GetBitmap(), nullptr);
                else
                    gbmp = Gdiplus::Bitmap::FromHICON(icon->GetIcon());

                if (gbmp)
                {
                    g.DrawImage(gbmp, drawLeft, drawTop, scaledSz, scaledSz);
                    delete gbmp;
                }

                if (icon->IsRunning())
                {
                    float dotCx = static_cast<float>(slotCx);
                    float dotCy = static_cast<float>(S(ICON_BOTTOM_Y) + S(DOT_OFFSET));
                    float dotR  = static_cast<float>(S(DOT_RADIUS));
                    g.FillEllipse(&dotBrush,
                                  dotCx - dotR, dotCy - dotR,
                                  dotR * 2.0f, dotR * 2.0f);
                }
            }

            // Separator: always draw a thin vertical line after the last
            // pinned icon (macOS-style divider at end of dock).
            if (n >= 1)
            {
                RECT lastB = m_icons.back()->GetBounds();
                float sepX = static_cast<float>(lastB.right + S(ICON_PADDING) / 2);
                float sepT = static_cast<float>(S(ICON_BOTTOM_Y) - S(ICON_SIZE) + S(6));
                float sepB = static_cast<float>(S(ICON_BOTTOM_Y) - S(6));
                Gdiplus::Pen sepPen(Gdiplus::Color(80, 255, 255, 255), 1.0f);
                g.DrawLine(&sepPen, sepX, sepT, sepX, sepB);
            }
        }
    }  // g destroyed here — required before EndDraw

    m_dcomp.EndDraw();
    m_dcomp.Commit();
}

// ─── Mouse handlers ───────────────────────────────────────────────────────────

void DockWindow::OnMouseMove(int x, int y)
{
    if (!m_mouseTracking)
    {
        TRACKMOUSEEVENT tme = {};
        tme.cbSize    = sizeof(tme);
        tme.dwFlags   = TME_LEAVE;
        tme.hwndTrack = m_hwnd;
        TrackMouseEvent(&tme);
        SetTimer(m_hwnd, TIMER_ANIMATE, 16, nullptr);
        m_mouseTracking = true;
    }
    m_cursorPos = { x, y };
    InvalidateRect(m_hwnd, nullptr, FALSE);

    // Tooltip: detect which icon is hovered
    {
        int newHover = -1;
        POINT p { x, y };
        for (int i = 0; i < static_cast<int>(m_icons.size()); ++i)
        {
            RECT b = m_icons[i]->GetBounds();
            if (PtInRect(&b, p)) { newHover = i; break; }
        }
        if (newHover != m_hoveredIndex)
        {
            m_hoveredIndex = newHover;
            if (newHover >= 0)
                ShowTooltip(newHover);
            else
                HideTooltip();
        }
    }

    if (m_dragIndex >= 0 && !m_dragging)
    {
        int dx = x - m_dragStart.x;
        int dy = y - m_dragStart.y;
        if (dx * dx + dy * dy >= DRAG_THRESHOLD * DRAG_THRESHOLD)
        {
            m_dragging = true;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
    }
}

void DockWindow::OnMouseLeave()
{
    m_mouseTracking = false;
    if (m_hoveredIndex >= 0)
    {
        m_hoveredIndex = -1;
        HideTooltip();
    }
}

void DockWindow::OnLButtonDown(int x, int y)
{
    POINT p { x, y };
    for (int i = 0; i < static_cast<int>(m_icons.size()); ++i)
    {
        RECT b = m_icons[i]->GetBounds();
        if (PtInRect(&b, p))
        {
            m_dragging  = false;
            m_dragIndex = i;
            m_dragStart = p;
            SetCapture(m_hwnd);
            return;
        }
    }
    m_dragIndex = -1;
}

void DockWindow::OnLButtonUp(int x, int y)
{
    if (m_dragIndex < 0) return;

    const int savedIndex = m_dragIndex;
    const bool wasDragging = m_dragging;
    m_dragging  = false;
    m_dragIndex = -1;
    ReleaseCapture();

    if (wasDragging)
    {
        RECT clientRect;
        GetClientRect(m_hwnd, &clientRect);
        POINT p { x, y };
        if (!PtInRect(&clientRect, p))
        {
            RemoveIcon(savedIndex);
            SaveConfig();
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    int dx = x - m_dragStart.x;
    int dy = y - m_dragStart.y;
    if (dx * dx + dy * dy < DRAG_THRESHOLD * DRAG_THRESHOLD)
    {
        if (savedIndex >= 0 && savedIndex < static_cast<int>(m_icons.size()))
        {
            // Start bounce animation (macOS-style icon bounce on launch)
            m_bounceIndex   = savedIndex;
            m_bounceStartMs = GetTickCount();
            SetTimer(m_hwnd, TIMER_BOUNCE, 16, nullptr);

            AppLauncher::LaunchOrFocus(m_icons[savedIndex]->GetPath());
        }
    }
}

void DockWindow::OnDropComplete(int pinnedCount, int totalFiles)
{
    if (pinnedCount > 0) SaveConfig();
    if (pinnedCount == 0 && totalFiles > 0) FlashReject();
}

// ─── Timer handler ────────────────────────────────────────────────────────────

void DockWindow::OnTimer(WPARAM timerId)
{
    if (timerId == TIMER_PROCESS_MONITOR)
    {
        auto running = ProcessMonitor::GetRunningAppNames();
        bool changed = false;

        static std::unordered_map<std::wstring, std::wstring> s_exeCache;
        static bool s_cacheBuilt = false;
        if (!s_cacheBuilt) { s_exeCache.clear(); s_cacheBuilt = true; }

        auto resolveExeName = [](const std::wstring& path) -> std::wstring {
            auto lower = [](std::wstring s) {
                std::transform(s.begin(), s.end(), s.begin(), ::towlower);
                return s;
            };
            auto basename = [](const std::wstring& p) {
                size_t slash = p.find_last_of(L"\\/");
                return (slash != std::wstring::npos) ? p.substr(slash + 1) : p;
            };

            const std::wstring shellPrefix = L"shell:AppsFolder\\";
            if (path.size() > shellPrefix.size() &&
                lower(path.substr(0, shellPrefix.size())) == lower(shellPrefix))
            {
                std::wstring aumid = lower(path.substr(shellPrefix.size()));

                // If the AUMID contains .EXE, extract it directly (e.g. Microsoft.Office.WINWORD.EXE.15)
                size_t exePos = aumid.find(L".exe");
                if (exePos != std::wstring::npos)
                {
                    // Walk backwards to find the start of the exe name
                    size_t start = aumid.rfind(L'.', exePos - 1);
                    if (start == std::wstring::npos) start = 0; else ++start;
                    std::wstring exe = aumid.substr(start, exePos - start + 4);
                    return exe;
                }

                // Known UWP app-to-exe mappings (match against full lowered AUMID)
                struct Mapping { const wchar_t* pattern; const wchar_t* exe; };
                static const Mapping mappings[] = {
                    { L"immersivecontrolpanel",  L"systemsettings.exe" },
                    { L"msedge",                 L"msedge.exe" },
                    { L"microsoftedge",          L"msedge.exe" },
                    { L"windowsterminal",        L"windowsterminal.exe" },
                    { L"windows.photos",         L"microsoft.photos.exe" },
                    { L"photos",                 L"microsoft.photos.exe" },
                    { L"zunemusic",              L"music.ui.exe" },
                    { L"zunevideo",              L"video.ui.exe" },
                    { L"windowscalculator",      L"calculatorapp.exe" },
                    { L"windowsstore",           L"winstore.app.exe" },
                    { L"windowscamera",          L"windowscamera.exe" },
                    { L"windowsalarms",          L"time.exe" },
                    { L"outlook",                L"olk.exe" },
                    { L"windowsnotepad",         L"notepad.exe" },
                    { L"paint",                  L"mspaint.exe" },
                    { L"whatsapp",               L"whatsapp.exe" },
                    { L"gamingapp",              L"gamingservicesui.exe" },
                    { L"xbox",                   L"gamebar.exe" },
                    { L"chrome",                 L"chrome.exe" },
                    { L"spotify",                L"spotify.exe" },
                    { L"discord",                L"discord.exe" },
                    { L"teams",                  L"ms-teams.exe" },
                    { L"onenote",                L"onenote.exe" },
                    { L"clipchamp",              L"clipchamp.exe" },
                    { L"screenclipping",         L"snippingtool.exe" },
                    { L"snippingtool",           L"snippingtool.exe" },
                };
                for (const auto& m : mappings)
                {
                    if (aumid.find(m.pattern) != std::wstring::npos)
                        return m.exe;
                }

                // Fallback: extract the part after the last backslash or dot
                // as a guess (won't match most cases but better than nothing)
                size_t bang = aumid.find(L'!');
                std::wstring familyName = (bang != std::wstring::npos) ? aumid.substr(0, bang) : aumid;
                return lower(familyName);
            }

            std::wstring lowerPath = lower(path);
            if (lowerPath.size() >= 4 &&
                lowerPath.substr(lowerPath.size() - 4) == L".lnk")
            {
                std::wstring target = ResolveLnkTarget(path);
                if (!target.empty()) return lower(basename(target));
            }
            return lower(basename(path));
        };

        for (const auto& icon : m_icons)
        {
            const std::wstring& path = icon->GetPath();
            auto it = s_exeCache.find(path);
            if (it == s_exeCache.end())
                it = s_exeCache.emplace(path, resolveExeName(path)).first;

            bool isRunning = !it->second.empty() && running.count(it->second) > 0;
            if (icon->IsRunning() != isRunning)
            {
                icon->SetRunning(isRunning);
                changed = true;
            }
        }
        if (changed) InvalidateRect(m_hwnd, nullptr, FALSE);
    }
    else if (timerId == TIMER_FLASH_REJECT)
    {
        m_flashActive = false;
        KillTimer(m_hwnd, TIMER_FLASH_REJECT);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
    else if (timerId == TIMER_ANIMATE)
    {
        POINT cur = {};
        GetCursorPos(&cur);
        RECT wr = {};
        GetWindowRect(m_hwnd, &wr);
        if (PtInRect(&wr, cur))
        {
            ScreenToClient(m_hwnd, &cur);
            m_cursorPos = cur;
        }
        else
        {
            m_cursorPos     = { -1, -1 };
            m_mouseTracking = false;
            KillTimer(m_hwnd, TIMER_ANIMATE);
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
    else if (timerId == TIMER_BOUNCE)
    {
        DWORD elapsed = GetTickCount() - m_bounceStartMs;
        if (elapsed >= BOUNCE_DURATION_MS)
        {
            m_bounceIndex = -1;
            KillTimer(m_hwnd, TIMER_BOUNCE);
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
    else if (timerId == TIMER_ENTRANCE)
    {
        // Ease-out quadratic slide-up: p(t) = start + dist*(2t/T - t²/T²)
        // With DComp rendering, the surface content is already committed — only
        // the window position needs updating each tick (fast SetWindowPos call,
        // no GDI re-render required). This avoids the UpdateLayeredWindow
        // per-frame cost that caused the old entrance animation to fail.
        DWORD elapsed = GetTickCount() - m_entranceStartMs;
        if (elapsed >= ENTRANCE_DURATION_MS)
        {
            m_entranceActive = false;
            KillTimer(m_hwnd, TIMER_ENTRANCE);

            RECT wr;
            GetWindowRect(m_hwnd, &wr);
            SetWindowPos(m_hwnd, HWND_TOPMOST,
                         wr.left, m_targetY, 0, 0,
                         SWP_NOSIZE | SWP_NOACTIVATE);
            RenderDComp();
            return;
        }

        float T    = static_cast<float>(ENTRANCE_DURATION_MS);
        float t    = static_cast<float>(elapsed);
        float ease = (2.0f * t / T) - (t * t) / (T * T);

        int startY = m_targetY + S(DOCK_WINDOW_HEIGHT) + 20;
        int curY   = startY + static_cast<int>((m_targetY - startY) * ease);

        RECT wr;
        GetWindowRect(m_hwnd, &wr);
        SetWindowPos(m_hwnd, HWND_TOPMOST,
                     wr.left, curY, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE);
        RenderDComp();
    }
}
