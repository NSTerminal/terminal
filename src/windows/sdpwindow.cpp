// Copyright 2021-2023 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sdpwindow.hpp"

#include <format>

#include <imgui.h>

#include "gui/imguiext.hpp"
#include "gui/newconn.hpp"
#include "net/btutils.hpp"
#include "utils/overload.hpp"

static void printUUID(BTUtils::UUID128 uuid) {
    const uint8_t* u = uuid.data();
    ImGui::BulletText("%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X", u[0], u[1], u[2], u[3],
                      u[4], u[5], u[6], u[7], u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
}

// Prints the details of a SDP result.
static void drawServiceDetails(const BTUtils::SDPResult& result) {
    // Print the description (if there is one)
    ImGui::Text("Description: %s", result.desc.empty() ? "(none)" : result.desc.c_str());

    // Print UUIDs
    if (!result.protoUUIDs.empty()) ImGui::Text("Protocol UUIDs:");
    for (auto i : result.protoUUIDs) ImGui::BulletText("0x%04X", i);

    // Print service class UUIDs
    if (!result.serviceUUIDs.empty()) ImGui::Text("Service class UUIDs:");
    for (const auto& i : result.serviceUUIDs) printUUID(i);

    // Print profile descriptors
    if (!result.profileDescs.empty()) ImGui::Text("Profile descriptors:");
    for (const auto& [uuid, verMajor, verMinor] : result.profileDescs)
        ImGui::BulletText("0x%04X (version %d.%d)", uuid, verMajor, verMinor);

    // Print the port
    ImGui::Text("Port: %d", result.port);
}

bool SDPWindow::_drawSDPList(const BTUtils::SDPResultList& list) {
    // Begin a scrollable child window to contain the list
    ImGui::BeginChild("sdpList", {}, true);
    bool ret = false;

    // ID to use in case multiple services have the same name
    int id = 0;
    for (const auto& result : list) {
        ImGui::PushID(id); // Push the ID, then increment it
        id++;              // TODO: Use views::enumerate() in C++23

        if (const char* name = result.name.empty() ? "Unnamed service" : result.name.c_str(); ImGui::TreeNode(name)) {
            drawServiceDetails(result);

            // Connection options
            if (ImGui::Button("Connect...")) {
                _serviceName = name;
                _port = result.port;
                ret = true;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    ImGui::EndChild();
    return ret;
}

void SDPWindow::_drawConnOptions(std::string_view info) {
    using enum ConnectionType;

    // Connection type selection
    ImGui::RadioButton("RFCOMM", _connType, RFCOMM);
    ImGui::RadioButton("L2CAP Sequential Packet", _connType, L2CAPSeqPacket);
    ImGui::RadioButton("L2CAP Stream", _connType, L2CAPStream);
    ImGui::RadioButton("L2CAP Datagram", _connType, L2CAPDgram);

    // Connect button
    ImGui::Spacing();
    if (ImGui::Button("Connect"))
        addConnWindow<SocketTag::BT>(_list, { _connType, _target.name, _target.address, _port }, info);
}

void SDPWindow::_checkInquiryStatus() {
    Overload visitor{
        [](std::monostate) { ImGui::TextUnformatted("No inquiry run"); },
        [this](AsyncSDPInquiry& asyncInq) {
            using namespace std::literals;
            if (asyncInq.wait_for(0s) != std::future_status::ready) {
                // Running, display a spinner
                ImGui::TextUnformatted("Running SDP inquiry");
                ImGui::SameLine();
                ImGui::Spinner();
                return;
            }

            // The async operation has completed, attempt to get its results
            try {
                _sdpInquiry = asyncInq.get();
            } catch (const System::SystemError& error) {
                _sdpInquiry = error;
            }
        },
        [](const std::system_error&) { ImGui::TextWrapped("System error: Failed to launch thread."); },
        [](const System::SystemError& error) { ImGui::TextWrapped("Error %s", error.what()); },
        [this](const BTUtils::SDPResultList& list) {
            // Done, print results
            if (list.empty()) {
                ImGui::Text("No SDP results found for \"%s\".", _target.name.c_str());
                return;
            }

            if (_drawSDPList(list)) ImGui::OpenPopup("options");

            if (ImGui::BeginPopup("options")) {
                _drawConnOptions(_serviceName);
                ImGui::EndPopup();
            }
        },
    };

    std::visit(visitor, _sdpInquiry);
}

void SDPWindow::_drawSDPTab() {
    if (!ImGui::BeginTabItem("Connect with SDP")) return;

    // Disable the widgets if the async inquiry is running
    ImGui::BeginDisabled(std::holds_alternative<AsyncSDPInquiry>(_sdpInquiry));

    // UUID selection combobox
    using namespace ImGui::Literals;

    ImGui::SetNextItemWidth(10_fh);
    if (ImGui::BeginCombo("Protocol/Service UUID", _selectedUUID.c_str())) {
        for (const auto& [name, _] : _uuids)
            if (ImGui::Selectable(name.c_str())) _selectedUUID = name;
        ImGui::EndCombo();
    }

#if OS_WINDOWS || OS_MACOS
    // Flush cache option (Windows only)
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x * 4);
    ImGui::Checkbox("Flush cache", &_flushCache);
    ImGui::HelpMarker("Ignore previous cached advertising data on this inquiry.");
#endif

    // Run button
    if (ImGui::Button("Run SDP Inquiry")) {
        try {
            // Start the inquiry
            _sdpInquiry = std::async(std::launch::async, BTUtils::sdpLookup, _target.address, _uuids.at(_selectedUUID),
                                     _flushCache);
        } catch (const std::system_error& error) {
            _sdpInquiry = error;
        }
    }

    ImGui::EndDisabled();
    _checkInquiryStatus();
    ImGui::EndTabItem();
}

void SDPWindow::_drawManualTab() {
    if (!ImGui::BeginTabItem("Connect Manually")) return;

    using namespace ImGui::Literals;
    ImGui::SetNextItemWidth(7_fh);
    ImGui::InputScalar("Port", _port, 1, 10);

    _drawConnOptions(std::format("Port {}", _port));
    ImGui::EndTabItem();
}

void SDPWindow::_onBeforeUpdate() {
    using namespace ImGui::Literals;

    ImGui::SetNextWindowSize(30_fh * 18_fh, ImGuiCond_Appearing);
    _setClosable(!std::holds_alternative<AsyncSDPInquiry>(_sdpInquiry));
}

void SDPWindow::_onUpdate() {
    if (ImGui::BeginTabBar("ConnectionOptions")) {
        _drawSDPTab();
        _drawManualTab();
        ImGui::EndTabBar();
    }
}