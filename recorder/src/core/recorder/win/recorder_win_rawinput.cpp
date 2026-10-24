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

} // namespace

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
    }
    default:
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}

std::pair<std::uint16_t, std::uint16_t> vidPidFromPnp(std::string_view s)
{
    std::uint16_t vid = 0;
    std::uint16_t pid = 0;
    if (auto pos = s.find("VID_"); pos != std::string::npos)
        std::from_chars(s.data() + pos + 4, s.data() + pos + 8, vid, 16);
    if (auto pos = s.find("PID_"); pos != std::string::npos)
        std::from_chars(s.data() + pos + 4, s.data() + pos + 8, pid, 16);
    return { vid, pid };
}

RecorderWinRawInput::RecorderWinRawInput(std::shared_ptr<spdlog::logger> logger)
    : Recorder::Impl(std::move(logger))
{
}

void RecorderWinRawInput::processRawInput(HRAWINPUT rawInput)
{
    auto now = steady_clock::now();
    UINT dwSize;
    GetRawInputData(rawInput, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
    std::vector<BYTE> data(dwSize);
    if (std::cmp_not_equal(
            GetRawInputData(rawInput, RID_INPUT, data.data(), &dwSize, sizeof(RAWINPUTHEADER)),
            dwSize))
        return;

    auto* raw = reinterpret_cast<RAWINPUT*>(data.data());
    UINT strSize;
    GetRawInputDeviceInfoW(raw->header.hDevice, RIDI_DEVICENAME, nullptr, &strSize);
    std::wstring devicePath(strSize, 0);
    GetRawInputDeviceInfoW(raw->header.hDevice, RIDI_DEVICENAME, devicePath.data(), &strSize);
    if (raw->header.dwType == RIM_TYPEKEYBOARD) {
        auto devicePathStr = wstringToString(devicePath);
        auto pnp = devicePathStr.substr(4, /* Remove \\?\ */
            devicePathStr.find("#{") - 4 /* Remove GUID at end */
        );
        for (auto& c : pnp) {
            if (c == '#')
                c = '\\';
        }
        RAWKEYBOARD& keyboard = raw->data.keyboard;
        bool released = (keyboard.Flags & RI_KEY_BREAK) != 0;
        if (keyboard.MakeCode == KEYBOARD_OVERRUN_MAKE_CODE || keyboard.VKey >= UCHAR_MAX)
            return;
        auto [vid, pid] = vidPidFromPnp(pnp);
        onInput()(pnp, vid, pid,
            Input { .timestamp
                = static_cast<std::uint64_t>(duration_cast<microseconds>(now - m_startRef).count()),
                .pressed = !released,
                .code = vkToKeycode(keyboard.VKey) });
    }
}

void RecorderWinRawInput::start(bool keyboard, bool mouse, bool gamepad)
{
    m_windowThread = std::jthread([=, this](const std::stop_token& stop) {
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
        if (keyboard) {
            rawInputDevices.push_back({ .usUsagePage = 0x01,
                .usUsage = 0x06,
                .dwFlags = static_cast<DWORD>(RIDEV_NOLEGACY | RIDEV_INPUTSINK | RIDEV_NOHOTKEYS),
                .hwndTarget = m_hwnd });
        }
        if (mouse) {
            rawInputDevices.push_back({ .usUsagePage = 0x01,
                .usUsage = 0x02,
                .dwFlags = static_cast<DWORD>(RIDEV_NOLEGACY | RIDEV_INPUTSINK),
                .hwndTarget = m_hwnd });
        }
        if (gamepad) {
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
    m_startRef = steady_clock::now();
}

void RecorderWinRawInput::stop()
{
    m_windowThread.request_stop();
    if (m_windowThread.joinable())
        m_windowThread.join();
    if (m_hwnd != nullptr) {
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
