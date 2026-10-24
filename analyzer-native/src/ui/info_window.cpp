#include "ui/info_window.h"

#include "parser/usb_descriptors.h"

#include <imgui.h>

#include <cstdio>
#include <format>
#include <map>
#include <string>
#include <string_view>

namespace Kbi::Ui {
namespace {

std::string hex16(std::uint16_t value) { return std::format("0x{:04X}", value); }

std::string hexBytes(const std::vector<unsigned char>& bytes)
{
    std::string out;
    out.reserve(bytes.size() * 3);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i != 0u)
            out += ' ';
        out += std::format("{:02X}", bytes[i]);
    }
    return out;
}

std::string_view speedName(UsbDeviceSpeed speed)
{
    switch (speed) {
    case UsbDeviceSpeed::LowSpeed:
        return "Low speed";
    case UsbDeviceSpeed::FullSpeed:
        return "Full speed";
    case UsbDeviceSpeed::HighSpeed:
        return "High speed";
    case UsbDeviceSpeed::SuperSpeed:
        return "Super speed";
    default:
        return "Unknown";
    }
}

void drawFields(const std::vector<DescriptorField>& fields)
{
    for (const DescriptorField& field : fields) {
        ImGui::Text("%s: %s", field.name.c_str(), field.value.c_str());
        if (!field.children.empty()) {
            ImGui::Indent();
            drawFields(field.children);
            ImGui::Unindent();
        }
    }
}

void drawSummary(const Result& result)
{
    ImGui::Text("OS: %s", result.info.osName.c_str());
    ImGui::Text("OS version: %s", result.info.osVer.c_str());
    ImGui::Text("Architecture: %s", result.info.arch.c_str());
    ImGui::Text("Backend: %s", result.info.backend.c_str());
    if (result.info.safeMode)
        ImGui::Text("Safe mode: %s", *result.info.safeMode ? "yes" : "no");
    ImGui::Separator();
    ImGui::Text("Recorded at: %s", result.time.c_str());
    ImGui::Text("Input events: %zu", result.totalInputCount());
}

void drawDevices(const Result& result)
{
    if (result.devices.empty()) {
        ImGui::TextDisabled("No devices");
        return;
    }
    for (const auto& [id, device] : result.devices) {
        ImGui::PushID(id.c_str());
        if (ImGui::CollapsingHeader(device.name.empty() ? id.c_str() : device.name.c_str(),
                ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Id: %s", id.c_str());
            ImGui::Text("VID: %s", device.vid == 0 ? "N/A" : hex16(device.vid).c_str());
            ImGui::Text("PID: %s", device.pid == 0 ? "N/A" : hex16(device.pid).c_str());
            const auto it = result.inputs.find(id);
            const std::size_t count = it == result.inputs.end() ? 0 : it->second.size();
            ImGui::Text("Number of inputs: %zu", count);
            if (device.usbDevice)
                ImGui::Text("USB device: %s", device.usbDevice->c_str());
        }
        ImGui::PopID();
    }
}

} // namespace

void InfoWindow::drawUsbDevices(const Result& result)
{
    if (result.usbDevices.empty()) {
        ImGui::TextDisabled("No USB devices");
        return;
    }

    // The descriptors are decoded once and then cached.
    for (const auto& [id, device] : result.usbDevices) {
        ImGui::PushID(id.c_str());
        if (ImGui::CollapsingHeader(id.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("VID: %s", hex16(device.vid).c_str());
            ImGui::Text("PID: %s", hex16(device.pid).c_str());
            // The speed names are literals, they are NUL-terminated.
            // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
            ImGui::Text("Speed: %s", speedName(device.speed).data());

            if (!device.descriptors.empty()) {
                auto cached = m_parsedDescriptors.find(id);
                if (cached == m_parsedDescriptors.end())
                    cached
                        = m_parsedDescriptors.emplace(id, parseUsbDescriptors(device.descriptors))
                              .first;
                for (const ParsedDescriptor& descriptor : cached->second) {
                    ImGui::PushID(static_cast<int>(descriptor.index));
                    const std::string header = std::format("#{} {} ({} bytes)", descriptor.index,
                        descriptor.typeName, descriptor.length);
                    if (ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        drawFields(descriptor.fields);
                        if (ImGui::TreeNode("Raw bytes")) {
                            ImGui::TextUnformatted(hexBytes(descriptor.raw).c_str());
                            ImGui::TreePop();
                        }
                        // A tree node is only pushed when it is open, so the
                        // pop has to happen in here as well.
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
            }
        }
        ImGui::PopID();
    }
}

void InfoWindow::draw(bool& open, const Result& result)
{
    ImGui::SetNextWindowSize(ImVec2(680.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Info", &open, ImGuiWindowFlags_NoCollapse))
        return;

    if (ImGui::BeginTabBar("##info_tabs")) {
        if (ImGui::BeginTabItem("Summary")) {
            drawSummary(result);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Devices")) {
            drawDevices(result);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("USB Devices")) {
            drawUsbDevices(result);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace Kbi::Ui
