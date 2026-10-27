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
#include <wil/resource.h>
#include <wil/win32_result_macros.h>
#include <windows.h>

namespace Kbi {

using namespace GameInput::v3;
using namespace std::chrono;

namespace {

IGameInput* gameInputInit()
{
    static std::mutex initMutex;
    static wil::com_ptr_t<IGameInput> gameinput;
    using GameInputCreate_t = HRESULT(IUnknown**);

    std::scoped_lock lock(initMutex);
    if (gameinput)
        return gameinput.get();

    // The module must stay loaded for as long as the COM object is alive, so
    // it is kept in a static RAII owner instead of a leaking raw handle
    // (R.12).
    static wil::unique_hmodule gameInputDll(LoadLibraryW(L"gameinput.dll"));
    THROW_LAST_ERROR_IF_NULL_MSG(gameInputDll.get(), "Failed to load the GameInput dll");

    auto createFunc = reinterpret_cast<GameInputCreate_t*>(
        GetProcAddress(gameInputDll.get(), "GameInputCreate"));
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

constexpr auto sortByScancode = [](const GameInputKeyState& a, const GameInputKeyState& b) {
    return a.scanCode < b.scanCode;
};

void gameInputReadingCallback(
    GameInputCallbackToken /*token*/, void* context, IGameInputReading* reading)
{
    wil::com_ptr<IGameInputDevice> device = nullptr;
    reading->GetDevice(&device);
    if (!device)
        return;
    const GameInputDeviceInfo* deviceInfo = nullptr;
    if (FAILED(device->GetDeviceInfo(&deviceInfo)) || !deviceInfo) {
        return;
    }

    const std::string pnp = deviceInfo->pnpPath;
    const VidPid ids { deviceInfo->vendorId, deviceInfo->productId };

    const auto timestamp = reading->GetTimestamp();
    const auto inputKind = reading->GetInputKind();
    if (inputKind & GameInputKind::GameInputKindKeyboard) {
        KeyStateArray state;
        auto stateLen = reading->GetKeyCount();
        state.resize(stateLen);
        reading->GetKeyState(stateLen, state.data());
        std::sort(state.begin(), state.end(), sortByScancode);
        reinterpret_cast<RecorderWinGameInput*>(context)->updateKeyStates(
            pnp, ids, timestamp, state);
    }
}

} // namespace

void RecorderWinGameInput::updateKeyStates(
    const std::string& pnp, const VidPid& ids, std::uint64_t timestamp, const KeyStateArray& state)
{
    auto& statePrev = m_keyStates[pnp];
    KeyStateArray pressed;
    KeyStateArray released;
    std::set_difference(statePrev.begin(), statePrev.end(), state.begin(), state.end(),
        std::back_inserter(released), sortByScancode);
    std::set_difference(state.begin(), state.end(), statePrev.begin(), statePrev.end(),
        std::back_inserter(pressed), sortByScancode);
    for (const auto& i : pressed) {
        onInput()(pnp, ids,
            Input { .timestamp = timestamp - m_timestampRef,
                .pressed = true,
                .code = vkToKeycode(i.virtualKey) });
    }
    for (const auto& i : released) {
        onInput()(pnp, ids,
            Input { .timestamp = timestamp - m_timestampRef,
                .pressed = false,
                .code = vkToKeycode(i.virtualKey) });
    }
    statePrev = state;
}

RecorderWinGameInput::RecorderWinGameInput(std::shared_ptr<spdlog::logger> logger)
    : Recorder::Impl(std::move(logger))
    , m_gameinput(gameInputInit())
{
}

void RecorderWinGameInput::start(InputMask devices)
{
    GameInputKind kind = GameInputKind::GameInputKindUnknown;
    if (has(devices, InputMask::Keyboard))
        kind |= GameInputKind::GameInputKindKeyboard;
    if (has(devices, InputMask::Mouse))
        kind |= GameInputKind::GameInputKindMouse;
    if (has(devices, InputMask::Gamepad))
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

RecorderWinGameInput::~RecorderWinGameInput()
{
    // Unregister the reading callback and stop the dispatcher thread even if
    // the recorder is destroyed without an explicit stop() (R.1); stop() is
    // idempotent.
    stop();
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
