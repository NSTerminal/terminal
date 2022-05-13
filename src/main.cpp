// Copyright 2021-2022 Aidan Sun and the Network Socket Terminal contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm> // std::max()
#include <map>
#include <future>
#include <chrono> // std::chrono
#include <iterator> // std::back_inserter()
#include <string_view>
#include <system_error> // std::system_error
#include <variant>

#include "app/settings.hpp"
#include "app/mainhandle.hpp"
#include "async/async.hpp"
#include "gui/console.hpp"
#include "gui/connwindowlist.hpp"
#include "net/btutils.hpp"
#include "sys/error.hpp"
#include "util/imguiext.hpp"
#include "util/formatcompat.hpp"

#ifdef _MSC_VER
// Use WinMain() as an entry point on MSVC
#define MAIN_FUNC CALLBACK WinMain
#define MAIN_ARGS _In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int
#else
// Use the standard main() as an entry point on other compilers
#define MAIN_FUNC main
#define MAIN_ARGS
#endif

// List of open windows
static ConnWindowList connections;

// If an attempted Bluetooth connection is unique
static bool isNewBT = true;

// Renders the "New Connection" window for Internet-based connections.
static void drawIPConnectionTab();

// Renders the "New Connection" window for Bluetooth-based connections.
static void drawBTConnectionTab();

int MAIN_FUNC(MAIN_ARGS) {
    if (!MainHandler::initApp()) return EXIT_FAILURE; // Create a main application window

    // Initialize APIs for sockets and Bluetooth
    std::variant<std::monostate, System::SystemError, std::system_error> initResult;
    try {
        BTUtils::init();
    } catch (const System::SystemError& error) {
        initResult = error;
    } catch (const std::system_error& error) {
        initResult = error;
    }

    // Main loop
    while (MainHandler::isActive()) {
        MainHandler::handleNewFrame();

        // Error message for failed initialization
        if (std::holds_alternative<System::SystemError>(initResult))
            ImGui::Overlay({ 10, 10 }, ImGuiOverlayCorner::TopLeft,
                           "Initialization failed - %s", std::get<System::SystemError>(initResult).formatted().c_str());
        else if (std::holds_alternative<std::system_error>(initResult))
            ImGui::Overlay({ 10, 10 }, ImGuiOverlayCorner::TopLeft, "Could not initialize thread pool.");

        // New connection window
        ImGui::SetNextWindowSize({ 600, 250 }, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("New Connection") && ImGui::BeginTabBar("ConnectionTypes")) {
            drawIPConnectionTab();
            drawBTConnectionTab();
            ImGui::EndTabBar();
        }
        ImGui::End();

        connections.update();

        MainHandler::renderWindow();
    }

    BTUtils::cleanup();
    MainHandler::cleanupApp();
    return EXIT_SUCCESS;
}

static void drawIPConnectionTab() {
    if (!ImGui::BeginTabItem("Internet Protocol")) return;
    using Sockets::ConnectionType;
    using enum ConnectionType;

    static std::string addr; // Server address
    static uint16_t port = 0; // Server port
    static ConnectionType type = TCP; // Type of connection to create
    static bool isNew = true; // If the attempted connection is unique

    ImGui::BeginChild("Output", { 0, isNew ? 0 : -ImGui::GetFrameHeightWithSpacing() });

    // Widget labels
    static const char* portLabel = "Port";
    static const char* addressLabel = "Address";

    static float portWidth = 100.0f; // The width of the port input (hardcoded)
    static float minAddressWidth = 120.0f; // The minimum width of the address textbox

    // The horizontal space available in the window
    float spaceAvail
        = ImGui::GetContentRegionAvail().x              // The width of the child window without scrollbars
        - ImGui::CalcTextWidthWithSpacing(addressLabel) // Width of address input label
        - ImGui::GetStyle().ItemSpacing.x               // Space between the address and port inputs
        - ImGui::CalcTextWidthWithSpacing(portLabel)    // Width of the port input label
        - portWidth;                                    // Width of the port input

    // Server address, set the textbox width to the space not taken up by everything else
    // Use std::max to set a minimum size for the texbox; it will not resize past a certain min bound.
    ImGui::SetNextItemWidth(std::max(spaceAvail, minAddressWidth));
    ImGui::InputText(addressLabel, addr);

    // Server port, keep it on the same line as the textbox if there's enough space
    if (spaceAvail > minAddressWidth) ImGui::SameLine();
    ImGui::SetNextItemWidth(portWidth);
    ImGui::InputScalar(portLabel, port, 1, 10);

    // Connection type selection
    if (ImGui::RadioButton("TCP", type == TCP)) type = TCP;
    if (ImGui::RadioButton("UDP", type == UDP)) type = UDP;

    // Connect button
    ImGui::Spacing();
    ImGui::BeginDisabled(addr.empty());
    if (ImGui::Button("Connect")) isNew = connections.add({ type, "", addr, port });
    ImGui::EndDisabled();
    ImGui::EndChild();

    // If the connection exists, show a message
    if (!isNew) ImGui::Text("This connection is already open.");

    ImGui::EndTabItem();
}

// Draws the combobox used to select UUIDs for Bluetooth SDP lookup.
static UUID drawUUIDCombo() {
    // Map of UUIDs, associating a UUID value with a user-given name
    static std::map<std::string, UUID> uuidList = {
        { "L2CAP", BTUtils::createUUIDFromBase(0x0100) },
        { "RFCOMM", BTUtils::createUUIDFromBase(0x0003) }
    };

    // The selected entry, start with the first one
    static std::string selected = uuidList.begin()->first;

    // Draw the combobox
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::BeginCombo("Protocol/Service UUID", selected.c_str())) {
        for (const auto& [name, uuid] : uuidList) if (ImGui::Selectable(name.c_str())) selected = name;
        ImGui::EndCombo();
    }

    // Return the selected entry
    return uuidList[selected];
}

// Draws a menu composed of the paired Bluetooth devices.
static bool drawPairedDevices(const Sockets::DeviceDataList& devices, bool showAddrs, Sockets::DeviceData& selected) {
    bool ret = false;

    // The menu is a listbox which can display 4 entries at a time
    if (ImGui::BeginListBox("##paired", { ImGui::FILL, ImGui::GetFrameHeight() * 4 })) {
        // The index of the selected item
        // The code is structured so that no buffer overruns occur if the number of devices decreases after a
        // refresh (don't use this variable to access something in the vector).
        // The initial value for the selected index is -1, so no items appear selected at first.
        // Negative values for size_t are well-defined by the standard - https://stackoverflow.com./q/28247733
        static size_t selectedIdx = static_cast<size_t>(-1);
        for (size_t i = 0; i < devices.size(); i++) {
            // The current device
            const auto& current = devices[i];
            const bool isSelected = (selectedIdx == i);

            // Get the necessary string fields, then create the device's entry in the listbox
            std::string text = current.name;
            if (showAddrs) std::format_to(std::back_inserter(text), " ({})", current.address);

            // Render the Selectable
            // Push the address (always unique) as the ID in case multiple devices have the same name.
            ImGui::PushID(current.address.c_str());
            if (ImGui::Selectable(text.c_str(), isSelected)) {
                // Something was selected
                selectedIdx = i;
                selected = current;
                ret = true;
            }
            ImGui::PopID();

            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndListBox();
    }

    return ret;
}

// Draws the options for connecting to a device with Bluetooth.
static void drawBTConnOptions(const Sockets::DeviceData& target, uint16_t port, std::string_view extraInfo) {
    using Sockets::ConnectionType;
    using enum ConnectionType;

    static ConnectionType type = RFCOMM;

    // Connection type selection
    if (ImGui::RadioButton("RFCOMM", type == RFCOMM)) type = RFCOMM;
    if (ImGui::RadioButton("L2CAP Sequential Packet", type == L2CAPSeqPacket)) type = L2CAPSeqPacket;
    if (ImGui::RadioButton("L2CAP Stream", type == L2CAPStream)) type = L2CAPStream;
    if (ImGui::RadioButton("L2CAP Datagram", type == L2CAPDgram)) type = L2CAPDgram;

    ImGui::Spacing();
    if (ImGui::Button("Connect")) isNewBT = connections.add({ type, target.name, target.address, port }, extraInfo);
}

// Begins a child window with an optional one-line spacing at the bottom.
static void beginChildWithSpacing(bool spacing, bool border) {
    ImGui::BeginChild("output", { 0, spacing ? 0 : -ImGui::GetFrameHeightWithSpacing() }, border);
}

// Displays the entries from an SDP lookup (with buttons to connect to each) in a tree format.
static void drawSDPList(const BTUtils::SDPResultList& list, const Sockets::DeviceData& selected) {
    // If no SDP results were found, display a message and exit
    if (list.empty()) {
        ImGui::Text("No SDP results found for \"%s\".", selected.name.c_str());
        return;
    }

    beginChildWithSpacing(isNewBT, true);

    // ID to use in case multiple services have the same name
    unsigned int id = 0;
    for (const auto& [protos, services, profiles, port, name, desc] : list) {
        const char* serviceName = name.empty() ? "Unnamed service" : name.c_str();

        ImGui::PushID(id++); // Push the ID, then increment it

        if (ImGui::TreeNode(serviceName)) {
            // Print the description (if there is one)
            ImGui::Text("Description: %s", desc.empty() ? "(none)" : desc.c_str());

            // Print UUIDs
            if (!protos.empty()) ImGui::Text("Protocol UUIDs:");
            for (auto i : protos) ImGui::BulletText("0x%04X", i);

            // Print service class UUIDs
            if (!services.empty()) ImGui::Text("Service class UUIDs:");
            for (const auto& i : services)
                ImGui::BulletText("%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                                  i.Data1, i.Data2, i.Data3,
                                  i.Data4[0], i.Data4[1], i.Data4[2], i.Data4[3],
                                  i.Data4[4], i.Data4[5], i.Data4[6], i.Data4[7]);

            // Print profile descriptors
            if (!profiles.empty()) ImGui::Text("Profile descriptors:");
            for (const auto& [uuid, verMajor, verMinor] : profiles)
                ImGui::BulletText("0x%04X (version %d.%d)", uuid, verMajor, verMinor);

            // Print the port
            ImGui::Text("Port: %d", port);

            // Connection options
            if (ImGui::Button("Connect...")) ImGui::OpenPopup("options");
            if (ImGui::BeginPopup("options")) {
                drawBTConnOptions(selected, port, serviceName);
                ImGui::EndPopup();
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
}

static void drawBTConnectionTab() {
    if (!ImGui::BeginTabItem("Bluetooth")) return;

    bool btInitDone = BTUtils::initialized(); // If Bluetooth initialization completed
    static enum class SDPStatus { NotStarted, Started, Error, Done } sdpStatus = SDPStatus::NotStarted;

    // Check if the application's sockets are initialized
    if (!btInitDone) {
        ImGui::TextWrapped("Socket initialization failed.");
        ImGui::Spacing();
    }

    ImGui::BeginDisabled(!btInitDone || (sdpStatus == SDPStatus::Started));

    // Get the paired devices when this tab is first clicked or if the "Refresh" button is clicked
    using Sockets::DeviceDataList;
    static bool devicesListed = false; // If device enumeration has completed at least once
    static std::variant<DeviceDataList, System::SystemError> pairedDevices; // Vector of paired devices

    if ((ImGui::Button("Refresh List") || !devicesListed) && btInitDone) {
        devicesListed = true;

        try {
            pairedDevices = BTUtils::getPaired();
        } catch (const System::SystemError& error) {
            pairedDevices = error;
        }
    }

    static bool useSDP = true; // If available connections on devices are found using SDP
    static Sockets::DeviceData selected; // The device selected in the menu
    static std::future<BTUtils::SDPResultList> sdpInq; // Asynchronous SDP inquiry
    static bool deviceSelected = false; // If the user interacted with the device menu at least once

    if (std::holds_alternative<DeviceDataList>(pairedDevices)) {
        // Enumeration succeeded, display all devices found
        const auto& deviceList = std::get<DeviceDataList>(pairedDevices);
        if (deviceList.empty()) {
            // Bluetooth initialization is done and no devices detected
            // (BT init checked because an empty devices vector may be caused by failed init.)
            ImGui::Text("No paired devices.");
        } else {
            float sameLineSpacing = ImGui::GetStyle().ItemInnerSpacing.x * 4; // Spacing between the below widgets

            // Checkbox to display device addresses
            static bool showAddrs = false;
            ImGui::SameLine(0, sameLineSpacing);
            ImGui::Checkbox("Show Addresses", &showAddrs);
            ImGui::Spacing();

            // Checkbox to switch between SDP/manual connection modes
            // Hide the unique connection message when the mode is switched.
            if (ImGui::Checkbox("Use SDP", &useSDP)) isNewBT = true;

            static UUID selectedUUID; // UUID selection combobox
            static bool flushSDPCache = false; // If SDP cache is flushed on an inquiry (Windows only)

            if (useSDP) {
                selectedUUID = drawUUIDCombo();

#ifdef _WIN32
                // "Flush cache" option (Windows only)
                ImGui::SameLine(0, sameLineSpacing);
                ImGui::Checkbox("Flush cache", &flushSDPCache);
                ImGui::HelpMarker("Ignore previous cached advertising data on this inquiry.");
#endif
            }

            // There are devices, display them
            if (drawPairedDevices(deviceList, showAddrs, selected)) {
                deviceSelected = true;

                if (useSDP) {
                    try {
                        sdpInq = std::async(std::launch::async, BTUtils::sdpLookup,
                                            selected.address, selectedUUID, flushSDPCache);
                        sdpStatus = SDPStatus::Started;
                    } catch (const std::system_error&) {
                        sdpStatus = SDPStatus::Error;
                    }
                }
            }
        }
    } else {
        // Error occurred
        ImGui::TextWrapped("Error %s", std::get<System::SystemError>(pairedDevices).formatted().c_str());
    }

    ImGui::EndDisabled();

    static BTUtils::SDPResultList sdpResults; // Result list from SDP search

    // Check if the future is ready
    using namespace std::literals;
    if (sdpInq.valid() && (sdpInq.wait_for(0s) == std::future_status::ready)) {
        sdpStatus = SDPStatus::Done;
        sdpResults = sdpInq.get();
    }

    if (useSDP) {
        switch (sdpStatus) {
        case SDPStatus::Started:
            // Running, display a spinner
            ImGui::LoadingSpinner("Running SDP inquiry");
            break;
        case SDPStatus::Error:
            // Error occurred
            ImGui::TextWrapped("System error: Failed to launch thread.");
            break;
        case SDPStatus::Done:
            // Done, print results
            drawSDPList(sdpResults, selected);
        }
    } else if (deviceSelected) {
        // SDP is not used and a device is selected, show the connection options
        beginChildWithSpacing(isNewBT, false);
        static uint16_t port = 0;

        ImGui::Spacing();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::InputScalar("Port", port, 1, 10);

        drawBTConnOptions(selected, port, std::format("Port {}", port));
        ImGui::EndChild();
    }

    ImGui::EndTabItem();

    // If the connection exists, show a message
    if (!isNewBT) ImGui::Text("This connection is already open.");
}
