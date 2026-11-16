#include <iostream>
#include <iomanip>
#include <cstdint>
#include "serialization.hpp"

void printBytes(const uint8_t* buffer, std::size_t size)
{
    for (std::size_t i = 0; i < size; ++i)
    {
        std::cout << std::uppercase
                  << std::hex
                  << std::setw(2)
                  << std::setfill('0')
                  << static_cast<int>(buffer[i])
                  << " ";
    }

    std::cout << std::dec << "\n";
}

int main()
{
    std::cout << "U-ROV serialization test\n\n";

    // ============================================================
    // CONTROL TEST
    // ============================================================

    urov::ControlPayload control{};

    control.forward = 1000;
    control.lateral = -200;
    control.vertical = 300;
    control.yaw = -400;
    control.mode = 2;
    control.emergency_stop = 0;
    control.reserved = 0;
    control.command_id = 12345;

    uint8_t controlBuffer[16]{};

    urov::serializeControl(control, controlBuffer);

    std::cout << "Serialized CONTROL: ";
    printBytes(controlBuffer, sizeof(controlBuffer));

    urov::ControlPayload decodedControl =
        urov::deserializeControl(controlBuffer);

    std::cout << "\nDecoded CONTROL\n";
    std::cout << "Forward        : "
              << decodedControl.forward << "\n";

    std::cout << "Lateral        : "
              << decodedControl.lateral << "\n";

    std::cout << "Vertical       : "
              << decodedControl.vertical << "\n";

    std::cout << "Yaw            : "
              << decodedControl.yaw << "\n";

    std::cout << "Mode           : "
              << static_cast<int>(decodedControl.mode) << "\n";

    std::cout << "Emergency stop : "
              << static_cast<int>(decodedControl.emergency_stop) << "\n";

    std::cout << "Command ID     : "
              << decodedControl.command_id << "\n";


    // ============================================================
    // TELEMETRY TEST
    // ============================================================

    urov::TelemetryPayload telemetry{};

    telemetry.depth_mm = 12500;
    telemetry.roll_cdeg = 150;
    telemetry.pitch_cdeg = -75;
    telemetry.yaw_cdeg = 450;
    telemetry.battery_mV = 14800;
    telemetry.temperature_cdeg = 2350;
    telemetry.system_status = 1;
    telemetry.reserved = 0;

    uint8_t telemetryBuffer[16]{};

    urov::serializeTelemetry(telemetry, telemetryBuffer);

    std::cout << "\nSerialized TELEMETRY: ";
    printBytes(telemetryBuffer, sizeof(telemetryBuffer));

    urov::TelemetryPayload decodedTelemetry =
        urov::deserializeTelemetry(telemetryBuffer);

    std::cout << "\nDecoded TELEMETRY\n";

    std::cout << "Depth (mm)          : "
              << decodedTelemetry.depth_mm << "\n";

    std::cout << "Roll (cdeg)         : "
              << decodedTelemetry.roll_cdeg << "\n";

    std::cout << "Pitch (cdeg)        : "
              << decodedTelemetry.pitch_cdeg << "\n";

    std::cout << "Yaw (cdeg)          : "
              << decodedTelemetry.yaw_cdeg << "\n";

    std::cout << "Battery (mV)        : "
              << decodedTelemetry.battery_mV << "\n";

    std::cout << "Temperature (cdeg)  : "
              << decodedTelemetry.temperature_cdeg << "\n";

    std::cout << "System status       : "
              << static_cast<int>(decodedTelemetry.system_status)
              << "\n";


    // ============================================================
    // VALIDATION
    // ============================================================

    bool controlPassed =
        decodedControl.forward == control.forward &&
        decodedControl.lateral == control.lateral &&
        decodedControl.vertical == control.vertical &&
        decodedControl.yaw == control.yaw &&
        decodedControl.mode == control.mode &&
        decodedControl.emergency_stop == control.emergency_stop &&
        decodedControl.command_id == control.command_id;

    bool telemetryPassed =
        decodedTelemetry.depth_mm == telemetry.depth_mm &&
        decodedTelemetry.roll_cdeg == telemetry.roll_cdeg &&
        decodedTelemetry.pitch_cdeg == telemetry.pitch_cdeg &&
        decodedTelemetry.yaw_cdeg == telemetry.yaw_cdeg &&
        decodedTelemetry.battery_mV == telemetry.battery_mV &&
        decodedTelemetry.temperature_cdeg == telemetry.temperature_cdeg &&
        decodedTelemetry.system_status == telemetry.system_status;

    std::cout << "\n--------------------------------------------\n";

    if (controlPassed)
        std::cout << "CONTROL TEST   : PASSED\n";
    else
        std::cout << "CONTROL TEST   : FAILED\n";

    if (telemetryPassed)
        std::cout << "TELEMETRY TEST : PASSED\n";
    else
        std::cout << "TELEMETRY TEST : FAILED\n";

    if (controlPassed && telemetryPassed)
        std::cout << "\nOVERALL TEST   : PASSED\n";
    else
        std::cout << "\nOVERALL TEST   : FAILED\n";

    std::cout << "--------------------------------------------\n";

    return (controlPassed && telemetryPassed) ? 0 : 1;
}