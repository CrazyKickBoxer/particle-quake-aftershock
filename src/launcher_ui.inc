/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
// Native, data-driven launcher pages. Included inside the launcher namespace.
struct SettingRow {
  HWND label{}, value{}, help{};
  int page{};
  std::wstring name, explanation, key, fallback;
  bool toggle = false, integer = false;
  double minimum = 0, maximum = 0;
};
std::vector<SettingRow> rows;
const wchar_t *pages[] = {L"Game & launch",         L"Display",
                          L"Particle surfaces",     L"Neon & reflections",
                          L"Destruction & effects", L"Holo Physics",
                          L"Gore & death",          L"Wakes & atmosphere",
                          L"Audio & mouse",         L"Advanced"};
HWND quality, qualityButton;
HWND searchLabel;
HWND panel, navigation, search, pageTitle, pageHint, browse, launchButton,
    saveButton, copyButton, resetButton, prismButton, fineButton, bubble,
    steamGogButton, sharewareButton;
int currentPage = 0, scrollY = 0, contentHeight = 0;
std::wstring helpText;
HBRUSH yellow;
std::wstring lower(std::wstring s) {
  for (auto &c : s)
    c = towlower(c);
  return s;
}
void layoutRows();
LRESULT CALLBACK panelProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_COMMAND || m == WM_CTLCOLORBTN || m == WM_CTLCOLORSTATIC ||
      m == WM_CTLCOLORLISTBOX || m == WM_DRAWITEM)
    return SendMessageW(window, m, w, l);
  if (m == WM_ERASEBKGND) {
    RECT r;
    GetClientRect(h, &r);
    FillRect(HDC(w), &r, background);
    return 1;
  }
  if (m == WM_MOUSEWHEEL || m == WM_VSCROLL) {
    if (m == WM_MOUSEWHEEL)
      scrollY -= GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA * px(90);
    else {
      SCROLLINFO si{sizeof(si), SIF_TRACKPOS};
      GetScrollInfo(h, SB_VERT, &si);
      switch (LOWORD(w)) {
      case SB_LINEUP:
        scrollY -= px(40);
        break;
      case SB_LINEDOWN:
        scrollY += px(40);
        break;
      case SB_PAGEUP:
        scrollY -= px(300);
        break;
      case SB_PAGEDOWN:
        scrollY += px(300);
        break;
      case SB_THUMBTRACK:
        scrollY = si.nTrackPos;
        break;
      }
    }
    layoutRows();
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}
LRESULT CALLBACK bubbleProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_PAINT || m == WM_PRINTCLIENT) {
    PAINTSTRUCT ps{};
    HDC dc = m == WM_PAINT ? BeginPaint(h, &ps) : HDC(w);
    RECT r;
    GetClientRect(h, &r);
    FillRect(dc, &r, yellow);
    InflateRect(&r, -px(16), -px(14));
    SelectObject(dc, font);
    SetTextColor(dc, RGB(36, 30, 10));
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, helpText.c_str(), -1, &r, DT_WORDBREAK | DT_NOPREFIX);
    if (m == WM_PAINT)
      EndPaint(h, &ps);
    return 0;
  }
  if (m == WM_LBUTTONDOWN || m == WM_KILLFOCUS) {
    ShowWindow(h, SW_HIDE);
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}
void showHelp(size_t index) {
  if (index >= rows.size())
    return;
  auto &s = rows[index];
  helpText = s.name + L"\n\n" + s.explanation;
  if (!s.key.empty())
    helpText += L"\n\nConsole: " + s.key;
  helpText += L"\n\nClick this bubble to dismiss.";
  RECT a;
  GetWindowRect(s.help, &a);
  RECT r{0, 0, px(360), 0};
  HDC dc = GetDC(window);
  SelectObject(dc, font);
  DrawTextW(dc, helpText.c_str(), -1, &r,
            DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
  ReleaseDC(window, dc);
  int width = px(392), height = r.bottom + px(28);
  MONITORINFO mi{sizeof(mi)};
  GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &mi);
  int x = std::clamp(a.right - width, mi.rcWork.left, mi.rcWork.right - width),
      y = std::clamp(a.bottom + px(5), mi.rcWork.top,
                     mi.rcWork.bottom - height);
  SetWindowPos(bubble, HWND_TOP, x, y, width, height,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
  InvalidateRect(bubble, nullptr, TRUE);
}
void row(HWND value, int page, const wchar_t *name, const wchar_t *help) {
  SetParent(value, panel);
  if ((GetWindowLongPtrW(value, GWL_STYLE) & BS_TYPEMASK) == BS_AUTOCHECKBOX) {
    wchar_t cls[32];
    GetClassNameW(value, cls, 32);
    if (wcscmp(cls, L"Button") == 0)
      SetWindowTheme(value, L"", L"");
  }
  HWND old = window;
  window = panel;
  HWND label =
      control(L"STATIC", name, SS_LEFT | SS_NOPREFIX, 0, 0, 280, 26, 0);
  HWND question =
      control(L"BUTTON", L"?", BS_OWNERDRAW | BS_NOTIFY | WS_TABSTOP, 0, 0, 28,
              28, 3000 + int(rows.size()));
  window = old;
  rows.push_back({label, value, question, page, name, help});
  SetWindowTextW(value, text(value).c_str());
}
void number(int page, const wchar_t *name, const wchar_t *key,
            const wchar_t *def, double min, double max, const wchar_t *help,
            bool integer = false) {
  HWND v = control(L"EDIT", def, WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 200, 28,
                   2000 + int(rows.size()));
  row(v, page, name, help);
  auto &r = rows.back();
  r.key = key;
  r.fallback = def;
  r.minimum = min;
  r.maximum = max;
  r.integer = integer;
  wchar_t range[160];
  swprintf_s(range, L"\nRange: %.4g to %.4g. Default: %s.", min, max, def);
  r.explanation += range;
}
void toggle(int page, const wchar_t *name, const wchar_t *key, bool def,
            const wchar_t *help) {
  HWND v =
      control(L"BUTTON", L"Enabled", WS_TABSTOP | BS_NOTIFY | BS_AUTOCHECKBOX,
              0, 0, 200, 28, 2000 + int(rows.size()));
  row(v, page, name, help);
  auto &r = rows.back();
  r.key = key;
  r.fallback = def ? L"1" : L"0";
  r.toggle = true;
  SendMessageW(v, BM_SETCHECK, def ? BST_CHECKED : BST_UNCHECKED, 0);
}
void layoutRows() {
  if (!panel)
    return;
  RECT r;
  GetClientRect(panel, &r);
  std::wstring query = lower(text(search));
  int count = 0;
  for (auto &s : rows)
    if (query.empty()
            ? s.page == currentPage
            : lower(s.name + L" " + s.explanation + L" " + s.key).find(query) !=
                  std::wstring::npos)
      ++count;
  contentHeight = px(74) * count;
  scrollY = std::clamp(scrollY, 0, std::max(0, contentHeight - int(r.bottom)));
  SCROLLINFO si{sizeof(si),
                SIF_RANGE | SIF_PAGE | SIF_POS,
                0,
                std::max(0, contentHeight - 1),
                UINT(r.bottom),
                scrollY};
  SetScrollInfo(panel, SB_VERT, &si, TRUE);
  GetClientRect(panel, &r);
  int y = -scrollY;
  for (auto &s : rows) {
    bool visible =
        query.empty()
            ? s.page == currentPage
            : lower(s.name + L" " + s.explanation + L" " + s.key).find(query) !=
                  std::wstring::npos;
    for (HWND c : {s.label, s.value, s.help})
      ShowWindow(c, visible ? SW_SHOW : SW_HIDE);
    if (!visible)
      continue;
    int valueX = std::max(px(240), int(r.right) / 2 - 20),
        valueWidth = r.right - valueX - px(60);
    MoveWindow(s.label, px(16), y + px(20), valueX - px(30), px(40), TRUE);
    wchar_t cls[32];
    GetClassNameW(s.value, cls, 32);
    MoveWindow(s.value, valueX, y + px(18), valueWidth,
               px(wcscmp(cls, L"ComboBox") == 0 ? 300 : 30), TRUE);
    MoveWindow(s.help, r.right - px(42), y + px(18), px(28), px(28), TRUE);
    y += px(74);
  }
  SetWindowTextW(pageTitle,
                 query.empty() ? pages[currentPage] : L"Search results");
  std::wstring hint = std::to_wstring(count) +
                      L" settings  /  Click a yellow ? for an explanation";
  SetWindowTextW(pageHint, hint.c_str());
  bool page0 = query.empty() && currentPage == 0;
  ShowWindow(browse, page0 ? SW_SHOW : SW_HIDE);
  ShowWindow(steamGogButton, page0 ? SW_SHOW : SW_HIDE);
  ShowWindow(sharewareButton, page0 ? SW_SHOW : SW_HIDE);
  InvalidateRect(panel, nullptr, TRUE);
}
void layout() {
  if (!panel)
    return;
  RECT r;
  GetClientRect(window, &r);
  int w = r.right, h = r.bottom;
  MoveWindow(titleControl, px(24), px(18), px(350), px(42), TRUE);
  MoveWindow(qualityButton, w - px(416), px(22), px(136), px(32), TRUE);
  MoveWindow(prismButton, w - px(270), px(22), px(122), px(32), TRUE);
  MoveWindow(fineButton, w - px(138), px(22), px(114), px(32), TRUE);
  MoveWindow(searchLabel, px(24), px(66), px(210), px(20), TRUE);
  MoveWindow(search, px(24), px(90), px(210), px(32), TRUE);
  MoveWindow(navigation, px(24), px(134), px(210), h - px(340), TRUE);
  MoveWindow(pageTitle, px(264), px(80), w - px(450), px(34), TRUE);
  MoveWindow(pageHint, px(264), px(119), w - px(290), px(24), TRUE);
  MoveWindow(browse, w - px(144), px(80), px(120), px(30), TRUE);
  MoveWindow(steamGogButton, w - px(292), px(80), px(140), px(30), TRUE);
  MoveWindow(sharewareButton, w - px(450), px(80), px(150), px(30), TRUE);
  MoveWindow(panel, px(248), px(152), w - px(272), h - px(346), TRUE);
  MoveWindow(preview, px(24), h - px(170), w - px(48), px(82), TRUE);
  MoveWindow(resetButton, px(24), h - px(64), px(140), px(38), TRUE);
  MoveWindow(copyButton, px(174), h - px(64), px(140), px(38), TRUE);
  MoveWindow(saveButton, w - px(300), h - px(64), px(120), px(38), TRUE);
  MoveWindow(launchButton, w - px(168), h - px(64), px(144), px(38), TRUE);
  layoutRows();
}
std::wstring additionalCommand() {
  std::wstring result;
  for (auto &s : rows)
    if (!s.key.empty())
      result +=
          L" +" + s.key + L" " +
          (s.toggle ? (SendMessageW(s.value, BM_GETCHECK, 0, 0) == BST_CHECKED
                           ? L"1"
                           : L"0")
                    : text(s.value));
  return result;
}
void saveAdditional() {
  for (auto &s : rows)
    if (!s.key.empty())
      persist(L"Settings", s.key.c_str(),
              (s.toggle
                   ? (SendMessageW(s.value, BM_GETCHECK, 0, 0) == BST_CHECKED
                          ? L"1"
                          : L"0")
                   : text(s.value))
                  .c_str(),
              ini.c_str());
}
void restoreAdditional() {
  for (auto &s : rows)
    if (!s.key.empty()) {
      wchar_t buf[256];
      GetPrivateProfileStringW(
          L"Settings", s.key.c_str(),
          (s.key == L"as_radius"   ? std::to_wstring(90 + choice(preset) * 40)
           : s.key == L"as_damage" ? std::to_wstring(3 + choice(preset) * 3)
                                   : s.fallback)
              .c_str(),
          buf, 256, ini.c_str());
      if (s.toggle)
        SendMessageW(s.value, BM_SETCHECK,
                     wcscmp(buf, L"1") == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
      else
        SetWindowTextW(s.value, buf);
    }
}
void setSetting(const wchar_t *key, const wchar_t *value) {
  for (auto &s : rows)
    if (s.key == key) {
      if (s.toggle)
        SendMessageW(s.value, BM_SETCHECK,
                     wcscmp(value, L"1") == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
      else
        SetWindowTextW(s.value, value);
    }
}

struct QualityPreset {
  const wchar_t *name;
  int density, layers, amount;
  bool fidelity, reflections, gibs, goo;
  const wchar_t *physics, *budget, *gore, *dust, *spray, *settle, *lights;
};
const QualityPreset qualityPresets[] = {
    {L"Low", 0, 1, 1, false, false, false, false, L"0", L"16384", L"2048", L"0",
     L"40", L"0.8", L"0"},
    {L"Medium", 0, 2, 1, true, false, true, true, L"1", L"16384", L"4096", L"0",
     L"64", L"1.0", L"0"},
    {L"High", 1, 3, 2, true, true, true, true, L"1", L"65536", L"20000", L"0.8",
     L"96", L"1.2", L"1"},
    {L"Ultra", 2, 3, 3, true, true, true, true, L"1", L"131072", L"32768", L"1",
     L"144", L"1.8", L"1"},
    {L"Ultra Max", 2, 4, 4, true, true, true, true, L"1", L"262144", L"65536",
     L"1.2", L"200", L"2", L"1"}};
std::vector<std::pair<const wchar_t *, const wchar_t *>>
qualityValues(const QualityPreset &q) {
  return {{L"r_holo_physics", q.physics},
          {L"r_holo_phys_budget", q.budget},
          {L"r_holo_phys_gore_max", q.gore},
          {L"r_holo_phys_dust", q.dust},
          {L"r_holo_phys_spray_count", q.spray},
          {L"r_holo_phys_settle", q.settle},
          {L"r_holo_phys_debris_light", q.lights},
          {L"r_holo_phys_gore", L"1"},
          {L"as_nails", L"1"},
          {L"vid_fsaa", L"0"}};
}
void refreshQuality() {
  if (!quality)
    return;
  int selected = 0;
  for (int i = 0; i < 5; ++i) {
    const auto &q = qualityPresets[i];
    bool matches = choice(renderer) == 1 && choice(density) == q.density &&
                   choice(layers) == q.layers - 1 &&
                   choice(amount) == q.amount - 1;
    for (auto pair :
         std::vector<std::pair<HWND, bool>>{{fidelity, q.fidelity},
                                            {reflections, q.reflections},
                                            {gibs, q.gibs},
                                            {goo, q.goo},
                                            {effects, true}})
      matches = matches && ((SendMessageW(pair.first, BM_GETCHECK, 0, 0) ==
                             BST_CHECKED) == pair.second);
    for (auto pair : qualityValues(q))
      for (auto &r : rows)
        if (r.key == pair.first) {
          auto value =
              r.toggle
                  ? (SendMessageW(r.value, BM_GETCHECK, 0, 0) == BST_CHECKED
                         ? L"1"
                         : L"0")
                  : text(r.value);
          matches = matches && wcstod(value.c_str(), nullptr) ==
                                   wcstod(pair.second, nullptr);
        }
    if (matches) {
      selected = i + 1;
      break;
    }
  }
  SendMessageW(quality, CB_SETCURSEL, selected, 0);
}
void applyQuality(int selection) {
  if (selection < 1 || selection > 5)
    return;
  bool previous = loading;
  loading = true;
  const auto &q = qualityPresets[selection - 1];
  SendMessageW(renderer, CB_SETCURSEL, 1, 0);
  SendMessageW(density, CB_SETCURSEL, q.density, 0);
  SendMessageW(layers, CB_SETCURSEL, q.layers - 1, 0);
  SendMessageW(amount, CB_SETCURSEL, q.amount - 1, 0);
  for (auto pair :
       std::vector<std::pair<HWND, bool>>{{fidelity, q.fidelity},
                                          {reflections, q.reflections},
                                          {gibs, q.gibs},
                                          {goo, q.goo},
                                          {effects, true}})
    SendMessageW(pair.first, BM_SETCHECK,
                 pair.second ? BST_CHECKED : BST_UNCHECKED, 0);
  for (auto pair : qualityValues(q))
    setSetting(pair.first, pair.second);
  loading = previous;
  update();
}
bool validateSettings(bool notify) {
  int argc = 0;
  LPWSTR *argv = CommandLineToArgvW(command().c_str(), &argc);
  if (argv)
    LocalFree(argv);
  if (argc > 256 || command().size() >= 32767) {
    if (notify)
      MessageBoxW(window,
                  L"The launch command is too long. Shorten the extra launch "
                  L"arguments.",
                  L"Check setting", MB_ICONWARNING);
    return false;
  }

  for (auto &s : rows) {
    if (s.key.empty() || s.toggle)
      continue;
    auto v = text(s.value);
    wchar_t *end = nullptr;
    double n = wcstod(v.c_str(), &end);
    if (v.empty() || end == v.c_str() || *end || !std::isfinite(n) ||
        n < s.minimum || n > s.maximum || (s.integer && std::floor(n) != n) ||
        (s.key == L"vid_fsaa" && n != 0 && n != 1 && n != 2 && n != 4 &&
         n != 8 && n != 16)) {
      if (notify) {
        SetWindowTextW(search, L"");
        currentPage = s.page;
        SendMessageW(navigation, LB_SETCURSEL, currentPage, 0);
        scrollY = 0;
        layoutRows();
        for (auto &a : rows) {
          if (&a == &s)
            break;
          if (a.page == s.page)
            scrollY += px(74);
        }
        layoutRows();
        SetFocus(s.value);
        MessageBoxW(window,
                    (L"Please enter a valid value for " + s.name + L".\n\n" +
                     s.explanation)
                        .c_str(),
                    L"Check setting", MB_ICONWARNING);
      }
      return false;
    }
  }
  wchar_t *end = nullptr;
  auto f = text(fov);
  double n = wcstod(f.c_str(), &end);
  if (f.empty() || *end || !std::isfinite(n) || n < 60 || n > 140 ||
      floor(n) != n) {
    if (notify)
      MessageBoxW(window,
                  L"Field of view must be a whole number from 60 to 140.",
                  L"Check setting", MB_ICONWARNING);
    return false;
  }
  return true;
}
void buildPages() {
  WNDCLASSW wc{};
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = background;
  wc.lpfnWndProc = panelProc;
  wc.lpszClassName = L"PQSettingsPanel";
  RegisterClassW(&wc);
  panel = CreateWindowExW(WS_EX_CONTROLPARENT, wc.lpszClassName, L"",
                          WS_CHILD | WS_VISIBLE | WS_VSCROLL, 0, 0, 0, 0,
                          window, nullptr, wc.hInstance, nullptr);
  wc.lpfnWndProc = bubbleProc;
  wc.lpszClassName = L"PQYellowHelp";
  RegisterClassW(&wc);
  yellow = CreateSolidBrush(RGB(255, 224, 105));
  bubble = CreateWindowExW(
      WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName, L"Setting help",
      WS_POPUP | WS_BORDER, 0, 0, 0, 0, window, nullptr, wc.hInstance, nullptr);
  searchLabel = control(L"STATIC", L"SEARCH ALL SETTINGS", 0, 0, 0, 210, 20, 0);
  search =
      control(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 200, 30, 400);
  SendMessageW(search, 0x1501, TRUE, LPARAM(L"Search settings..."));
  navigation = control(L"LISTBOX", L"",
                       WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY |
                           LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
                       0, 0, 200, 400, 401);
  for (auto p : pages)
    SendMessageW(navigation, LB_ADDSTRING, 0, LPARAM(p));
  SendMessageW(navigation, LB_SETITEMHEIGHT, 0, px(40));
  SendMessageW(navigation, LB_SETCURSEL, 0, 0);
  pageTitle = control(L"STATIC", L"", SS_NOPREFIX, 0, 0, 500, 34, 0);
  SendMessageW(pageTitle, WM_SETFONT, WPARAM(titleFont), TRUE);
  pageHint = control(L"STATIC", L"", 0, 0, 0, 600, 24, 0);
  auto edit = [&](HWND &v, int page, const wchar_t *name, const wchar_t *help,
                  int id, const wchar_t *def) {
    v = control(L"EDIT", def, WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 200, 30, id);
    row(v, page, name, help);
  };
  auto cb = [&](HWND &v, int page, const wchar_t *name, const wchar_t *help,
                int id, std::vector<const wchar_t *> options, int def) {
    v = combo(0, 0, 200, id, options, def);
    row(v, page, name, help);
  };
  auto ck = [&](HWND &v, int page, const wchar_t *name, const wchar_t *help,
                int id) {
    v = control(L"BUTTON", L"Enabled", WS_TABSTOP | BS_NOTIFY | BS_AUTOCHECKBOX,
                0, 0, 200, 30, id);
    row(v, page, name, help);
  };
  edit(data, 0, L"Quake data folder",
       L"Choose your Quake folder or its id1 subfolder. A valid id1/pak0.pak "
       L"is required. Use Browse at the top of this page.",
       100, L"");
  cb(addon, 0, L"Game / expansion",
     L"Select the original game or an installed mission pack. Expansion data "
     L"must already exist in your Quake folder.",
     119, {L"Quake", L"Scourge of Armagon", L"Dissolution of Eternity"}, 0);
  edit(mod, 0, L"Mod folder",
       L"Optional mod directory relative to the Quake folder, such as quoth. "
       L"Leave blank for the selected game. Requires the mod to be installed.",
       120, L"");
  edit(map, 0, L"Starting map",
       L"Map name without .bsp, for example e1m1 or start. Launching starts "
       L"this map; it does not load a save.",
       108, L"e1m1");
  cb(skill, 0, L"Difficulty",
     L"Quake gameplay difficulty. Nightmare changes monster behavior; this is "
     L"a gameplay setting.",
     109, {L"Easy", L"Normal", L"Hard", L"Nightmare"}, 1);
  cb(quality, 1, L"Quality preset",
     L"Low favors speed; Medium enables modest reactive physics; High balances "
     L"reflections and detail; Ultra adds Showcase density; Ultra Max uses "
     L"four layers and the largest physics/gore budgets. Ultra Max can be "
     L"substantially slower. Presets keep your selected structure, colors, "
     L"reflection look, resolution, frame cap, audio and gameplay settings. "
     L"Editing a managed setting shows Custom. Changes apply at the next "
     L"launch.",
     411,
     {L"Custom", L"Low", L"Medium", L"High (recommended)", L"Ultra",
      L"Ultra Max"},
     0);
  cb(display, 1, L"Display mode",
     L"Windowed uses the chosen size. Desktop fullscreen uses your desktop "
     L"size. Exclusive fullscreen uses the requested resolution.",
     121, {L"Windowed", L"Desktop fullscreen", L"Exclusive fullscreen"}, 0);
  cb(resolution, 1, L"Resolution",
     L"Higher resolution improves detail but increases GPU cost. Desktop "
     L"fullscreen follows the desktop resolution instead.",
     107,
     {L"1280 x 720", L"1600 x 900", L"1920 x 1080", L"2560 x 1440",
      L"3840 x 2160"},
     0);
  edit(fov, 1, L"Field of view",
       L"Horizontal viewing angle, 60 to 140 degrees. Higher values show more "
       L"of the room and make objects appear smaller.",
       122, L"100");
  ck(vsync, 1, L"Vertical sync",
     L"Synchronizes presentation to your screen to reduce tearing. It can "
     L"increase input latency and limit frame rate.",
     110);
  number(1, L"Frame-rate limit", L"host_maxfps", L"200", 10, 1000,
         L"Maximum rendering frames per second. Lower caps reduce power use "
         L"and heat.",
         true);
  number(1, L"Antialiasing samples", L"vid_fsaa", L"0", 0, 16,
         L"MSAA sample request; valid requests are 0, 1, 2, 4, 8 or 16. "
         L"Supported sample counts depend on your GPU. 0 disables it and is "
         L"recommended for Neon fidelity.",
         true);
  number(1, L"Gamma", L"gamma", L"0.9", 0.5, 1.5,
         L"Brightness response. Lower values brighten dark areas.");
  number(1, L"Contrast", L"contrast", L"1.4", 1, 2,
         L"Raises the contrast of the final image. High values can crush dark "
         L"details.");
  cb(renderer, 2, L"Renderer",
     L"Classic restores triangle rendering. Particle surfaces enables the "
     L"holographic point-cloud renderer. Holo Physics requires particle "
     L"rendering.",
     102, {L"Classic triangles", L"Particle surfaces"}, 1);
  cb(structure, 2, L"Particle structure",
     L"Changes the actual shapes and spatial arrangement of surface samples. "
     L"Neon Cathedral uses its own cyan/gold treatment and enables the "
     L"dedicated fidelity pipeline.",
     132,
     {L"Original square samples", L"Soft Dust Cloud", L"Solid Beads",
      L"Broken Stone Chips", L"Scattered Microvoxels", L"Layered Flakes",
      L"Splinter Field", L"Fiber Bundles", L"Goo Blobs", L"Hollow Rings",
      L"Mixed Debris Cloud", L"Neon Cathedral"},
     11);
  cb(style, 2, L"Material palette",
     L"Surface color treatment for non-Neon structures. Neon Cathedral has its "
     L"own palette, so this selection is disabled in that structure.",
     104,
     {L"Faithful", L"Enhanced", L"Inferno", L"Inferno color", L"Living Stone",
      L"Volcanic", L"Sandstorm", L"Crystal", L"Corrupted Flesh",
      L"Industrial Rust", L"Spectral", L"Frozen Ruins", L"Electric Grid",
      L"Cosmic Dust"},
     0);
  cb(density, 2, L"Surface density",
     L"Play favors speed. Fine balances detail and cost. Showcase draws a "
     L"denser field and uses more GPU time. Holo Physics has a separate "
     L"active-particle budget.",
     105, {L"Play", L"Fine", L"Showcase"}, 1);
  cb(layers, 2, L"Particle layers",
     L"Stacks stable surface samples for depth. More layers make walls richer "
     L"but increase particle rendering cost.",
     133, {L"1 layer", L"2 layers", L"3 layers", L"4 layers"}, 2);
  ck(fidelity, 3, L"Neon fidelity",
     L"Enables the enhanced Neon post-processing path. Requires Neon Cathedral "
     L"particle structure.",
     134);
  ck(reflections, 3, L"Floor reflections",
     L"Reflects visible neon detail on floors. Requires Neon Cathedral and "
     L"fidelity. Screen-space reflections cannot show objects outside the "
     L"rendered view.",
     135);
  toggle(3, L"Neon Prism", L"as_neon_prism", false,
         L"Polished cyan/gold appearance with stronger edges and foreground "
         L"glints. Requires Neon Cathedral; use the Neon Prism preset button "
         L"to enable its prerequisites.");
  number(3, L"Reflection strength", L"as_reflection_strength", L"1", 0, 4,
         L"Scales reflected light. Around 2.4 produces the brighter "
         L"polished-floor reference look.");
  number(3, L"Reflection roughness", L"as_reflection_roughness", L"0.20", 0.03,
         0.7,
         L"Lower values sharpen reflections; higher values soften them. "
         L"Prism's recommended value is 0.10.");
  number(3, L"Neon glow", L"as_neon_glow", L"1", 0, 2,
         L"Scales the luminous halo. Too much glow can obscure individual "
         L"points.");
  cb(mode, 4, L"World mode",
     L"Faithful preserves the normal world. Destruction sandbox enables the "
     L"existing PhysX destruction mode. This is separate from renderer-only "
     L"Holo Physics.",
     103, {L"Faithful Quake", L"Destruction sandbox"}, 1);
  cb(preset, 4, L"Destruction strength",
     L"Sets the sandbox blast radius and damage together: Restrained 90/3, "
     L"Cinematic 130/6, Cataclysm 170/9. Independent of Holo blast strength.",
     106, {L"Restrained", L"Cinematic", L"Cataclysm"}, 1);
  number(4, L"Destruction radius", L"as_radius", L"130", 1, 1024,
         L"Sandbox blast radius in Quake units. The strength preset fills this "
         L"value; you can then customize it.");
  number(4, L"Destruction damage", L"as_damage", L"6", 0, 100,
         L"Damage applied to the sandbox destruction system. Independent of "
         L"cosmetic Holo impulses.");
  toggle(4,L"Progressive destruction",L"as_progressive",true,L"Rockets chip eligible interior structures and weaken their bonds over repeated hits. Unsupported wall, pillar and bridge sections fall with PhysX. Requires Destruction sandbox world mode. The exterior shell and unrecognized geometry stay protected.");
  number(4,L"Structure toughness",L"as_rocket_hits",L"4",2,12,L"Nominal direct-hit toughness. Higher values need more rockets; material, distance and support connections also affect the count. Small non-support chips can break earlier.");
  number(4,L"Structural blast radius",L"as_chip_radius",L"72",24,160,L"Maximum radius of physical damage in Quake units. Keeps each hit localized while repeated hits widen the breach. Does not change rocket gameplay damage.");
  ck(effects, 4, L"Combat effects",
     L"Enables the Aftershock combat effect layer. Switch off for a simpler "
     L"presentation.",
     123);
  ck(shake, 4, L"Camera shake",
     L"Adds a 0.35-strength visual camera kick to blasts. Disable for a "
     L"steadier view.",
     124);
  ck(reduced, 4, L"Reduced flashes",
     L"Reduces bright combat flashes for a calmer presentation.", 125);
  cb(amount, 4, L"Effect particle amount",
     L"Scales transient effect particles: 1x is 2,048 particles per explosion; "
     L"4x is 8,192. Separate from world density and the Holo active budget.",
     131, {L"1x", L"2x", L"3x", L"4x"}, 1);
  toggle(4, L"Nail ricochets", L"as_nails", true,
         L"Visual sparks, debris and pressure ripples at nail impacts. Does "
         L"not change the real nail trajectory or damage.");
  toggle(5, L"Holo Physics", L"r_holo_physics", false,
         L"Lets GPU world points detach, react to combat and spring home. "
         L"Cosmetic only: no changes to gameplay physics or entity state. "
         L"Requires particle rendering.");
  number(5, L"Active particle budget", L"r_holo_phys_budget", L"65536", 1024,
         262144,
         L"Maximum displaced points. Fine uses 65,536; Play 16,384; Showcase "
         L"131,072. Raising it increases GPU memory and heavy-combat cost.",
         true);
  number(5, L"Return / settle time", L"r_holo_phys_settle", L"1.2", 0.1, 5,
         L"Seconds controlling the spring return. Longer times leave the scan "
         L"disturbed for longer.");
  number(5, L"Blast strength", L"r_holo_phys_blast", L"1", 0, 8,
         L"Outward shockwave force on holographic points. Does not change "
         L"rocket damage.");
  number(5, L"Blast radius scale", L"r_holo_phys_blast_radius", L"1", 0.1, 4,
         L"Scales the visual disturbance radius. Wider blasts can activate "
         L"more points.");
  number(5, L"Suction", L"r_holo_phys_suction", L"0.35", 0, 1,
         L"Inward pulse after a blast. Adds a brief breathing-in motion before "
         L"points settle.");
  number(
      5, L"Impact spray strength", L"r_holo_phys_spray", L"1", 0, 8,
      L"Scales the short, sharp visual spray produced by bullets and spikes.");
  number(5, L"Impact spray count", L"r_holo_phys_spray_count", L"96", 0, 200,
         L"Maximum points requested for a small impact. Lower counts reduce "
         L"busy combat effects.",
         true);
  toggle(5, L"Particle collision", L"r_holo_phys_collide", true,
         L"Free particles collide with the renderer's coarse static world "
         L"volume. Moving doors and lifts are not represented by this volume.");
  number(5, L"Bounce restitution", L"r_holo_phys_restitution", L"0.25", 0, 1,
         L"Amount of speed retained after a particle bounce. 0 is dead, 1 is "
         L"fully elastic.");
  ck(gibs, 6, L"Directional gibs",
     L"Enables the existing flying chunk effect with directional motion. "
     L"Separate from Holo point-cloud gore.",
     129);
  ck(goo, 6, L"Goo splashes & trails",
     L"Enables the existing goo wall splashes and ground slide trails.", 130);
  toggle(6, L"Holo gore", L"r_holo_phys_gore", true,
         L"Detached blood points stick to static architecture and slowly fade "
         L"instead of springing home.");
  number(6, L"Stuck gore cap", L"r_holo_phys_gore_max", L"20000", 0, 65536,
         L"Maximum persistent gore points. Lower values reduce draw cost and "
         L"visual accumulation.",
         true);
  number(6, L"Gore lifetime", L"r_holo_phys_gore_life", L"45", 1, 90,
         L"Seconds that stuck blood remains visible while fading.");
  number(6, L"Gore gravity", L"r_holo_phys_gore_gravity", L"1", 0, 4,
         L"Gravity multiplier for detached blood points. Larger values pull "
         L"them down faster.");
  number(6, L"Death dissolution", L"r_holo_phys_death", L"1", 0, 8,
         L"Strength of cosmetic monster point-cloud dissolution. Zero disables "
         L"this effect; entity lifetime is unchanged.");
  number(6, L"Death effect lifetime", L"r_holo_phys_death_life", L"1.2", 0.2, 4,
         L"Seconds for the detached death effect and fading core.");
  number(7, L"Projectile wake", L"r_holo_phys_wake", L"0.7", 0, 4,
         L"Pushes points sideways around fast projectiles and gibs. Zero "
         L"disables the parting wake.");
  number(7, L"Player wake", L"r_holo_phys_player_wake", L"0.5", 0, 4,
         L"Subtle floor-point scatter while moving. No contribution while "
         L"standing still.");
  number(7, L"Lightning disturbance", L"r_holo_phys_beam", L"1", 0, 8,
         L"Strength of the point disturbance around thunderbolt beams.");
  number(7, L"Lightning radius", L"r_holo_phys_beam_radius", L"24", 4, 128,
         L"Beam disturbance radius in Quake units. Larger radii can activate "
         L"more particles.");
  number(7, L"Liquid splashes", L"r_holo_phys_liquid", L"1", 0, 8,
         L"Scales cosmetic lava and liquid-surface disturbances.");
  number(7, L"Teleport implosion", L"r_holo_phys_teleport", L"1", 0, 8,
         L"Strength of the inward pull and release around a teleport event.");
  number(7, L"Ceiling dust", L"r_holo_phys_dust", L"0.8", 0, 2,
         L"Sparse falling dust from surfaces above explosions. Set 0 for the "
         L"conservative Play preset.");
  number(7, L"Structural shudder", L"r_holo_phys_shake", L"0.4", 0, 1,
         L"Scales the short local point-field shudder from combat. Separate "
         L"from camera shake.");
  number(7, L"Turbulence", L"r_holo_phys_turbulence", L"0.6", 0, 8,
         L"Slow wind force on free points only. Anchored and stuck points do "
         L"not get animated particle noise.");
  number(7, L"Turbulence scale", L"r_holo_phys_turbulence_scale", L"48", 8, 512,
         L"Spatial scale of the wind field in Quake units. Larger values give "
         L"broader, smoother motion.");
  toggle(7, L"Debris lights", L"r_holo_phys_debris_light", true,
         L"A few bright debris particles use the existing dynamic-light budget "
         L"for short-lived local glows.");
  number(8, L"Sound volume", L"volume", L"0.7", 0, 1,
         L"Sound effect volume. 0 mutes weapon and world sounds; 1 is full "
         L"volume.");
  number(8, L"Music volume", L"bgmvolume", L"1", 0, 1,
         L"Background music volume, from silent to full volume.");
  number(8, L"Mouse sensitivity", L"sensitivity", L"3", 0.1, 30,
         L"Mouse turning sensitivity. Higher values turn the view farther for "
         L"the same mouse movement.");
  ck(debug, 9, L"Engine diagnostics",
     L"Writes a console log and enables developer messages and the FPS "
     L"counter.",
     127);
  toggle(9, L"Holo debug overlay", L"r_holo_phys_debug", false,
         L"Shows Holo active count, gore, events, evictions and compute "
         L"timing. Requires Holo Physics.");
  edit(extra, 9, L"Extra launch arguments",
       L"Advanced engine or mod options appended last. These can override "
       L"controls above. Use +name value for console settings; quote paths "
       L"containing spaces.",
       111, L"");
  browse = control(L"BUTTON", L"Browse...", WS_TABSTOP, 0, 0, 120, 30, 101);
  steamGogButton = control(L"BUTTON", L"Find Steam/GOG", WS_TABSTOP, 0, 0, 140,
                            30, 133);
  sharewareButton =
      control(L"BUTTON", L"Get shareware Quake", WS_TABSTOP, 0, 0, 150, 30,
              134);
  preview = control(L"EDIT", L"",
                    ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL |
                        WS_TABSTOP,
                    0, 0, 800, 82, 112);
  resetButton =
      control(L"BUTTON", L"Reset this page", WS_TABSTOP, 0, 0, 140, 38, 405);
  copyButton =
      control(L"BUTTON", L"Copy command", WS_TABSTOP, 0, 0, 140, 38, 406);
  saveButton =
      control(L"BUTTON", L"Save settings", WS_TABSTOP, 0, 0, 120, 38, 407);
  launchButton = control(L"BUTTON", L"Launch Quake",
                         WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 144, 38, 114);
  qualityButton =
      control(L"BUTTON", L"Quality presets", WS_TABSTOP, 0, 0, 136, 32, 410);
  prismButton =
      control(L"BUTTON", L"Neon Prism", WS_TABSTOP, 0, 0, 122, 32, 408);
  fineButton = control(L"BUTTON", L"Holo Fine", WS_TABSTOP, 0, 0, 114, 32, 409);
}
