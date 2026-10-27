#include "device_name.h"
#include "impl.h"
#include "wstring.h"
#include <charconv>
#include <string_view>
#include <utility>
#include <wil/result.h>

#include "vk_to_keycode.h"

namespace Kbi {

using namespace std::chrono;

namespace {

const wchar_t* const className = L"ListenerWindow";

LRESULT CALLBACK wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
    case WM_PAINT:
    case WM_SIZE:
    case WM_DESTROY:
        return 0;

    case WM_INPUT: {
        // The instance is stored in the user data of the window, which
        // Win32 only offers as an integer.
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        auto* recorderInstance = reinterpret_cast<RecorderWinRawInput*>(GetWindowLongPtrW(hwnd, 0));
        recorderInstance->processRawInput(
            // The WM_INPUT message carries the HRAWINPUT in lParam.
            // NOLINTNEXTLINE(performance-no-int-to-ptr)
            reinterpret_cast<HRAWINPUT>(lParam));
        // WM_INPUT is handled here and everything else is passed on to the
        // default procedure.
        [[fallthrough]];
    }
    default:
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}

VidPid vidPidFromPnp(std::string_view s)
{
    std::uint16_t vid = 0;
    std::uint16_t pid = 0;
    // Only parse if the four hex digits are fully present; from_chars leaves
    // the value untouched on error, which keeps the zero default (P.7).
    if (auto pos = s.find("VID_"); pos != std::string_view::npos && pos + 8 <= s.size()) {
        if (std::from_chars(s.data() + pos + 4, s.data() + pos + 8, vid, 16).ec != std::errc {})
            vid = 0;
    }
    if (auto pos = s.find("PID_"); pos != std::string_view::npos && pos + 8 <= s.size()) {
        if (std::from_chars(s.data() + pos + 4, s.data() + pos + 8, pid, 16).ec != std::errc {})
            pid = 0;
    }
    return { vid, pid };
}

} // namespace

RecorderWinRawInput::RecorderWinRawInput(std::shared_ptr<spdlog::logger> logger)
    : Recorder::Impl(std::move(logger))
{
}

void RecorderWinRawInput::processRawInput(HRAWINPUT rawInput)
{
    const auto now = steady_clock::now();
    // The result of the size query is checked before it is used as a buffer
    // size; both calls fail with (UINT)-1 (E.27, ES.20).
    UINT dwSize = 0;
    if (GetRawInputData(rawInput, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER))
        == static_cast<UINT>(-1))
        return;
    std::vector<BYTE> data(dwSize);
    if (std::cmp_not_equal(
            GetRawInputData(rawInput, RID_INPUT, data.data(), &dwSize, sizeof(RAWINPUTHEADER)),
            dwSize))
        return;

    auto* raw = reinterpret_cast<RAWINPUT*>(data.data());
    UINT strSize = 0;
    if (GetRawInputDeviceInfoW(raw->header.hDevice, RIDI_DEVICENAME, nullptr, &strSize)
        == static_cast<UINT>(-1))
        return;
    std::wstring devicePath(strSize, 0);
    if (GetRawInputDeviceInfoW(raw->header.hDevice, RIDI_DEVICENAME, devicePath.data(), &strSize)
        == static_cast<UINT>(-1))
        return;
    if (raw->header.dwType == RIM_TYPEKEYBOARD) {
        auto devicePathStr = wstringToString(devicePath);
        // The path is expected to look like \\?\HID#VID_...#{guid}; validate
        // the shape before slicing it up (P.7).
        const auto guidPos = devicePathStr.find("#{");
        if (guidPos == std::string::npos || guidPos < 4)
            return;
        auto pnp = devicePathStr.substr(4, guidPos - 4 /* Remove GUID at end */);
        for (auto& c : pnp) {
            if (c == '#')
                c = '\\';
        }
        RAWKEYBOARD& keyboard = raw->data.keyboard;
        const bool released = (keyboard.Flags & RI_KEY_BREAK) != 0;
        if (keyboard.MakeCode == KEYBOARD_OVERRUN_MAKE_CODE || keyboard.VKey >= UCHAR_MAX)
            return;
        const VidPid ids = vidPidFromPnp(pnp);
        onInput()(pnp, ids,
            Input { .timestamp
                = static_cast<std::uint64_t>(duration_cast<microseconds>(now - m_startRef).count()),
                .pressed = !released,
                .code = vkToKeycode(keyboard.VKey) });
    }
}

void RecorderWinRawInput::start(InputMask devices)
{
    // Publish the timestamp reference before the window thread starts; the
    // thread reads it from WM_INPUT dispatch without further synchronization
    // (CP.2).
    m_startRef = steady_clock::now();
    m_windowThread = std::jthread([this, devices](const std::stop_token& stop) {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        SetThreadAffinityMask(GetCurrentThread(), 1);
        WNDCLASSW wndClass = {
            .lpfnWndProc = wndProc,
            .cbWndExtra = 8,
            .hInstance = GetModuleHandleW(nullptr),
            .lpszClassName = className,
        };
        RegisterClassW(&wndClass);
        m_hwnd = CreateWindowExW(0, className, nullptr, 0, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT,
            // HWND_MESSAGE is a macro that casts -1 to a window handle.
            // NOLINTNEXTLINE(performance-no-int-to-ptr)
            HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
        SetWindowLongPtrW(m_hwnd, 0, reinterpret_cast<LONG_PTR>(this));
        std::vector<RAWINPUTDEVICE> rawInputDevices;
        if (has(devices, InputMask::Keyboard)) {
            rawInputDevices.push_back({ .usUsagePage = 0x01,
                .usUsage = 0x06,
                .dwFlags = static_cast<DWORD>(RIDEV_NOLEGACY | RIDEV_INPUTSINK | RIDEV_NOHOTKEYS),
                .hwndTarget = m_hwnd });
        }
        if (has(devices, InputMask::Mouse)) {
            rawInputDevices.push_back({ .usUsagePage = 0x01,
                .usUsage = 0x02,
                .dwFlags = static_cast<DWORD>(RIDEV_NOLEGACY | RIDEV_INPUTSINK),
                .hwndTarget = m_hwnd });
        }
        if (has(devices, InputMask::Gamepad)) {
            rawInputDevices.push_back({ .usUsagePage = 0x01,
                .usUsage = 0x04,
                .dwFlags = static_cast<DWORD>(RIDEV_INPUTSINK),
                .hwndTarget = m_hwnd });
        }
        if (!RegisterRawInputDevices(
                rawInputDevices.data(), rawInputDevices.size(), sizeof(RAWINPUTDEVICE)))
            LOG_LAST_ERROR();
        MSG msg;
        while (!stop.stop_requested()) {
            if (GetMessageW(&msg, m_hwnd, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            } else {
                LOG_LAST_ERROR();
            }
        }
    });
}

RecorderWinRawInput::~RecorderWinRawInput()
{
    // Release the window and window class even if the recorder is destroyed
    // without an explicit stop() (R.1); stop() is idempotent.
    stop();
}

void RecorderWinRawInput::stop()
{
    m_windowThread.request_stop();
    if (m_windowThread.joinable())
        m_windowThread.join();
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    UnregisterClassW(className, GetModuleHandleW(nullptr));
}

std::string RecorderWinRawInput::deviceName(std::string_view pnp) const
{
    return deviceNameFromPnp(pnp);
}

std::optional<std::string> RecorderWinRawInput::usbDeviceId(std::string_view id) const
{
    return findUsbDevice(id);
}

std::optional<UsbDeviceInfo> RecorderWinRawInput::usbDeviceInfo(std::string_view id) const
{
    return usbDeviceInfoFromPath(id);
}

} // namespace Kbi
