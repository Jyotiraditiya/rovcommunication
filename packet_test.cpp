#include <iostream>
#include <iomanip>
#include <cstdint>

#include "packet.hpp"

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
    std::cout << "U-ROV packet construction test\n\n";

    // ============================================================
    // CONTROL PACKET
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

    uint8_t controlPacket[urov::CONTROL_PACKET_SIZE]{};

    urov::buildControlPacket(
        control,
        1,
        1000000,
        urov::FLAG_ACK_REQ,
        controlPacket
    );

    std::cout << "CONTROL packet\n";
    std::cout << "Packet size: "
              << sizeof(controlPacket)
              << " bytes\n";

    std::cout << "Raw packet:\n";
    printBytes(
        controlPacket,
        sizeof(controlPacket)
    );


    // ============================================================
    // PARSE CONTROL PACKET
    // ============================================================

    urov::PacketHeader decodedControlHeader{};
    urov::ControlPayload decodedControl{};

    bool controlResult =
        urov::parseControlPacket(
            controlPacket,
            sizeof(controlPacket),
            decodedControlHeader,
            decodedControl
        );

    std::cout << "\nCONTROL parse result: "
              << (controlResult ? "PASSED" : "FAILED")
              << "\n";

    if (controlResult)
    {
        std::cout << "Message type : "
                  << static_cast<int>(
                         decodedControlHeader.message_type)
                  << "\n";

        std::cout << "Sequence     : "
                  << decodedControlHeader.sequence_number
                  << "\n";

        std::cout << "Payload size : "
                  << decodedControlHeader.payload_length
                  << "\n";

        std::cout << "Forward      : "
                  << decodedControl.forward
                  << "\n";

        std::cout << "Command ID   : "
                  << decodedControl.command_id
                  << "\n";
    }


    // ============================================================
    // TELEMETRY PACKET
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

    uint8_t telemetryPacket[
        urov::TELEMETRY_PACKET_SIZE
    ]{};

    urov::buildTelemetryPacket(
        telemetry,
        25,
        2000000,
        urov::FLAG_NONE,
        telemetryPacket
    );

    std::cout << "\n\nTELEMETRY packet\n";

    std::cout << "Packet size: "
              << sizeof(telemetryPacket)
              << " bytes\n";

    std::cout << "Raw packet:\n";

    printBytes(
        telemetryPacket,
        sizeof(telemetryPacket)
    );


    // ============================================================
    // PARSE TELEMETRY PACKET
    // ============================================================

    urov::PacketHeader decodedTelemetryHeader{};
    urov::TelemetryPayload decodedTelemetry{};

    bool telemetryResult =
        urov::parseTelemetryPacket(
            telemetryPacket,
            sizeof(telemetryPacket),
            decodedTelemetryHeader,
            decodedTelemetry
        );

    std::cout << "\nTELEMETRY parse result: "
              << (telemetryResult ? "PASSED" : "FAILED")
              << "\n";

    if (telemetryResult)
    {
        std::cout << "Message type : "
                  << static_cast<int>(
                         decodedTelemetryHeader.message_type)
                  << "\n";

        std::cout << "Sequence     : "
                  << decodedTelemetryHeader.sequence_number
                  << "\n";

        std::cout << "Payload size : "
                  << decodedTelemetryHeader.payload_length
                  << "\n";

        std::cout << "Depth        : "
                  << decodedTelemetry.depth_mm
                  << " mm\n";

        std::cout << "Battery      : "
                  << decodedTelemetry.battery_mV
                  << " mV\n";
    }


    // ============================================================
    // FINAL RESULT
    // ============================================================

    std::cout << "\n--------------------------------------------\n";

    if (controlResult && telemetryResult)
    {
        std::cout << "PACKET TEST : PASSED\n";
        std::cout << "--------------------------------------------\n";
        return 0;
    }

    std::cout << "PACKET TEST : FAILED\n";
    std::cout << "--------------------------------------------\n";

    return 1;
}