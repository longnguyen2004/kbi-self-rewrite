#include "ui/info_window.h"

#include "parser/usb_descriptors.h"

#include <imgui.h>

#include <cstdio>
#include <format>
#include <map>
#include <string>

namespace kbi::ui {
namespace {

std::string hex16(std::uint16_t value) {
    return std::format("0x{:04X}", value);
}

std::string hex_bytes(const std::vector<unsigned char>& bytes) {
    std::string out;
    out.reserve(bytes.size() * 3);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i)
            out += ' ';
        out += std::format("{:02X}", bytes[i]);
    }
    return out;
}

const char* speed_name(UsbDeviceSpeed speed) {
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

void draw_fields(const std::vector<DescriptorField>& fields) {
    for (const DescriptorField& field : fields) {
        ImGui::Text("%s: %s", field.name.c_str(), field.value.c_str());
        if (!field.children.empty()) {
            ImGui::Indent();
            draw_fields(field.children);
            ImGui::Unindent();
        }
    }
}

void draw_summary(const Result& result) {
    ImGui::Text("OS: %s", result.info.os_name.c_str());
    ImGui::Text("OS version: %s", result.info.os_ver.c_str());
    ImGui::Text("Architecture: %s", result.info.arch.c_str());
    ImGui::Text("Backend: %s", result.info.backend.c_str());
    if (result.info.safe_mode)
        ImGui::Text("Safe mode: %s", *result.info.safe_mode ? "yes" : "no");
    ImGui::Separator();
    ImGui::Text("Recorded at: %s", result.time.c_str());
    ImGui::Text("Input events: %zu", result.total_input_count());
}

void draw_devices(const Result& result) {
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
            if (device.usb_device)
                ImGui::Text("USB device: %s", device.usb_device->c_str());
        }
        ImGui::PopID();
    }
}

void draw_usb_devices(const Result& result) {
    if (result.usb_devices.empty()) {
        ImGui::TextDisabled("No USB devices");
        return;
    }

    // The descriptors are decoded once and then cached.
    static std::map<std::string, std::vector<ParsedDescriptor>> cache;
    for (const auto& [id, device] : result.usb_devices) {
        ImGui::PushID(id.c_str());
        if (ImGui::CollapsingHeader(id.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("VID: %s", hex16(device.vid).c_str());
            ImGui::Text("PID: %s", hex16(device.pid).c_str());
            ImGui::Text("Speed: %s", speed_name(device.speed));

            if (!device.descriptors.empty()) {
                auto cached = cache.find(id);
                if (cached == cache.end())
                    cached = cache.emplace(id, parse_usb_descriptors(device.descriptors)).first;
                for (const ParsedDescriptor& descriptor : cached->second) {
                    ImGui::PushID(static_cast<int>(descriptor.index));
                    const std::string header = std::format("#{} {} ({} bytes)",
                                                          descriptor.index,
                                                          descriptor.type_name,
                                                          descriptor.length);
                    if (ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        draw_fields(descriptor.fields);
                        if (ImGui::TreeNode("Raw bytes")) {
                            ImGui::TextUnformatted(hex_bytes(descriptor.raw).c_str());
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

} // namespace

void draw_info_window(bool* open, const Result& result) {
    ImGui::SetNextWindowSize(ImVec2(680.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Info", open, ImGuiWindowFlags_NoCollapse))
        return;

    if (ImGui::BeginTabBar("##info_tabs")) {
        if (ImGui::BeginTabItem("Summary")) {
            draw_summary(result);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Devices")) {
            draw_devices(result);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("USB Devices")) {
            draw_usb_devices(result);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace kbi::ui
