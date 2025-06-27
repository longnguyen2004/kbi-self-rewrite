#include "impl.h"
#include "device_name.h"
#include "vk_to_keycode.h"

#include <spdlog/spdlog.h>
#include <wil/win32_result_macros.h>
#include <wil/com.h>
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <stop_token>
#include <type_traits>

using namespace GameInput::v1;
using namespace std::chrono;
using namespace std::literals;

IGameInput* gameinput_init()
{
    static std::mutex mtx_init;
    static wil::com_ptr_t<IGameInput> gameinput;
    using GameInputCreate_t = HRESULT(IUnknown** );

    std::lock_guard lck(mtx_init);
    if (gameinput)
        return gameinput.get();

    HMODULE gameinput_dll = LoadLibraryW(L"gameinput.dll");
    THROW_LAST_ERROR_IF_NULL_MSG(gameinput_dll, "Failed to load the GameInput dll");

    auto create_func = reinterpret_cast<GameInputCreate_t*>(
        GetProcAddress(gameinput_dll, "GameInputCreate")
    );
    THROW_LAST_ERROR_IF_NULL_MSG(create_func, "Failed to load the GameInput factory function");

    IUnknown* unknown = nullptr;
    HRESULT hr = create_func(&unknown);
    THROW_IF_FAILED_MSG(hr, "Failed to initialize GameInput");

    hr = unknown->QueryInterface(IID_PPV_ARGS(&gameinput));
    unknown->Release();
    THROW_IF_FAILED_MSG(hr, "Failed to initialize GameInput");

    gameinput->SetFocusPolicy(GameInputDefaultFocusPolicy);
    return gameinput.get();
}

bool sort_by_scancode(const GameInputKeyState& a, const GameInputKeyState& b)
{
    return a.scanCode < b.scanCode;
}

void recorder_win_gameinput::_update_key_states(
    const std::string& pnp, std::uint16_t vid, std::uint16_t pid,
    std::uint64_t timestamp, const KeyStateArray& state
)
{
    auto& state_prev = m_key_states[pnp];
    KeyStateArray pressed;
    KeyStateArray released;
    std::set_difference(
        state_prev.begin(), state_prev.end(),
        state.begin(), state.end(),
        std::back_inserter(released),
        sort_by_scancode
    );
    std::set_difference(
        state.begin(), state.end(),
        state_prev.begin(), state_prev.end(),
        std::back_inserter(pressed),
        sort_by_scancode
    );
    for (auto& i : pressed)
    {
        OnInput()(pnp, vid, pid, Input{
            .Timestamp = timestamp - m_timestamp_ref,
            .Pressed = true,
            .Code = static_cast<Keycode>(vk_to_keycode(i.virtualKey))
        });
    }
    for (auto& i : released)
    {
        OnInput()(pnp, vid, pid, Input{
            .Timestamp = timestamp - m_timestamp_ref,
            .Pressed = false,
            .Code = static_cast<Keycode>(vk_to_keycode(i.virtualKey))
        });
    }
    state_prev = state;
}

recorder_win_gameinput::recorder_win_gameinput(std::shared_ptr<spdlog::logger> logger):
    Recorder::Impl(logger)
{
    m_gameinput = gameinput_init();
}

void _gameinput_reading_callback(
    GameInputCallbackToken token,
    void* context,
    IGameInputReading* reading
)
{
    IGameInputDevice *device = nullptr;
    const GameInputDeviceInfo *device_info = nullptr;
    reading->GetDevice(&device);
    device->GetDeviceInfo(&device_info);

    std::string pnp = device_info->pnpPath;
    std::uint16_t vid = device_info->vendorId;
    std::uint16_t pid = device_info->productId;

    auto timestamp = reading->GetTimestamp();
    auto input_kind = reading->GetInputKind();
    if (input_kind & GameInputKind::GameInputKindKeyboard)
    {
        KeyStateArray state;
        auto state_len = reading->GetKeyCount();
        state.resize(state_len);
        reading->GetKeyState(state_len, state.data());
        std::sort(state.begin(), state.end(), sort_by_scancode);
        reinterpret_cast<recorder_win_gameinput*>(context)->_update_key_states(
            pnp, vid, pid, timestamp, state
        );
    }

    device->Release();
}

void recorder_win_gameinput::Start(bool keyboard, bool mouse, bool gamepad)
{
    GameInputKind kind = GameInputKind::GameInputKindUnknown;
    if (keyboard)
        kind |= GameInputKind::GameInputKindKeyboard;
    if (mouse)
        kind |= GameInputKind::GameInputKindMouse;
    if (gamepad)
        kind |= GameInputKind::GameInputKindGamepad;
    m_timestamp_ref = m_gameinput->GetCurrentTimestamp();
    m_key_states.clear();
    m_dispatcher_thread = std::jthread([&](const std::stop_token& stop) {
        IGameInputDispatcher *dispatcher;
        if (FAILED(m_gameinput->CreateDispatcher(&dispatcher)))
            return;
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        SetThreadAffinityMask(GetCurrentThread(), 1);
        HANDLE waitHandle = nullptr;
        dispatcher->OpenWaitHandle(&waitHandle);
        while (!stop.stop_requested())
        {
            if (waitHandle)
            {
                auto res = WaitForSingleObject(waitHandle, 100);
                if (res != 0)
                    continue;
            }
            dispatcher->Dispatch(1000);
        }
        if (waitHandle)
            CloseHandle(waitHandle);
        dispatcher->Release();
    });
    m_gameinput->RegisterReadingCallback(
        nullptr, kind, this, _gameinput_reading_callback, &m_callback_token
    );
}

void recorder_win_gameinput::Stop()
{
    m_gameinput->UnregisterCallback(m_callback_token);
    m_dispatcher_thread.request_stop();
    if (m_dispatcher_thread.joinable())
        m_dispatcher_thread.join();
}

std::string recorder_win_gameinput::GetDeviceName(std::string_view pnp) const
{
    return device_name_from_pnp(pnp);
}

std::optional<std::string> recorder_win_gameinput::GetUsbDeviceId(std::string_view id) const
{
    return find_usb_device(id);
}

std::optional<UsbDeviceInfo> recorder_win_gameinput::GetUsbDeviceInfo(std::string_view id) const
{
    return get_usb_device_info(id);
}
