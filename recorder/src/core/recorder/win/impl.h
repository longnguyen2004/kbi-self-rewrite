#pragma once

#include "../recorder_impl.h"
#include <boost/container/static_vector.hpp>
#include <chrono>
#include <cstdint>
#include <gameinput.h>
#include <memory>
#include <optional>
#include <spdlog/fwd.h>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <windows.h>

namespace Kbi {

using KeyStateArray = boost::container::static_vector<GameInput::v3::GameInputKeyState, 50>;

class RecorderWinGameInput : public Recorder::Impl {
public:
    explicit RecorderWinGameInput(std::shared_ptr<spdlog::logger> logger);
    ~RecorderWinGameInput() override;
    void start(InputMask devices) override;
    void stop() override;
    std::string deviceName(std::string_view pnp) const override;
    std::optional<std::string> usbDeviceId(std::string_view id) const override;
    std::optional<UsbDeviceInfo> usbDeviceInfo(std::string_view id) const override;

    void updateKeyStates(const std::string& pnp, const VidPid& ids, std::uint64_t timestamp,
        const KeyStateArray& state);

private:
    GameInput::v3::IGameInput* m_gameinput = nullptr;
    std::jthread m_dispatcherThread;
    std::uint64_t m_timestampRef = 0;
    GameInput::v3::GameInputCallbackToken m_callbackToken = 0;
    std::unordered_map<std::string, KeyStateArray> m_keyStates;
};

class RecorderWinRawInput : public Recorder::Impl {
public:
    explicit RecorderWinRawInput(std::shared_ptr<spdlog::logger> logger);
    ~RecorderWinRawInput() override;
    void start(InputMask devices) override;
    void stop() override;
    std::string deviceName(std::string_view pnp) const override;
    std::optional<std::string> usbDeviceId(std::string_view id) const override;
    std::optional<UsbDeviceInfo> usbDeviceInfo(std::string_view id) const override;

    void processRawInput(HRAWINPUT rawInput);

private:
    HWND m_hwnd = nullptr;
    std::jthread m_windowThread;
    std::chrono::steady_clock::time_point m_startRef;
};

} // namespace Kbi
