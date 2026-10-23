#include "device_name.h"
#include "impl.h"
#include "vk_to_keycode.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <spdlog/spdlog.h>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <wil/com.h>
#include <wil/win32_result_macros.h>
#include <windows.h>

namespace Kbi {

using namespace GameInput::v3;
using namespace std::chrono;

IGameInput* gameInputInit()
{
    static std::mutex initMutex;
    static wil::com_ptr_t<IGameInput> gameinput;
    using GameInputCreate_t = HRESULT(IUnknown**);

    std::scoped_lock lock(initMutex);
    if (gameinput)
        return gameinput.get();

    HMODULE gameInputDll = LoadLibraryW(L"gameinput.dll");
    THROW_LAST_ERROR_IF_NULL_MSG(gameInputDll, "Failed to load the GameInput dll");

    auto createFunc = reinterpret_cast<GameInputCreate_t*>(
        GetProcAddress(gameInputDll, "GameInputCreate"));
    THROW_LAST_ERROR_IF_NULL_MSG(createFunc, "Failed to load the GameInput factory function");

    IUnknown* unknown = nullptr;
    HRESULT hr = createFunc(&unknown);
    THROW_IF_FAILED_MSG(hr, "Failed to initialize GameInput");

    hr = unknown->QueryInterface(IID_PPV_ARGS(&gameinput));
    unknown->Release();
    THROW_IF_FAILED_MSG(hr, "Failed to initialize GameInput");

    gameinput->SetFocusPolicy(GameInputDefaultFocusPolicy);
    return gameinput.get();
}

bool sortByScancode(const GameInputKeyState& a, const GameInputKeyState& b)
{
    return a.scanCode < b.scanCode;
}

void RecorderWinGameInput::updateKeyStates(
    const std::string& pnp, std::uint16_t vid, std::uint16_t pid,
    std::uint64_t timestamp, const KeyStateArray& state)
{
    auto& statePrev = m_keyStates[pnp];
    KeyStateArray pressed;
    KeyStateArray released;
    std::set_difference(
        statePrev.begin(), statePrev.end(),
        state.begin(), state.end(),
        std::back_inserter(released),
        sortByScancode);
    std::set_difference(
        state.begin(), state.end(),
        statePrev.begin(), statePrev.end(),
        std::back_inserter(pressed),
        sortByScancode);
    for (auto& i : pressed) {
        onInput()(pnp, vid, pid, Input { .timestamp = timestamp - m_timestampRef, .pressed = true, .code = vkToKeycode(i.virtualKey) });
    }
    for (auto& i : released) {
        onInput()(pnp, vid, pid, Input { .timestamp = timestamp - m_timestampRef, .pressed = false, .code = vkToKeycode(i.virtualKey) });
    }
    statePrev = state;
}

RecorderWinGameInput::RecorderWinGameInput(std::shared_ptr<spdlog::logger> logger)
    : Recorder::Impl(std::move(logger))
{
    m_gameinput = gameInputInit();
}

void gameInputReadingCallback(
    GameInputCallbackToken token,
    void* context,
    IGameInputReading* reading)
{
    IGameInputDevice* device = nullptr;
    const GameInputDeviceInfo* deviceInfo = nullptr;
    reading->GetDevice(&device);
    device->GetDeviceInfo(&deviceInfo);

    std::string pnp = deviceInfo->pnpPath;
    std::uint16_t vid = deviceInfo->vendorId;
    std::uint16_t pid = deviceInfo->productId;

    auto timestamp = reading->GetTimestamp();
    auto inputKind = reading->GetInputKind();
    if (inputKind & GameInputKind::GameInputKindKeyboard) {
        KeyStateArray state;
        auto stateLen = reading->GetKeyCount();
        state.resize(stateLen);
        reading->GetKeyState(stateLen, state.data());
        std::sort(state.begin(), state.end(), sortByScancode);
        reinterpret_cast<RecorderWinGameInput*>(context)->updateKeyStates(
            pnp, vid, pid, timestamp, state);
    }

    device->Release();
}

void RecorderWinGameInput::start(bool keyboard, bool mouse, bool gamepad)
{
    GameInputKind kind = GameInputKind::GameInputKindUnknown;
    if (keyboard)
        kind |= GameInputKind::GameInputKindKeyboard;
    if (mouse)
        kind |= GameInputKind::GameInputKindMouse;
    if (gamepad)
        kind |= GameInputKind::GameInputKindGamepad;
    m_timestampRef = m_gameinput->GetCurrentTimestamp();
    m_keyStates.clear();
    m_dispatcherThread = std::jthread([&](const std::stop_token& stop) {
        IGameInputDispatcher* dispatcher;
        if (FAILED(m_gameinput->CreateDispatcher(&dispatcher)))
            return;
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        SetThreadAffinityMask(GetCurrentThread(), 1);
        HANDLE waitHandle = nullptr;
        dispatcher->OpenWaitHandle(&waitHandle);
        while (!stop.stop_requested()) {
            if (waitHandle) {
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
        nullptr, kind, this, gameInputReadingCallback, &m_callbackToken);
}

void RecorderWinGameInput::stop()
{
    m_gameinput->UnregisterCallback(m_callbackToken);
    m_dispatcherThread.request_stop();
    if (m_dispatcherThread.joinable())
        m_dispatcherThread.join();
}

std::string RecorderWinGameInput::deviceName(std::string_view pnp) const
{
    return deviceNameFromPnp(pnp);
}

std::optional<std::string> RecorderWinGameInput::usbDeviceId(std::string_view id) const
{
    return findUsbDevice(id);
}

std::optional<UsbDeviceInfo> RecorderWinGameInput::usbDeviceInfo(std::string_view id) const
{
    return usbDeviceInfoFromPath(id);
}

} // namespace Kbi
