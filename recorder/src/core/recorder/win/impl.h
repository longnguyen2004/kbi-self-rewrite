#include "../recorder_impl.h"
#include <boost/container/static_vector.hpp>
#include <chrono>
#include <gameinput.h>
#include <spdlog/fwd.h>
#include <thread>
#include <windows.h>

using namespace GameInput::v3;
using KeyStateArray = boost::container::static_vector<GameInputKeyState, 50>;

class RecorderWinGameinput : public Recorder::Impl {
public:
    explicit RecorderWinGameinput(std::shared_ptr<spdlog::logger> logger);
    void start(bool keyboard, bool mouse, bool gamepad) override;
    void stop() override;
    std::string getDeviceName(std::string_view pnp) const override;
    std::optional<std::string> getUsbDeviceId(std::string_view id) const override;
    std::optional<UsbDeviceInfo> getUsbDeviceInfo(std::string_view id) const override;

    void updateKeyStates(
        const std::string& pnp, std::uint16_t vid, std::uint16_t pid,
        std::uint64_t timestamp, const KeyStateArray& state);

private:
    IGameInput* m_gameinput;
    std::jthread m_dispatcherThread;
    std::uint64_t m_timestampRef;
    GameInputCallbackToken m_callbackToken;
    std::unordered_map<std::string, KeyStateArray> m_keyStates;
};

class RecorderWinRawinput : public Recorder::Impl {
public:
    explicit RecorderWinRawinput(std::shared_ptr<spdlog::logger> logger);
    void start(bool keyboard, bool mouse, bool gamepad) override;
    void stop() override;
    std::string getDeviceName(std::string_view pnp) const override;
    std::optional<std::string> getUsbDeviceId(std::string_view id) const override;
    std::optional<UsbDeviceInfo> getUsbDeviceInfo(std::string_view id) const override;

    void processRawInput(HRAWINPUT rawInput);

private:
    HWND m_hwnd = nullptr;
    std::jthread m_windowThread;
    std::chrono::steady_clock::time_point m_startRef;
};
