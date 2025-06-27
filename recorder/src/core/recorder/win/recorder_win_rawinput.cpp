#include "impl.h"
#include "device_name.h"
#include "wstring.h"
#include <wil/result.h>
#include <charconv>
#include <string_view>
#include <utility>
#include <iostream>

#include "vk_to_keycode.h"

using namespace std::chrono;

static const wchar_t *CLASS_NAME = L"ListenerWindow";

LRESULT CALLBACK WndProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (uMsg)
    {
    case WM_CREATE:
    case WM_PAINT:
    case WM_SIZE:
    case WM_DESTROY:
        return 0;

    case WM_INPUT:
    {
        auto recorderInstance = reinterpret_cast<recorder_win_rawinput*>(
            GetWindowLongPtrW(hwnd, 0)
        );
        recorderInstance->_processRawInput(
            reinterpret_cast<HRAWINPUT>(lParam)
        );
    }
    default:
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}

std::pair<std::uint16_t, std::uint16_t> vid_pid_from_pnp(std::string_view s)
{
    std::uint16_t vid = 0, pid = 0;
    if (auto pos = s.find("VID_"); pos != std::string::npos)
        std::from_chars(s.data() + pos + 4, s.data() + pos + 8, vid, 16);
    if (auto pos = s.find("PID_"); pos != std::string::npos)
        std::from_chars(s.data() + pos + 4, s.data() + pos + 8, pid, 16);
    return { vid, pid };
}

recorder_win_rawinput::recorder_win_rawinput(std::shared_ptr<spdlog::logger> logger):
    Recorder::Impl(logger)
{
}

void recorder_win_rawinput::_processRawInput(HRAWINPUT rawInput)
{
    auto now = steady_clock::now();
    UINT dwSize;
    GetRawInputData(rawInput, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
    std::vector<BYTE> data(dwSize);
    if (GetRawInputData(rawInput, RID_INPUT, data.data(), &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
        return;
    
    auto *raw = reinterpret_cast<RAWINPUT*>(data.data());
    UINT strSize;
    GetRawInputDeviceInfoW(raw->header.hDevice, RIDI_DEVICENAME, nullptr, &strSize);
    std::wstring devicePath(strSize, 0);
    GetRawInputDeviceInfoW(raw->header.hDevice, RIDI_DEVICENAME, devicePath.data(), &strSize);
    if (raw->header.dwType == RIM_TYPEKEYBOARD)
    {
        auto devicePathStr = wstring_to_string(devicePath);
        auto pnp = devicePathStr.substr(
            4,                           /* Remove \\?\ */
            devicePathStr.find("#{") - 4 /* Remove GUID at end */
        );
        for (auto& c: pnp)
        {
            if (c == '#')
                c = '\\';
        }
        RAWKEYBOARD& keyboard = raw->data.keyboard;
        bool released = keyboard.Flags & RI_KEY_BREAK;
        if (keyboard.MakeCode == KEYBOARD_OVERRUN_MAKE_CODE || keyboard.VKey >= UCHAR_MAX)
            return;
        auto [ vid, pid ] = vid_pid_from_pnp(pnp);
        this->OnInput()(pnp, vid, pid, Input{
            .Timestamp = static_cast<std::uint64_t>(duration_cast<microseconds>(now - m_start_ref).count()),
            .Pressed = !released,
            .Code = vk_to_keycode(keyboard.VKey)
        });
    }
}

void recorder_win_rawinput::Start(bool keyboard, bool mouse, bool gamepad)
{
    m_window_thread = std::jthread([=, this](const std::stop_token& stop) {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        SetThreadAffinityMask(GetCurrentThread(), 1);
        WNDCLASSW wnd_class = {
            .lpfnWndProc = WndProc,
            .cbWndExtra = 8,
            .hInstance = GetModuleHandleW(nullptr),
            .lpszClassName = CLASS_NAME,
        };
        RegisterClassW(&wnd_class);
        m_hwnd = CreateWindowExW(
            0,
            CLASS_NAME,
            nullptr,
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            HWND_MESSAGE,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr
        );
        SetWindowLongPtrW(m_hwnd, 0, reinterpret_cast<LONG_PTR>(this));
        std::vector<RAWINPUTDEVICE> rawInputDevices;
        if (keyboard)
        {
            rawInputDevices.push_back({
                .usUsagePage = 0x01,
                .usUsage = 0x06,
                .dwFlags = (DWORD)(RIDEV_NOLEGACY | RIDEV_INPUTSINK | RIDEV_NOHOTKEYS),
                .hwndTarget = m_hwnd
            });
            std::cout << keyboard << '\n';
        }
        if (mouse)
        {
            rawInputDevices.push_back({
                .usUsagePage = 0x01,
                .usUsage = 0x02,
                .dwFlags = (DWORD)(RIDEV_NOLEGACY | RIDEV_INPUTSINK),
                .hwndTarget = m_hwnd
            });
        }
        if (gamepad)
        {
            rawInputDevices.push_back({
                .usUsagePage = 0x01,
                .usUsage = 0x04,
                .dwFlags = (DWORD)(RIDEV_INPUTSINK),
                .hwndTarget = m_hwnd
            });
        }
        if (!RegisterRawInputDevices(
            rawInputDevices.data(),
            rawInputDevices.size(),
            sizeof(RAWINPUTDEVICE)
        ))
            LOG_LAST_ERROR();
        MSG msg;
        while (!stop.stop_requested())
        {
            if (GetMessageW(&msg, m_hwnd, 0, 0))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            else
            {
                LOG_LAST_ERROR();
            }
        }
    });
    m_start_ref = steady_clock::now();
}

void recorder_win_rawinput::Stop()
{
    m_window_thread.request_stop();
    if (m_window_thread.joinable())
        m_window_thread.join();
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    UnregisterClassW(CLASS_NAME, GetModuleHandleW(nullptr));
}

std::string recorder_win_rawinput::GetDeviceName(std::string_view pnp) const
{
    return device_name_from_pnp(pnp);
}

std::optional<std::string> recorder_win_rawinput::GetUsbDeviceId(std::string_view id) const
{
    return find_usb_device(id);
}

std::optional<UsbDeviceInfo> recorder_win_rawinput::GetUsbDeviceInfo(std::string_view id) const
{
    return get_usb_device_info(id);
}
