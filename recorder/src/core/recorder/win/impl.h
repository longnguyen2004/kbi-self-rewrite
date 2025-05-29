#include "../recorder_impl.h"
#include <windows.h>
#include <gameinput.h>
#include <boost/container/static_vector.hpp>
#include <thread>
#include <atomic>

using namespace GameInput::v1;
using KeyStateArray = boost::container::static_vector<GameInputKeyState, 50>;

class recorder_win_gameinput: public Recorder::Impl
{
public:
    recorder_win_gameinput();
    virtual bool Recording() const;
    virtual void Start(bool keyboard, bool mouse, bool gamepad);
    virtual void Stop();

    void _update_key_states(
        const std::string& pnp, std::uint16_t vid, std::uint16_t pid,
        std::uint64_t timestamp, const KeyStateArray& state
    );
private:
    IGameInput *m_gameinput;
    std::uint64_t m_timestamp_ref;
    GameInputCallbackToken m_callback_token;

    bool m_running = false;
    std::unordered_map<std::string, KeyStateArray> m_key_states;
};
