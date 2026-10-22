#include "core/input_timeline.h"

#include "core/keycode_util.h"

#include <algorithm>
#include <format>

namespace Kbi {

void InputTimeline::reset()
{
    m_keypresses.clear();
    m_keys.clear();
    m_keyDownMap.clear();
    m_endTimestamp = 0.0;
}

void InputTimeline::add(const std::string& deviceId, const InputEvent& event)
{
    const std::string mapKey = std::format("{}-{}", deviceId, event.code);
    if (event.pressed) {
        m_keyDownMap[mapKey] = event.timestamp;
        return;
    }

    const auto it = m_keyDownMap.find(mapKey);
    if (it == m_keyDownMap.end())
        return;
    const std::uint64_t keydownTime = it->second;
    m_keyDownMap.erase(it);

    const std::string keyName(keycodeName(event.code));
    const double start = static_cast<double>(keydownTime) / 1000000.0;
    const double end = static_cast<double>(event.timestamp) / 1000000.0;
    m_keypresses.push_back(Keypress { .start = start, .end = end, .key = keyName, .deviceId = deviceId });
    if (std::find(m_keys.begin(), m_keys.end(), keyName) == m_keys.end())
        m_keys.push_back(keyName);
    m_endTimestamp = std::max(m_endTimestamp, end);
}

} // namespace Kbi
