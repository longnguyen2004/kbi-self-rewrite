#pragma once

#include <core/result.h>
#include <parser/usb_descriptors.h>

#include <map>
#include <string>
#include <vector>

namespace Kbi::Ui {

// The info window with the Summary / Devices / USB Devices tabs.
class InfoWindow {
public:
    void draw(bool& open, const Result& result);

    // Drops the cached parse results, the next recording displays from
    // scratch.
    void reset() { m_parsedDescriptors.clear(); }

private:
    void drawUsbDevices(const Result& result);

    // The decoded USB descriptors, they are parsed once per device.
    std::map<std::string, std::vector<ParsedDescriptor>> m_parsedDescriptors;
};

} // namespace Kbi::Ui
