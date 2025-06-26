#include "../recorder_impl.h"
#include <boost/container/static_vector.hpp>
#include <gameinput.h>
#include <spdlog/fwd.h>
#include <windows.h>
#include <chrono>
#include <thread>

using namespace GameInput::v1;
using KeyStateArray = boost::container::static_vector<GameInputKeyState, 50>;

class recorder_win_gameinput: public Recorder::Impl
{
public:
    recorder_win_gameinput(std::shared_ptr<spdlog::logger> logger);
    virtual void Start(bool keyboard, bool mouse, bool gamepad);
    virtual void Stop();
    virtual std::string GetDeviceName(std::string_view id) const;

    void _update_key_states(
        const std::string& pnp, std::uint16_t vid, std::uint16_t pid,
        std::uint64_t timestamp, const KeyStateArray& state
    );
private:
    IGameInput *m_gameinput;
    std::jthread m_dispatcher_thread;
    std::uint64_t m_timestamp_ref;
    GameInputCallbackToken m_callback_token;
    std::unordered_map<std::string, KeyStateArray> m_key_states;
};

class recorder_win_rawinput: public Recorder::Impl
{
public:
    recorder_win_rawinput(std::shared_ptr<spdlog::logger> logger);
    virtual void Start(bool keyboard, bool mouse, bool gamepad);
    virtual void Stop();
    virtual std::string GetDeviceName(std::string_view id) const;

    void _processRawInput(HRAWINPUT rawInput);
private:
    HWND m_hwnd = nullptr;
    std::jthread m_window_thread;
    std::chrono::steady_clock::time_point m_start_ref;
};
