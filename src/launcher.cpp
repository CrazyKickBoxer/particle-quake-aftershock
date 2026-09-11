/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Josh Nicholls */
#define UNICODE
#define _UNICODE
#define NOMINMAX
#pragma comment(                                                               \
    linker,                                                                    \
    "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <shlobj.h>
#include <string>
#include <uxtheme.h>
#include <vector>
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "uxtheme.lib")
#include <cmath>
#include <cwctype>

namespace {
HWND window, data, renderer, mode, style, density, preset, map, skill,
    resolution, extra, preview, vsync;
HWND addon, mod, display, fov, effects, shake, reduced, debug, titleControl;
HWND gibs, goo, amount, structure, layers, fidelity, reflections;
HFONT font, titleFont;
HBRUSH background;
std::wstring root, ini, userdir, engineDirectory;
std::wstring findEngineDirectory(const std::wstring &launcherDirectory) {
  std::filesystem::path directory(launcherDirectory);
  std::error_code error;
  if (std::filesystem::is_regular_file(directory / L"vkQuake.exe", error))
    return directory.wstring();
  for (int depth = 0; depth < 5 && !directory.empty(); ++depth) {
    auto candidate = directory / L"bin";
    if (std::filesystem::is_regular_file(candidate / L"vkQuake.exe", error))
      return candidate.wstring();
    auto parent = directory.parent_path();
    if (parent == directory)
      break;
    directory = parent;
  }
  return launcherDirectory;
}
bool loading = true, selftest = false;
int dpi = 96;
int px(int x) { return MulDiv(x, dpi, 96); }
void refreshQuality();
std::wstring additionalCommand();
void saveAdditional();
bool saveSucceeded = true;
BOOL persist(LPCWSTR section, LPCWSTR key, LPCWSTR value, LPCWSTR path) {
  BOOL result = WritePrivateProfileStringW(section, key, value, path);
  saveSucceeded = saveSucceeded && result;
  return result;
}
void restoreAdditional();
bool validateSettings(bool notify);
std::wstring text(HWND h) {
  int n = GetWindowTextLengthW(h);
  std::wstring s(n + 1, L'\0');
  GetWindowTextW(h, s.data(), n + 1);
  s.resize(n);
  return s;
}
std::wstring quote(const std::wstring &s) {
  std::wstring out = L"\"";
  size_t slash = 0;
  for (wchar_t c : s) {
    if (c == L'\\') {
      ++slash;
      continue;
    }
    if (c == L'\"') {
      out.append(slash * 2 + 1, L'\\');
      out += c;
    } else {
      out.append(slash, L'\\');
      out += c;
    }
    slash = 0;
  }
  out.append(slash * 2, L'\\');
  return out + L"\"";
}
int choice(HWND h) { return int(SendMessageW(h, CB_GETCURSEL, 0, 0)); }
std::wstring base() {
  std::filesystem::path p(text(data));
  if (_wcsicmp(p.filename().c_str(), L"id1") == 0)
    p = p.parent_path();
  return p.wstring();
}
bool isValidPak(const std::filesystem::path &p) {
  HANDLE f = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, 0, nullptr);
  if (f == INVALID_HANDLE_VALUE)
    return false;
  uint32_t header[3];
  DWORD read = 0;
  LARGE_INTEGER size{};
  GetFileSizeEx(f, &size);
  ReadFile(f, header, 12, &read, nullptr);
  CloseHandle(f);
  return read == 12 && memcmp(header, "PACK", 4) == 0 && header[1] >= 12 &&
         header[2] > 0 && header[2] % 64 == 0 &&
         uint64_t(header[1]) + header[2] <= uint64_t(size.QuadPart);
}
bool validData() {
  return isValidPak(std::filesystem::path(base()) / L"id1" / L"pak0.pak");
}
bool hasValidId1(const std::wstring &folder) {
  return isValidPak(std::filesystem::path(folder) / L"id1" / L"pak0.pak");
}
// GOG and older Steam installers are frequently 32-bit and register under
// the WOW6432Node registry view even on a 64-bit machine running this (native
// x64) launcher; a plain lookup only sees the native 64-bit view, so every
// query here retries once under the 32-bit view before giving up.
std::wstring registryString(HKEY root, const std::wstring &subkey,
                             const wchar_t *value) {
  wchar_t buf[MAX_PATH]{};
  DWORD size = sizeof(buf);
  if (RegGetValueW(root, subkey.c_str(), value, RRF_RT_REG_SZ, nullptr, buf,
                    &size) == ERROR_SUCCESS)
    return buf;
  size = sizeof(buf);
  if (RegGetValueW(root, subkey.c_str(), value,
                    RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY, nullptr, buf,
                    &size) == ERROR_SUCCESS)
    return buf;
  return L"";
}
std::vector<std::wstring> registrySubkeys(HKEY root,
                                           const std::wstring &subkey) {
  std::vector<std::wstring> names;
  HKEY key;
  REGSAM views[] = {KEY_READ, KEY_READ | KEY_WOW64_32KEY};
  for (REGSAM view : views) {
    if (RegOpenKeyExW(root, subkey.c_str(), 0, view, &key) != ERROR_SUCCESS)
      continue;
    for (DWORD i = 0;; ++i) {
      wchar_t name[256];
      DWORD nameLength = 256;
      if (RegEnumKeyExW(key, i, name, &nameLength, nullptr, nullptr, nullptr,
                         nullptr) != ERROR_SUCCESS)
        break;
      names.emplace_back(name);
    }
    RegCloseKey(key);
    if (!names.empty())
      break;
  }
  return names;
}
// Steam library folders beyond the primary install are listed in a small,
// simply-quoted VDF text file; a full VDF parser is unnecessary here.
std::vector<std::wstring> steamLibraryFolders(const std::wstring &steamPath) {
  std::vector<std::wstring> libraries{steamPath};
  std::filesystem::path vdf =
      std::filesystem::path(steamPath) / L"steamapps" / L"libraryfolders.vdf";
  std::error_code error;
  if (!std::filesystem::exists(vdf, error))
    return libraries;
  FILE *file = _wfopen(vdf.c_str(), L"rb");
  if (!file)
    return libraries;
  std::string contents;
  char chunk[4096];
  size_t read;
  while ((read = fread(chunk, 1, sizeof(chunk), file)) > 0)
    contents.append(chunk, read);
  fclose(file);
  size_t pos = 0;
  while ((pos = contents.find("\"path\"", pos)) != std::string::npos) {
    size_t open = contents.find('"', pos + 6);
    size_t close = open == std::string::npos
                       ? std::string::npos
                       : contents.find('"', open + 1);
    if (open == std::string::npos || close == std::string::npos)
      break;
    std::string raw = contents.substr(open + 1, close - open - 1);
    std::wstring path;
    for (size_t i = 0; i < raw.size(); ++i) {
      if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '\\') {
        path += L'\\';
        ++i;
      } else {
        path += wchar_t((unsigned char)raw[i]);
      }
    }
    libraries.push_back(path);
    pos = close + 1;
  }
  return libraries;
}
std::wstring findSteamQuake() {
  std::wstring steamPath =
      registryString(HKEY_CURRENT_USER, L"SOFTWARE\\Valve\\Steam", L"SteamPath");
  if (steamPath.empty())
    steamPath = registryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam",
                                L"InstallPath");
  if (steamPath.empty())
    return L"";
  for (const std::wstring &library : steamLibraryFolders(steamPath)) {
    std::wstring candidate =
        (std::filesystem::path(library) / L"steamapps" / L"common" / L"Quake")
            .wstring();
    if (hasValidId1(candidate))
      return candidate;
  }
  return L"";
}
std::wstring findGogQuake() {
  for (const std::wstring &id :
       registrySubkeys(HKEY_LOCAL_MACHINE, L"SOFTWARE\\GOG.com\\Games")) {
    std::wstring path =
        registryString(HKEY_LOCAL_MACHINE,
                        L"SOFTWARE\\GOG.com\\Games\\" + id, L"path");
    if (!path.empty() && hasValidId1(path))
      return path;
  }
  return L"";
}
// Only ever reads the registry and checks for an existing id1/pak0.pak on
// disk - no network access, nothing is downloaded or written here.
std::wstring findInstalledQuake() {
  std::wstring found = findSteamQuake();
  return found.empty() ? findGogQuake() : found;
}
std::wstring command() {
  const wchar_t *renderers[] = {L"classic", L"particle"},
                *modes[] = {L"faithful", L"destruction"},
                *styles[] = {L"faithful",        L"enhanced",
                             L"inferno",         L"inferno-color",
                             L"living-stone",    L"volcanic",
                             L"sandstorm",       L"crystal",
                             L"corrupted-flesh", L"industrial-rust",
                             L"spectral",        L"frozen-ruins",
                             L"electric-grid",   L"cosmic-dust"},
                *densities[] = {L"play", L"fine", L"showcase"};
  int width[] = {1280, 1600, 1920, 2560, 3840},
      height[] = {720, 900, 1080, 1440, 2160}, r = choice(resolution);
  std::wstring cmd = quote(engineDirectory + L"\\vkquake.exe") + L" -basedir " +
                     quote(base()) + L" -userdir " + quote(userdir);
  if (choice(addon) == 1)
    cmd += L" -hipnotic";
  if (choice(addon) == 2)
    cmd += L" -rogue";
  if (!text(mod).empty())
    cmd += L" -game " + quote(text(mod));
  cmd += L" -renderer " + std::wstring(renderers[choice(renderer)]) +
         L" -worldmode " + modes[choice(mode)] + L" -physics " +
         (choice(mode) ? L"physx-cpu" : L"off");
  cmd += L" -style " + std::wstring(styles[choice(style)]) + L" -density " +
         densities[choice(density)];
  cmd += (choice(display) ? L" -fullscreen" : L" -window");
  cmd += L" -width " + std::to_wstring(width[r]) + L" -height " +
         std::to_wstring(height[r]);
  cmd += L" +vid_desktopfullscreen " +
         std::to_wstring(choice(display) == 1 ? 1 : 0) + L" +fov " +
         std::to_wstring(std::clamp(_wtoi(text(fov).c_str()), 60, 140));
  cmd += L" +as_effects " +
         std::to_wstring(
             SendMessageW(effects, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0) +
         L" +as_shake " +
         (SendMessageW(shake, BM_GETCHECK, 0, 0) == BST_CHECKED
              ? std::wstring(L"0.35")
              : std::wstring(L"0")) +
         L" +as_reduced_flashes " +
         std::to_wstring(
             SendMessageW(reduced, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
  if (SendMessageW(debug, BM_GETCHECK, 0, 0) == BST_CHECKED)
    cmd += L" -condebug +developer 1 +show_fps 1";
  cmd += L" +as_gibs " +
         std::to_wstring(
             SendMessageW(gibs, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
  cmd += L" +as_goo " +
         std::to_wstring(
             SendMessageW(goo, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
  cmd += L" +as_particle_amount " + std::to_wstring(choice(amount) + 1);
  cmd += L" +as_structure " + std::to_wstring(choice(structure));
  cmd += L" +as_layers " + std::to_wstring(choice(layers) + 1);
  cmd += L" +as_fidelity " +
         std::to_wstring(
             SendMessageW(fidelity, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
  cmd +=
      L" +as_reflections " +
      std::to_wstring(
          SendMessageW(reflections, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
  cmd += L" +vid_vsync " +
         std::to_wstring(
             SendMessageW(vsync, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
  cmd += L" +skill " + std::to_wstring(choice(skill)) + L" +as_radius " +
         std::to_wstring(90 + choice(preset) * 40) + L" +as_damage " +
         std::to_wstring(3 + choice(preset) * 3);
  // Runtime cvars follow config loading; command-line choices retain
  // precedence.
  cmd += L" +as_renderer " + std::to_wstring(choice(renderer)) +
         L" +as_style " + std::to_wstring(choice(style));
  cmd += additionalCommand();
  cmd += L" +map " + quote(text(map));
  if (!text(extra).empty())
    cmd += L" " + text(extra);
  return cmd;
}
void update() {
  if (reflections && fidelity)
    EnableWindow(reflections,
                 choice(structure) == 11 &&
                     SendMessageW(fidelity, BM_GETCHECK, 0, 0) == BST_CHECKED);
  if (structure && style)
    EnableWindow(style, choice(structure) != 11);
  if (!loading) {
    refreshQuality();
    SetWindowTextW(preview, command().c_str());
  }
}
HWND control(const wchar_t *cls, const wchar_t *label, DWORD flags, int x,
             int y, int w, int h, int id) {
  HWND c = CreateWindowExW(cls == std::wstring(L"EDIT") ? WS_EX_CLIENTEDGE : 0,
                           cls, label, WS_CHILD | WS_VISIBLE | flags, px(x),
                           px(y), px(w), px(h), window, HMENU(INT_PTR(id)),
                           GetModuleHandleW(nullptr), nullptr);
  SendMessageW(c, WM_SETFONT, WPARAM(font), TRUE);
  return c;
}
void label(const wchar_t *s, int x, int y, int w = 180) {
  control(L"STATIC", s, 0, x, y, w, 22, 0);
}
HWND combo(int x, int y, int w, int id,
           const std::vector<const wchar_t *> &items, int selected) {
  auto c = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
                   x, y, w, 180, id);
  for (auto i : items)
    SendMessageW(c, CB_ADDSTRING, 0, LPARAM(i));
  SendMessageW(c, CB_SETCURSEL, selected, 0);
  return c;
}
std::wstring read(const wchar_t *key, const wchar_t *fallback) {
  wchar_t buf[4096];
  GetPrivateProfileStringW(L"Aftershock", key, fallback, buf, 4096,
                           ini.c_str());
  return buf;
}
void save() {
  saveSucceeded = true;
  if (selftest)
    return;
  saveAdditional();
  persist(L"Aftershock", L"data", text(data).c_str(), ini.c_str());
  persist(L"Aftershock", L"map", text(map).c_str(), ini.c_str());
  persist(L"Aftershock", L"extra", text(extra).c_str(), ini.c_str());
  persist(L"Aftershock", L"mod", text(mod).c_str(), ini.c_str());
  persist(L"Aftershock", L"fov", text(fov).c_str(), ini.c_str());
  persist(L"Aftershock", L"amount", std::to_wstring(choice(amount)).c_str(),
          ini.c_str());
  HWND controls[] = {renderer,   mode,  style,   density,   preset, skill,
                     resolution, addon, display, structure, layers};
  const wchar_t *keys[] = {L"renderer", L"mode",      L"style",      L"density",
                           L"preset",   L"skill",     L"resolution", L"addon",
                           L"display",  L"structure", L"layers"};
  for (int i = 0; i < 11; ++i)
    persist(L"Aftershock", keys[i],
            std::to_wstring(choice(controls[i])).c_str(), ini.c_str());
  HWND checks[] = {vsync, effects, shake,    reduced,    debug,
                   gibs,  goo,     fidelity, reflections};
  const wchar_t *ck[] = {L"vsync",   L"effects",  L"shake",
                         L"reduced", L"debug",    L"gibs",
                         L"goo",     L"fidelity", L"reflections"};
  for (int i = 0; i < 9; ++i)
    persist(L"Aftershock", ck[i],
            SendMessageW(checks[i], BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1"
                                                                      : L"0",
            ini.c_str());
}
void restore() {
  std::wstring savedData = read(L"data", L"");
  if (savedData.empty()) {
    savedData = findInstalledQuake();
    if (savedData.empty())
      savedData = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Quake\\id1";
  }
  SetWindowTextW(data, savedData.c_str());
  SetWindowTextW(map, read(L"map", L"e1m1").c_str());
  SetWindowTextW(extra, read(L"extra", L"").c_str());
  SetWindowTextW(mod, read(L"mod", L"").c_str());
  SetWindowTextW(fov, read(L"fov", L"100").c_str());
  SendMessageW(amount, CB_SETCURSEL,
               std::clamp(int(GetPrivateProfileIntW(L"Aftershock", L"amount", 1,
                                                    ini.c_str())),
                          0, 3),
               0);
  HWND controls[] = {renderer,   mode,  style,   density,   preset, skill,
                     resolution, addon, display, structure, layers};
  const wchar_t *keys[] = {L"renderer", L"mode",      L"style",      L"density",
                           L"preset",   L"skill",     L"resolution", L"addon",
                           L"display",  L"structure", L"layers"};
  int defaults[] = {1, 1, 0, 1, 1, 1, 0, 0, 0, 11, 2};
  for (int i = 0; i < 11; ++i) {
    int n =
        GetPrivateProfileIntW(L"Aftershock", keys[i], defaults[i], ini.c_str());
    if (n < 0 || n >= SendMessageW(controls[i], CB_GETCOUNT, 0, 0))
      n = defaults[i];
    SendMessageW(controls[i], CB_SETCURSEL, n, 0);
  }
  HWND checks[] = {vsync, effects, shake,    reduced,    debug,
                   gibs,  goo,     fidelity, reflections};
  const wchar_t *ck[] = {L"vsync",   L"effects",  L"shake",
                         L"reduced", L"debug",    L"gibs",
                         L"goo",     L"fidelity", L"reflections"};
  for (int i = 0; i < 9; ++i)
    SendMessageW(checks[i], BM_SETCHECK,
                 GetPrivateProfileIntW(L"Aftershock", ck[i],
                                       (i < 3 || i >= 5) ? 1 : 0, ini.c_str())
                     ? BST_CHECKED
                     : BST_UNCHECKED,
                 0);
}
#include "launcher_ui.inc"
bool captureLauncher(HWND target, const std::wstring &name) {
  bool ok = true;
  RECT r;
  GetClientRect(target, &r);
  int width = r.right, height = r.bottom;
  HDC dc = GetDC(target), copy = CreateCompatibleDC(dc);
  HBITMAP bitmap = CreateCompatibleBitmap(dc, width, height);
  auto old = SelectObject(copy, bitmap);
  RedrawWindow(target, nullptr, nullptr,
               RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
  PrintWindow(target, copy, PW_CLIENTONLY | 2);
  if (target == bubble)
    SendMessageW(target, WM_PRINTCLIENT, WPARAM(copy), PRF_CLIENT);
  SelectObject(copy, old);
  BITMAPINFO info{};
  info.bmiHeader = {sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB};
  std::vector<unsigned char> pixels(size_t(width) * height * 4);
  GetDIBits(copy, bitmap, 0, height, pixels.data(), &info, DIB_RGB_COLORS);
  FILE *f = _wfopen((root + L"\\" + name).c_str(), L"wb");
  if (f) {
    BITMAPFILEHEADER header{
        0x4d42,
        DWORD(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) +
              pixels.size()),
        0, 0, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)};
    fwrite(&header, sizeof(header), 1, f);
    fwrite(&info.bmiHeader, sizeof(info.bmiHeader), 1, f);
    fwrite(pixels.data(), 1, pixels.size(), f);
    fclose(f);
  } else
    ok = false;
  DeleteObject(bitmap);
  DeleteDC(copy);
  ReleaseDC(target, dc);
  return ok;
}
bool checkSettingsUI();
bool checkLauncher() {
  bool ok = text(preview) == command() && validData();
  for (const std::wstring s :
       {std::wstring(L"C:\\folder with spaces\\"), std::wstring(L"a\"b\\"),
        std::wstring(L"Quake 漢字 é")}) {
    int count = 0;
    auto args = CommandLineToArgvW((L"game.exe " + quote(s)).c_str(), &count);
    ok = ok && args && count == 2 && args[1] == s;
    if (args)
      LocalFree(args);
  }
  int previousStructure = choice(structure), previousLayers = choice(layers);
  for (int i = 0; i <= 11; ++i)
    for (int j = 0; j < 4; ++j) {
      SendMessageW(structure, CB_SETCURSEL, i, 0);
      SendMessageW(layers, CB_SETCURSEL, j, 0);
      auto cmd = command();
      ok = ok &&
           cmd.find(L" +as_structure " + std::to_wstring(i) + L" ") !=
               std::wstring::npos &&
           cmd.find(L" +as_layers " + std::to_wstring(j + 1) + L" ") !=
               std::wstring::npos;
    }
  int oldFidelity = int(SendMessageW(fidelity, BM_GETCHECK, 0, 0)),
      oldReflections = int(SendMessageW(reflections, BM_GETCHECK, 0, 0));
  for (int f = 0; f < 2; ++f)
    for (int r = 0; r < 2; ++r) {
      SendMessageW(fidelity, BM_SETCHECK, f ? BST_CHECKED : BST_UNCHECKED, 0);
      SendMessageW(reflections, BM_SETCHECK, r ? BST_CHECKED : BST_UNCHECKED,
                   0);
      std::wstring c = command();
      ok = ok &&
           c.find(L" +as_fidelity " + std::to_wstring(f) + L" ") !=
               std::wstring::npos &&
           c.find(L" +as_reflections " + std::to_wstring(r) + L" ") !=
               std::wstring::npos;
    }
  SendMessageW(fidelity, BM_SETCHECK, oldFidelity, 0);
  SendMessageW(reflections, BM_SETCHECK, oldReflections, 0);
  SendMessageW(structure, CB_SETCURSEL, previousStructure, 0);
  SendMessageW(layers, CB_SETCURSEL, previousLayers, 0);
  ok = checkSettingsUI() && ok;
  ok = captureLauncher(window, L"launcher-preview.bmp") && ok;
  FILE *f;
  f = _wfopen((root + L"\\launcher-selftest.txt").c_str(), L"wb");
  if (f) {
    std::wstring result =
        (ok ? L"PASS" : L"FAIL") +
        std::wstring(L": live preview, PACK directory bounds, Windows quoting, "
                     L"Unicode arguments; settings validation, INI round trip, "
                     L"search, presets, pages, yellow help\n") +
        command();
    int n = WideCharToMultiByte(CP_UTF8, 0, result.data(), int(result.size()),
                                nullptr, 0, nullptr, nullptr);
    std::string utf8(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, result.data(), int(result.size()),
                        utf8.data(), n, nullptr, nullptr);
    fwrite(utf8.data(), 1, utf8.size(), f);
    fclose(f);
  }
  return ok;
}

bool checkSettingsUI() {
  bool ok = true;
  loading = true;
  for (auto &s : rows)
    if (!s.key.empty())
      setSetting(s.key.c_str(), s.fallback.c_str());
  SetWindowTextW(fov, L"100");
  loading = false;
  ok = validateSettings(false) && ok;
  setSetting(L"r_holo_phys_budget", L"nan");
  ok = !validateSettings(false) && ok;
  setSetting(L"r_holo_phys_budget", L"1024.5");
  ok = !validateSettings(false) && ok;
  setSetting(L"r_holo_phys_budget", L"65536");
  setSetting(L"vid_fsaa", L"3");
  ok = !validateSettings(false) && ok;
  setSetting(L"vid_fsaa", L"0");
  SendMessageW(window, WM_COMMAND, 408, 0);
  ok = command().find(L"+as_reflection_strength 2.4 ") != std::wstring::npos &&
       ok;
  SendMessageW(window, WM_COMMAND, 409, 0);
  ok = command().find(L"+r_holo_physics 1 ") != std::wstring::npos && ok;
  for (auto &s : rows) {
    ok = !s.explanation.empty() && s.help && s.value && ok;
    if (!s.key.empty())
      ok = command().find(L" +" + s.key + L" ") != std::wstring::npos && ok;
  }
  std::wstring realIni = ini;
  ini = root + L"\\launcher-test-only.ini";
  // This dedicated test file never touches the user's configuration.
  FILE *test = _wfopen(ini.c_str(), L"wb");
  if (test) {
    unsigned short bom = 0xfeff;
    fwrite(&bom, 2, 1, test);
    fclose(test);
  } else
    ok = false;
  std::wstring before = command();
  selftest = false;
  save();
  selftest = true;
  loading = true;
  setSetting(L"r_holo_phys_budget", L"1024");
  SendMessageW(structure, CB_SETCURSEL, 0, 0);
  restore();
  restoreAdditional();
  loading = false;
  ok = command() == before && ok;
  ini = realIni;
  for (int page = 0; page < 10; ++page) {
    currentPage = page;
    scrollY = 0;
    SendMessageW(navigation, LB_SETCURSEL, page, 0);
    layoutRows();
    update();
    ok = captureLauncher(window,
                         L"launcher-page-" + std::to_wstring(page) + L".bmp") &&
         ok;
    int shown = 0;
    for (auto &s : rows)
      if (IsWindowVisible(s.value)) {
        ++shown;
        ok = s.page == page && ok;
      }
    ok = shown > 0 && ok;
    scrollY = 100000;
    layoutRows();
    RECT client;
    GetClientRect(panel, &client);
    ok = scrollY <= std::max(0, contentHeight - int(client.bottom)) && ok;
  }
  SetWindowTextW(search, L"reflection");
  layoutRows();
  int matches = 0;
  for (auto &s : rows)
    if (IsWindowVisible(s.value)) {
      ++matches;
      ok = lower(s.name + L" " + s.explanation + L" " + s.key)
                   .find(L"reflection") != std::wstring::npos &&
           ok;
    }
  ok = matches >= 3 && ok;
  SetWindowTextW(search, L"");
  currentPage = 3;
  scrollY = 0;
  SendMessageW(navigation, LB_SETCURSEL, 3, 0);
  layoutRows();
  for (size_t i = 0; i < rows.size(); ++i)
    if (rows[i].key == L"as_reflection_strength") {
      showHelp(i);
      ok = IsWindowVisible(bubble) &&
           helpText.find(L"as_reflection_strength") != std::wstring::npos && ok;
      ok = captureLauncher(bubble, L"launcher-help.bmp") && ok;
      ShowWindow(bubble, SW_HIDE);
      break;
    }
  auto oldMap = text(map), oldExtra = text(extra);
  int oldResolution = choice(resolution), oldMode = choice(mode),
      oldStyle = choice(style), oldStructure = choice(structure);
  for (int q = 1; q <= 5; ++q) {
    applyQuality(q);
    ok = validateSettings(false) && choice(quality) == q && ok;
    ok = text(map) == oldMap && text(extra) == oldExtra &&
         choice(resolution) == oldResolution && choice(mode) == oldMode &&
         choice(style) == oldStyle && choice(structure) == oldStructure && ok;
  }
  setSetting(L"r_holo_phys_budget", L"10000");
  update();
  ok = choice(quality) == 0 && ok;
  applyQuality(3);
  currentPage = 1;
  scrollY = 0;
  SendMessageW(navigation, LB_SETCURSEL, 1, 0);
  layoutRows();
  ok = captureLauncher(window, L"launcher-quality-presets.bmp") && ok;
  int nativeDpi = dpi;
  RECT previous;
  GetWindowRect(window, &previous);
  for (int testDpi : {96, 144}) {
    RECT size{previous.left, previous.top,
              previous.left + MulDiv(980, testDpi, 96),
              previous.top + MulDiv(680, testDpi, 96)};
    SendMessageW(window, WM_DPICHANGED, MAKEWPARAM(testDpi, testDpi),
                 LPARAM(&size));
    layout();
    ok = captureLauncher(window, L"launcher-dpi-" + std::to_wstring(testDpi) +
                                     L".bmp") &&
         ok;
    RECT content;
    GetClientRect(panel, &content);
    for (auto &s : rows)
      if (IsWindowVisible(s.value)) {
        RECT a, b;
        GetWindowRect(s.value, &a);
        GetWindowRect(s.help, &b);
        ok = a.right <= b.left && ok;
      }
  }
  SendMessageW(window, WM_DPICHANGED, MAKEWPARAM(nativeDpi, nativeDpi),
               LPARAM(&previous));
  layout();
  update();
  return ok;
}
LRESULT CALLBACK proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
  switch (msg) {
  case WM_CREATE: {
    window = h;
    dpi = GetDpiForWindow(h);
    font = CreateFontW(-px(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    titleFont =
        CreateFontW(-px(29), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    titleControl = control(L"STATIC", L"PARTICLE QUAKE", 0, 24, 20, 570, 42, 0);
    SendMessageW(titleControl, WM_SETFONT, WPARAM(titleFont), TRUE);
    buildPages();
    restore();
    restoreAdditional();
    layout();
    loading = false;
    update();
    if (selftest)
      SetTimer(h, 1, 200, nullptr);
    return 0;
  }
  case WM_COMMAND: {
    int id = LOWORD(w);
    if (id >= 3000 && id < 3000 + int(rows.size())) {
      showHelp(id - 3000);
      return 0;
    }
    if (HIWORD(w) == EN_SETFOCUS || HIWORD(w) == CBN_SETFOCUS ||
        HIWORD(w) == BN_SETFOCUS) {
      int y = 0;
      auto query = lower(text(search));
      for (auto &s : rows) {
        bool match = query.empty()
                         ? s.page == currentPage
                         : lower(s.name + L" " + s.explanation + L" " + s.key)
                                   .find(query) != std::wstring::npos;
        if (!match)
          continue;
        if (HWND(l) == s.value || HWND(l) == s.help) {
          RECT r;
          GetClientRect(panel, &r);
          if (y < scrollY)
            scrollY = y;
          else if (y + px(74) > scrollY + r.bottom)
            scrollY = y + px(74) - r.bottom;
          layoutRows();
          return 0;
        }
        y += px(74);
      }
    }
    ShowWindow(bubble, SW_HIDE);
    if (id == 400 && HIWORD(w) == EN_CHANGE) {
      scrollY = 0;
      layoutRows();
      return 0;
    }
    if (id == 401 && HIWORD(w) == LBN_SELCHANGE) {
      currentPage = int(SendMessageW(navigation, LB_GETCURSEL, 0, 0));
      SetWindowTextW(search, L"");
      scrollY = 0;
      layoutRows();
      return 0;
    }
    if (id == 405) {
      loading = true;
      for (auto &s : rows)
        if (s.page == currentPage) {
          if (!s.key.empty()) {
            setSetting(s.key.c_str(), s.fallback.c_str());
            continue;
          }
          if (s.value == data)
            continue;
          if (s.value == map)
            SetWindowTextW(map, L"e1m1");
          else if (s.value == fov)
            SetWindowTextW(fov, L"100");
          else if (s.value == mod || s.value == extra)
            SetWindowTextW(s.value, L"");
          else {
            wchar_t cls[32];
            GetClassNameW(s.value, cls, 32);
            if (wcscmp(cls, L"ComboBox") == 0) {
              int d = 0;
              if (s.value == renderer || s.value == mode ||
                  s.value == density || s.value == preset || s.value == skill ||
                  s.value == amount)
                d = 1;
              if (s.value == structure)
                d = 11;
              if (s.value == layers)
                d = 2;
              SendMessageW(s.value, CB_SETCURSEL, d, 0);
            } else
              SendMessageW(s.value, BM_SETCHECK,
                           (s.value == reduced || s.value == debug)
                               ? BST_UNCHECKED
                               : BST_CHECKED,
                           0);
          }
        }
      loading = false;
      update();
      return 0;
    }
    if (id == 406) {
      auto c = command();
      if (OpenClipboard(h)) {
        HGLOBAL mem =
            GlobalAlloc(GMEM_MOVEABLE, (c.size() + 1) * sizeof(wchar_t));
        if (mem) {
          void *ptr = GlobalLock(mem);
          if (ptr) {
            memcpy(ptr, c.c_str(), (c.size() + 1) * sizeof(wchar_t));
            GlobalUnlock(mem);
            EmptyClipboard();
            if (!SetClipboardData(CF_UNICODETEXT, mem))
              GlobalFree(mem);
          } else
            GlobalFree(mem);
        }
        CloseClipboard();
      }
      return 0;
    }
    if (id == 407) {
      if (validateSettings(true)) {
        save();
        SetWindowTextW(pageHint, saveSucceeded
                                     ? L"Settings saved. Ready to launch."
                                     : L"Could not save settings. Check write "
                                       L"access to aftershock.ini.");
      }
      return 0;
    }
    if (id == 410) {
      currentPage = 1;
      SetWindowTextW(search, L"");
      SendMessageW(navigation, LB_SETCURSEL, 1, 0);
      scrollY = 0;
      layoutRows();
      SetFocus(quality);
      return 0;
    }
    if (id == 411 && HIWORD(w) == CBN_SELCHANGE) {
      applyQuality(choice(quality));
      return 0;
    }
    if (id == 408 || id == 409) {
      loading = true;
      SendMessageW(renderer, CB_SETCURSEL, 1, 0);
      if (id == 408) {
        SendMessageW(structure, CB_SETCURSEL, 11, 0);
        SendMessageW(layers, CB_SETCURSEL, 2, 0);
        SendMessageW(fidelity, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(reflections, BM_SETCHECK, BST_CHECKED, 0);
        setSetting(L"as_neon_prism", L"1");
        setSetting(L"as_reflection_strength", L"2.4");
        setSetting(L"as_reflection_roughness", L"0.10");
        setSetting(L"as_neon_glow", L"1.15");
        currentPage = 3;
      } else {
        setSetting(L"r_holo_physics", L"1");
        setSetting(L"r_holo_phys_budget", L"65536");
        setSetting(L"r_holo_phys_gore_max", L"20000");
        setSetting(L"r_holo_phys_settle", L"1.2");
        setSetting(L"r_holo_phys_dust", L"0.8");
        currentPage = 5;
      }
      loading = false;
      SetWindowTextW(search, L"");
      SendMessageW(navigation, LB_SETCURSEL, currentPage, 0);
      scrollY = 0;
      layoutRows();
      update();
      return 0;
    }
    if (id == 106 && HIWORD(w) == CBN_SELCHANGE) {
      setSetting(L"as_radius",
                 std::to_wstring(90 + choice(preset) * 40).c_str());
      setSetting(L"as_damage", std::to_wstring(3 + choice(preset) * 3).c_str());
    }
    if (id == 101) {
      BROWSEINFOW b{};
      b.hwndOwner = h;
      b.lpszTitle = L"Select your Quake or id1 folder";
      b.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
      auto pidl = SHBrowseForFolderW(&b);
      if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path))
          SetWindowTextW(data, path);
        CoTaskMemFree(pidl);
      }
    } else if (id == 133) {
      std::wstring found = findInstalledQuake();
      if (!found.empty())
        SetWindowTextW(data, found.c_str());
      else
        MessageBoxW(
            h,
            L"No Steam or GOG installation of Quake was found on this "
            L"machine (registry and known library folders only - nothing "
            L"was downloaded or changed). Use Browse to point at your Quake "
            L"folder yourself, or use \"Get shareware Quake\" if you don't "
            L"own a copy.",
            L"Not found", MB_ICONINFORMATION);
    } else if (id == 134) {
      ShellExecuteW(h, L"open", L"https://archive.org/details/Quake_802",
                    nullptr, nullptr, SW_SHOWNORMAL);
    } else if (id == 113) {
      loading = true;
      HWND controls[] = {renderer,   mode,  style,   density,   preset, skill,
                         resolution, addon, display, structure, layers};
      int defaults[] = {1, 1, 0, 1, 1, 1, 0, 0, 0, 11, 2};
      for (int i = 0; i < 11; ++i)
        SendMessageW(controls[i], CB_SETCURSEL, defaults[i], 0);
      HWND checks[] = {vsync, effects, shake,    reduced,    debug,
                       gibs,  goo,     fidelity, reflections};
      for (int i = 0; i < 9; ++i)
        SendMessageW(checks[i], BM_SETCHECK,
                     (i < 3 || i >= 5) ? BST_CHECKED : BST_UNCHECKED, 0);
      SendMessageW(amount, CB_SETCURSEL, 1, 0);
      SetWindowTextW(map, L"e1m1");
      SetWindowTextW(fov, L"100");
      SetWindowTextW(extra, L"");
      SetWindowTextW(mod, L"");
      loading = false;
    } else if (id == 128) {
      loading = true;
      SendMessageW(mode, CB_SETCURSEL, 1, 0);
      SendMessageW(addon, CB_SETCURSEL, 0, 0);
      SendMessageW(preset, CB_SETCURSEL, 1, 0);
      SetWindowTextW(mod, L"");
      SetWindowTextW(map, L"e1m1");
      SetWindowTextW(
          extra, L"-test-rocket -test-panel 9 -test-scenario rocket -test-view "
                 L"-frames 540 +host_maxfps 60 +host_framerate 0");
      loading = false;
    } else if (id == 114) {
      if (!validateSettings(true))
        return 0;
      if (!validData()) {
        MessageBoxW(h,
                    L"Select a Quake folder containing a valid id1\\pak0.pak.",
                    L"Quake data required", MB_ICONERROR);
        return 0;
      }
      save();
      if (!saveSucceeded) {
        MessageBoxW(h,
                    L"Could not save aftershock.ini. Check that the "
                    L"configuration folder is writable.",
                    L"Save failed", MB_ICONERROR);
        return 0;
      }
      std::error_code error;
      std::filesystem::create_directories(userdir, error);
      if (error) {
        MessageBoxW(h, L"Cannot create the user-data folder.", L"Launch failed",
                    MB_ICONERROR);
        return 0;
      }
      std::wstring cmd = command();
      STARTUPINFOW si{sizeof(si)};
      PROCESS_INFORMATION pi{};
      if (CreateProcessW((engineDirectory + L"\\vkquake.exe").c_str(),
                         cmd.data(), nullptr, nullptr, FALSE, 0, nullptr,
                         engineDirectory.c_str(), &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
      } else {
        DWORD code = GetLastError();
        LPWSTR description = nullptr;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                           FORMAT_MESSAGE_FROM_SYSTEM |
                           FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, code, 0, reinterpret_cast<LPWSTR>(&description),
                       0, nullptr);
        std::wstring message = L"Could not launch:\n" + engineDirectory +
                               L"\\vkquake.exe\n\nWindows error " +
                               std::to_wstring(code) + L": " +
                               (description ? description : L"Unknown error");
        if (description)
          LocalFree(description);
        if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND)
          message +=
              L"\nOpen the Particle Quake Launcher shortcut in the project "
              L"folder, or place the launcher beside vkQuake.exe.";
        MessageBoxW(h, message.c_str(), L"Launch failed", MB_ICONERROR);
      }
    }
    if (id == 132 && HIWORD(w) == CBN_SELCHANGE && choice(structure) == 11) {
      SendMessageW(renderer, CB_SETCURSEL, 1, 0);
      SendMessageW(density, CB_SETCURSEL, 1, 0);
      SendMessageW(layers, CB_SETCURSEL, 2, 0);
    }
    if (id != 112)
      update();
    return 0;
  }
  case WM_TIMER:
    if (selftest) {
      KillTimer(h, 1);
      PostQuitMessage(checkLauncher() ? 0 : 2);
    }
    return 0;
  case WM_SIZE:
    layout();
    return 0;
  case WM_GETMINMAXINFO: {
    auto *mm = reinterpret_cast<MINMAXINFO *>(l);
    mm->ptMinTrackSize = {px(980), px(680)};
    return 0;
  }
  case WM_DPICHANGED: {
    dpi = HIWORD(w);
    HFONT old = font, oldTitle = titleFont;
    font =
        CreateFontW(-px(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    titleFont =
        CreateFontW(-px(27), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    EnumChildWindows(
        h,
        [](HWND c, LPARAM f) -> BOOL {
          SendMessageW(c, WM_SETFONT, WPARAM(f), TRUE);
          return TRUE;
        },
        LPARAM(font));
    SendMessageW(titleControl, WM_SETFONT, WPARAM(titleFont), TRUE);
    SendMessageW(pageTitle, WM_SETFONT, WPARAM(titleFont), TRUE);
    SendMessageW(navigation, LB_SETITEMHEIGHT, 0, px(40));
    DeleteObject(old);
    DeleteObject(oldTitle);
    RECT *r = reinterpret_cast<RECT *>(l);
    SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left,
                 r->bottom - r->top, SWP_NOZORDER);
    layout();
    return 0;
  }
  case WM_DRAWITEM: {
    auto *d = reinterpret_cast<DRAWITEMSTRUCT *>(l);
    HDC dc = d->hDC;
    RECT r = d->rcItem;
    SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    if (d->CtlID == 401) {
      HBRUSH b = CreateSolidBrush(
          d->itemState & ODS_SELECTED ? RGB(36, 77, 92) : RGB(23, 26, 31));
      FillRect(dc, &r, b);
      DeleteObject(b);
      SetTextColor(dc, d->itemState & ODS_SELECTED ? RGB(107, 234, 255)
                                                   : RGB(204, 212, 220));
      r.left += px(12);
      if (d->itemID < 10)
        DrawTextW(dc, pages[d->itemID], -1, &r,
                  DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    } else {
      FillRect(dc, &r, yellow);
      SetTextColor(dc, RGB(36, 30, 10));
      DrawTextW(dc, L"?", 1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (d->itemState & ODS_FOCUS) {
      InflateRect(&r, -2, -2);
      DrawFocusRect(dc, &r);
    }
    return TRUE;
  }
  case WM_ACTIVATE:
    if (LOWORD(w) == WA_INACTIVE)
      ShowWindow(bubble, SW_HIDE);
    break;
  case WM_CTLCOLORBTN:
  case WM_CTLCOLORSTATIC:
    SetTextColor(HDC(w), RGB(224, 226, 229));
    SetBkColor(HDC(w), RGB(23, 26, 31));
    return LRESULT(background);
  case WM_CTLCOLORLISTBOX:
    SetTextColor(HDC(w), RGB(224, 226, 229));
    SetBkColor(HDC(w), RGB(23, 26, 31));
    return LRESULT(background);
  case WM_ERASEBKGND: {
    RECT r;
    GetClientRect(h, &r);
    FillRect(HDC(w), &r, background);
    return 1;
  }
  case WM_CLOSE:
    save();
    if (!saveSucceeded) {
      MessageBoxW(h,
                  L"Could not save settings. The launcher will remain open so "
                  L"your changes are not lost.",
                  L"Save failed", MB_ICONERROR);
      return 0;
    }
    DestroyWindow(h);
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(h, msg, w, l);
}
} // namespace
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR arguments, int show) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  wchar_t path[32768];
  GetModuleFileNameW(nullptr, path, 32768);
  root = std::filesystem::path(path).parent_path().wstring();
  engineDirectory = findEngineDirectory(root);
  ini = root + L"\\aftershock.ini";
  selftest = wcsstr(arguments, L"--selftest") != nullptr;
  userdir = root + L"\\userdata";
  std::wstring probe =
      root + L"\\.write-test-" + std::to_wstring(GetCurrentProcessId());
  HANDLE writable = CreateFileW(
      probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
      FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
  if (writable != INVALID_HANDLE_VALUE)
    CloseHandle(writable);
  else {
    PWSTR local = nullptr;
    if (SUCCEEDED(
            SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) {
      std::filesystem::path config =
          std::filesystem::path(local) / L"ParticleQuake";
      CoTaskMemFree(local);
      std::error_code error;
      std::filesystem::create_directories(config, error);
      ini = (config / L"aftershock.ini").wstring();
      userdir = (config / L"userdata").wstring();
    }
  }
  background = CreateSolidBrush(RGB(23, 26, 31));
  WNDCLASSW wc{};
  wc.lpfnWndProc = proc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = background;
  wc.lpszClassName = L"AftershockLauncher";
  RegisterClassW(&wc);
  dpi = GetDpiForSystem();
  RECT bounds{0, 0, px(1100), px(820)};
  AdjustWindowRectExForDpi(&bounds, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi);
  MONITORINFO monitor{sizeof(monitor)};
  GetMonitorInfoW(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY),
                  &monitor);
  int initialWidth = std::min(int(bounds.right - bounds.left),
                              int(monitor.rcWork.right - monitor.rcWork.left));
  int initialHeight = std::min(int(bounds.bottom - bounds.top),
                               int(monitor.rcWork.bottom - monitor.rcWork.top));
  HWND h = CreateWindowW(wc.lpszClassName, L"Particle Quake: Aftershock",
                         WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                         initialWidth, initialHeight, nullptr, nullptr,
                         instance, nullptr);
  if (!h)
    return 1;
  ShowWindow(h, selftest ? SW_SHOWNOACTIVATE : show);
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(h, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  DeleteObject(font);
  DeleteObject(titleFont);
  DeleteObject(background);
  DeleteObject(yellow);
  CoUninitialize();
  return int(msg.wParam);
}
