#include <iostream>
#include <cstdint>
#include <chrono>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "packet.hpp"

int main()
{
    const char* IP = "127.0.0.1";
    const int PORT = 5000;

    // ============================================================
    // CREATE UDP SOCKET
    // ============================================================

    int sockfd =
        socket(
            AF_INET,
            SOCK_DGRAM,
            0
        );

    if (sockfd < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    // ============================================================
    // BIND ROV RECEIVER
    // ============================================================

    sockaddr_in rovAddress{};

    rovAddress.sin_family =
        AF_INET;

    rovAddress.sin_port =
        htons(PORT);

    rovAddress.sin_addr.s_addr =
        inet_addr(IP);

    if (bind(
            sockfd,
            reinterpret_cast<sockaddr*>(
                &rovAddress
            ),
            sizeof(rovAddress)) < 0)
    {
        perror("Bind failed");

        close(sockfd);
        return 1;
    }

    // ============================================================
    // START MESSAGE
    // ============================================================

    std::cout
        << "============================================\n";

    std::cout
        << "       U-ROV COMMUNICATION RECEIVER\n";

    std::cout
        << "============================================\n";

    std::cout
        << "Listening on "
        << IP
        << ":"
        << PORT
        << "\n";

    std::cout
        << "Waiting for CONTROL packet...\n\n";

    // ============================================================
    // RECEIVE BUFFER
    // ============================================================

    uint8_t buffer[2048];

    // ============================================================
    // SEQUENCE TRACKING
    // ============================================================

    bool hasProcessedControl = false;
    uint32_t lastProcessedSequence = 0;

    // ============================================================
    // MAIN RECEIVE LOOP
    // ============================================================

    while (true)
    {
        sockaddr_in surfaceAddress{};

        socklen_t surfaceLength =
            sizeof(surfaceAddress);

        // --------------------------------------------------------
        // RECEIVE UDP PACKET
        // --------------------------------------------------------

        ssize_t bytesReceived =
            recvfrom(
                sockfd,
                buffer,
                sizeof(buffer),
                0,
                reinterpret_cast<sockaddr*>(
                    &surfaceAddress
                ),
                &surfaceLength
            );

        if (bytesReceived < 0)
        {
            perror("recvfrom failed");
            continue;
        }

        std::cout
            << "--------------------------------------------\n";

        std::cout
            << "Packet received\n";

        std::cout
            << "Packet size: "
            << bytesReceived
            << " bytes\n";

        // ========================================================
        // PARSE CONTROL PACKET
        // ========================================================

        urov::PacketHeader receivedHeader{};
        urov::ControlPayload control{};

        if (!urov::parseControlPacket(
                buffer,
                static_cast<std::size_t>(
                    bytesReceived
                ),
                receivedHeader,
                control))
        {
            std::cout
                << "Invalid CONTROL packet\n\n";

            continue;
        }

        // ========================================================
        // DISPLAY CONTROL
        // ========================================================

        std::cout
            << "\nCONTROL packet parsed successfully\n";

        std::cout
            << "Message type   : "
            << static_cast<int>(
                receivedHeader.message_type
            )
            << "\n";

        std::cout
            << "Sequence       : "
            << receivedHeader.sequence_number
            << "\n";

        std::cout
            << "Payload size   : "
            << receivedHeader.payload_length
            << " bytes\n";

        std::cout
            << "Flags          : 0x"
            << std::hex
            << receivedHeader.flags
            << std::dec
            << "\n";

        std::cout
            << "Forward        : "
            << control.forward
            << "\n";

        std::cout
            << "Lateral        : "
            << control.lateral
            << "\n";

        std::cout
            << "Vertical       : "
            << control.vertical
            << "\n";

        std::cout
            << "Yaw            : "
            << control.yaw
            << "\n";

        std::cout
            << "Mode           : "
            << static_cast<int>(
                control.mode
            )
            << "\n";

        std::cout
            << "Emergency stop : "
            << static_cast<int>(
                control.emergency_stop
            )
            << "\n";

        std::cout
            << "Command ID     : "
            << control.command_id
            << "\n";

        // ========================================================
        // CHECK FOR DUPLICATE CONTROL
        // ========================================================

        bool duplicateControl = false;

        if (hasProcessedControl &&
            receivedHeader.sequence_number ==
                lastProcessedSequence)
        {
            duplicateControl = true;

            std::cout
                << "\nDUPLICATE CONTROL detected\n";

            std::cout
                << "Sequence "
                << receivedHeader.sequence_number
                << " was already processed\n";

            std::cout
                << "This is treated as a retransmission\n";

            std::cout
                << "CONTROL command will NOT be executed again\n";
        }

        // ========================================================
        // NEW CONTROL COMMAND
        // ========================================================

        if (!duplicateControl)
        {
            std::cout
                << "\nNEW CONTROL command accepted\n";

            std::cout
                << "Processing sequence "
                << receivedHeader.sequence_number
                << "\n";

            // ----------------------------------------------------
            // REAL ACTUATOR COMMAND WILL EVENTUALLY GO HERE
            // ----------------------------------------------------
            //
            // Example:
            //
            // thrusterController.setForward(control.forward);
            //
            // For now we only demonstrate protocol processing.
            //
            // ----------------------------------------------------

            lastProcessedSequence =
                receivedHeader.sequence_number;

            hasProcessedControl = true;
        }

        // ========================================================
        // ACK CONTROL PACKET
        // ========================================================

        if (receivedHeader.flags &
            urov::FLAG_ACK_REQ)
        {
            if (duplicateControl)
            {
                std::cout
                    << "\nACK requested for retransmission\n";
            }
            else
            {
                std::cout
                    << "\nACK requested by Surface\n";
            }

            // ----------------------------------------------------
            // CREATE ACK PAYLOAD
            // ----------------------------------------------------

            urov::AckPayload ack{};

            ack.acknowledged_sequence =
                receivedHeader.sequence_number;

            ack.acknowledged_message_type =
                receivedHeader.message_type;

            ack.status =
                static_cast<uint8_t>(
                    urov::AckStatus::SUCCESS
                );

            ack.reserved =
                0;

            // ----------------------------------------------------
            // ACK TIMESTAMP
            // ----------------------------------------------------

            uint64_t ackTimestamp =
                static_cast<uint64_t>(
                    std::chrono::duration_cast<
                        std::chrono::milliseconds
                    >(
                        std::chrono::steady_clock::now()
                            .time_since_epoch()
                    ).count()
                );

            // ----------------------------------------------------
            // BUILD ACK PACKET
            // ----------------------------------------------------

            uint8_t ackPacket[
                urov::ACK_PACKET_SIZE
            ];

            if (!urov::buildAckPacket(
                    ack,
                    receivedHeader.sequence_number,
                    ackTimestamp,
                    urov::FLAG_NONE,
                    ackPacket))
            {
                std::cout
                    << "Failed to build ACK packet\n";

                continue;
            }

            // ----------------------------------------------------
            // SEND ACK
            // ----------------------------------------------------

            ssize_t ackBytesSent =
                sendto(
                    sockfd,
                    ackPacket,
                    sizeof(ackPacket),
                    0,
                    reinterpret_cast<sockaddr*>(
                        &surfaceAddress
                    ),
                    surfaceLength
                );

            if (ackBytesSent < 0)
            {
                perror("ACK sendto failed");
                continue;
            }

            std::cout
                << "ACK sent successfully\n";

            std::cout
                << "ACK size       : "
                << ackBytesSent
                << " bytes\n";

            std::cout
                << "ACK sequence   : "
                << ack.acknowledged_sequence
                << "\n";

            std::cout
                << "ACK status     : "
                << urov::ackStatusToString(
                       static_cast<urov::AckStatus>(
                           ack.status
                       )
                   )
                << "\n";
        }
        else
        {
            std::cout
                << "\nACK not requested\n";
        }

        // ========================================================
        // GENERATE SIMULATED TELEMETRY
        // ========================================================

        urov::TelemetryPayload telemetry{};

        telemetry.depth_mm =
            12500;

        telemetry.roll_cdeg =
            150;

        telemetry.pitch_cdeg =
            -75;

        telemetry.yaw_cdeg =
            450;

        telemetry.battery_mV =
            14800;

        telemetry.temperature_cdeg =
            2350;

        telemetry.system_status =
            1;

        telemetry.reserved =
            0;

        // ========================================================
        // TELEMETRY SEQUENCE
        // ========================================================

        uint32_t telemetrySequence =
            receivedHeader.sequence_number;

        // ========================================================
        // TELEMETRY TIMESTAMP
        // ========================================================

        uint64_t telemetryTimestamp =
            static_cast<uint64_t>(
                std::chrono::duration_cast<
                    std::chrono::milliseconds
                >(
                    std::chrono::steady_clock::now()
                        .time_since_epoch()
                ).count()
            );

        // ========================================================
        // BUILD TELEMETRY PACKET
        // ========================================================

        uint8_t telemetryPacket[
            urov::TELEMETRY_PACKET_SIZE
        ];

        if (!urov::buildTelemetryPacket(
                telemetry,
                telemetrySequence,
                telemetryTimestamp,
                urov::FLAG_NONE,
                telemetryPacket))
        {
            std::cout
                << "Failed to build TELEMETRY packet\n";

            continue;
        }

        // ========================================================
        // SEND TELEMETRY
        // ========================================================

        ssize_t telemetryBytesSent =
            sendto(
                sockfd,
                telemetryPacket,
                sizeof(telemetryPacket),
                0,
                reinterpret_cast<sockaddr*>(
                    &surfaceAddress
                ),
                surfaceLength
            );

        if (telemetryBytesSent < 0)
        {
            perror("TELEMETRY sendto failed");
            continue;
        }

        std::cout
            << "\nTELEMETRY packet sent\n";

        std::cout
            << "Packet size: "
            << telemetryBytesSent
            << " bytes\n";

        std::cout
            << "Sequence   : "
            << telemetrySequence
            << "\n";

        std::cout
            << "Depth      : "
            << telemetry.depth_mm
            << " mm\n";

        std::cout
            << "Battery    : "
            << telemetry.battery_mV
            << " mV\n";

        std::cout
            << "--------------------------------------------\n\n";
    }

    // ============================================================
    // CLOSE SOCKET
    // ============================================================

    close(sockfd);

    return 0;
}