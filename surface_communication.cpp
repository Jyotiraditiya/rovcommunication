#include <iostream>
#include <cstdint>
#include <chrono>
#include <cerrno>
#include <cstring>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "packet.hpp"

int main()
{
    const char* ROV_IP = "127.0.0.1";
    const int ROV_PORT = 5000;

    // ============================================================
    // RELIABILITY PARAMETERS
    // ============================================================

    constexpr int ACK_TIMEOUT_SECONDS = 2;
    constexpr int MAX_RETRIES = 3;

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
    // RECEIVE TIMEOUT
    // ============================================================

    timeval timeout{};

    timeout.tv_sec = ACK_TIMEOUT_SECONDS;
    timeout.tv_usec = 0;

    if (setsockopt(
            sockfd,
            SOL_SOCKET,
            SO_RCVTIMEO,
            &timeout,
            sizeof(timeout)) < 0)
    {
        perror("setsockopt failed");
        close(sockfd);
        return 1;
    }

    // ============================================================
    // ROV DESTINATION ADDRESS
    // ============================================================

    sockaddr_in rovAddress{};

    rovAddress.sin_family =
        AF_INET;

    rovAddress.sin_port =
        htons(ROV_PORT);

    rovAddress.sin_addr.s_addr =
        inet_addr(ROV_IP);

    // ============================================================
    // CREATE CONTROL PAYLOAD
    // ============================================================

    urov::ControlPayload control{};

    control.forward =
        1000;

    control.lateral =
        -200;

    control.vertical =
        300;

    control.yaw =
        -400;

    control.mode =
        2;

    control.emergency_stop =
        0;

    control.reserved =
        0;

    control.command_id =
        12345;

    // ============================================================
    // SEQUENCE NUMBER
    // ============================================================

    // In the next stage this can become a persistent sequence
    // manager. For this test program, start at 1.
    uint32_t controlSequence = 1;

    // ============================================================
    // TIMESTAMP
    // ============================================================

    uint64_t timestamp =
        static_cast<uint64_t>(
            std::chrono::duration_cast<
                std::chrono::milliseconds
            >(
                std::chrono::steady_clock::now()
                    .time_since_epoch()
            ).count()
        );

    // ============================================================
    // BUILD CONTROL PACKET
    // ============================================================

    uint8_t controlPacket[
        urov::CONTROL_PACKET_SIZE
    ];

    if (!urov::buildControlPacket(
            control,
            controlSequence,
            timestamp,
            urov::FLAG_ACK_REQ,
            controlPacket))
    {
        std::cout
            << "Failed to build CONTROL packet\n";

        close(sockfd);
        return 1;
    }

    // ============================================================
    // DISPLAY INFORMATION
    // ============================================================

    std::cout
        << "============================================\n";

    std::cout
        << "        U-ROV SURFACE COMMUNICATION\n";

    std::cout
        << "============================================\n";

    std::cout
        << "\nCONTROL packet\n";

    std::cout
        << "Packet size    : "
        << sizeof(controlPacket)
        << " bytes\n";

    std::cout
        << "Sequence       : "
        << controlSequence
        << "\n";

    std::cout
        << "ACK requested  : YES\n";

    std::cout
        << "ACK timeout    : "
        << ACK_TIMEOUT_SECONDS
        << " seconds\n";

    std::cout
        << "Maximum retries: "
        << MAX_RETRIES
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
        << "Command ID     : "
        << control.command_id
        << "\n";

    // ============================================================
    // ACK/RETRY LOOP
    // ============================================================

    uint8_t response[2048];

    bool communicationSuccess = false;
    int attemptsUsed = 0;

    sockaddr_in senderAddress{};
    socklen_t senderLength =
        sizeof(senderAddress);

    for (int attempt = 1;
         attempt <= MAX_RETRIES;
         ++attempt)
    {
        attemptsUsed = attempt;

        std::cout
            << "\n--------------------------------------------\n";

        std::cout
            << "CONTROL transmission attempt "
            << attempt
            << "/"
            << MAX_RETRIES
            << "\n";

        std::cout
            << "Sequence: "
            << controlSequence
            << "\n";

        // --------------------------------------------------------
        // SEND CONTROL
        // --------------------------------------------------------

        ssize_t bytesSent =
            sendto(
                sockfd,
                controlPacket,
                sizeof(controlPacket),
                0,
                reinterpret_cast<sockaddr*>(
                    &rovAddress
                ),
                sizeof(rovAddress)
            );

        if (bytesSent < 0)
        {
            perror("sendto failed");
            close(sockfd);
            return 1;
        }

        std::cout
            << "CONTROL sent: "
            << bytesSent
            << " bytes\n";

        std::cout
            << "Waiting for ACK...\n";

        // --------------------------------------------------------
        // RECEIVE ACK
        // --------------------------------------------------------

        senderLength =
            sizeof(senderAddress);

        ssize_t bytesReceived =
            recvfrom(
                sockfd,
                response,
                sizeof(response),
                0,
                reinterpret_cast<sockaddr*>(
                    &senderAddress
                ),
                &senderLength
            );

        if (bytesReceived < 0)
        {
            if (errno == EAGAIN ||
                errno == EWOULDBLOCK)
            {
                std::cout
                    << "ACK timeout on attempt "
                    << attempt
                    << "\n";

                if (attempt < MAX_RETRIES)
                {
                    std::cout
                        << "Retrying CONTROL...\n";

                    continue;
                }

                std::cout
                    << "\nERROR: Maximum retries reached\n";

                break;
            }

            perror("recvfrom failed");
            close(sockfd);
            return 1;
        }

        std::cout
            << "\nPacket received while waiting for ACK\n";

        std::cout
            << "Packet size: "
            << bytesReceived
            << " bytes\n";

        // --------------------------------------------------------
        // PARSE ACK
        // --------------------------------------------------------

        urov::PacketHeader ackHeader{};
        urov::AckPayload ack{};

        if (!urov::parseAckPacket(
                response,
                static_cast<std::size_t>(
                    bytesReceived
                ),
                ackHeader,
                ack))
        {
            std::cout
                << "Received packet is not a valid ACK\n";

            std::cout
                << "Ignoring packet and waiting for ACK...\n";

            if (attempt < MAX_RETRIES)
            {
                continue;
            }

            break;
        }

        // --------------------------------------------------------
        // DISPLAY ACK
        // --------------------------------------------------------

        std::cout
            << "\nACK packet parsed successfully\n";

        std::cout
            << "Message type               : "
            << static_cast<int>(
                ackHeader.message_type
            )
            << "\n";

        std::cout
            << "ACK header sequence        : "
            << ackHeader.sequence_number
            << "\n";

        std::cout
            << "Payload size               : "
            << ackHeader.payload_length
            << " bytes\n";

        std::cout
            << "Acknowledged sequence      : "
            << ack.acknowledged_sequence
            << "\n";

        std::cout
            << "Acknowledged message type : "
            << static_cast<int>(
                ack.acknowledged_message_type
            )
            << "\n";

        std::cout
            << "ACK status                 : "
            << urov::ackStatusToString(
                static_cast<urov::AckStatus>(
                    ack.status
                )
            )
            << "\n";

        // --------------------------------------------------------
        // VALIDATE ACK
        // --------------------------------------------------------

        bool ackValid = true;

        if (ack.acknowledged_sequence !=
            controlSequence)
        {
            std::cout
                << "\nERROR: ACK sequence mismatch\n";

            std::cout
                << "Expected: "
                << controlSequence
                << "\n";

            std::cout
                << "Received: "
                << ack.acknowledged_sequence
                << "\n";

            ackValid = false;
        }

        if (ackHeader.sequence_number !=
            controlSequence)
        {
            std::cout
                << "\nERROR: ACK header sequence mismatch\n";

            ackValid = false;
        }

        if (ack.acknowledged_message_type !=
            static_cast<uint8_t>(
                urov::MessageType::CONTROL
            ))
        {
            std::cout
                << "\nERROR: ACK message type mismatch\n";

            ackValid = false;
        }

        if (ack.status !=
            static_cast<uint8_t>(
                urov::AckStatus::SUCCESS
            ))
        {
            std::cout
                << "\nERROR: CONTROL was not accepted\n";

            ackValid = false;
        }

        if (!ackValid)
        {
            std::cout
                << "\nACK validation FAILED\n";

            if (attempt < MAX_RETRIES)
            {
                std::cout
                    << "Retrying CONTROL...\n";

                continue;
            }

            break;
        }

        // --------------------------------------------------------
        // ACK SUCCESS
        // --------------------------------------------------------

        std::cout
            << "\nACK validation PASSED\n";

        std::cout
            << "CONTROL sequence "
            << controlSequence
            << " confirmed by ROV\n";

        communicationSuccess = true;

        break;
    }

    // ============================================================
    // STOP IF ACK FAILED
    // ============================================================

    if (!communicationSuccess)
    {
        std::cout
            << "\n============================================\n";

        std::cout
            << "COMMUNICATION FAILED\n";

        std::cout
            << "Sequence           : "
            << controlSequence
            << "\n";

        std::cout
            << "Attempts used      : "
            << attemptsUsed
            << "\n";

        std::cout
            << "============================================\n";

        close(sockfd);
        return 1;
    }

    // ============================================================
    // RECEIVE TELEMETRY
    // ============================================================

    std::cout
        << "\nWaiting for TELEMETRY...\n";

    senderLength =
        sizeof(senderAddress);

    ssize_t bytesReceived =
        recvfrom(
            sockfd,
            response,
            sizeof(response),
            0,
            reinterpret_cast<sockaddr*>(
                &senderAddress
            ),
            &senderLength
        );

    if (bytesReceived < 0)
    {
        if (errno == EAGAIN ||
            errno == EWOULDBLOCK)
        {
            std::cout
                << "\nERROR: TELEMETRY timeout\n";

            close(sockfd);
            return 1;
        }

        perror("recvfrom failed");

        close(sockfd);
        return 1;
    }

    std::cout
        << "\nTELEMETRY packet received\n";

    std::cout
        << "Packet size: "
        << bytesReceived
        << " bytes\n";

    // ============================================================
    // PARSE TELEMETRY
    // ============================================================

    urov::PacketHeader telemetryHeader{};
    urov::TelemetryPayload telemetry{};

    if (!urov::parseTelemetryPacket(
            response,
            static_cast<std::size_t>(
                bytesReceived
            ),
            telemetryHeader,
            telemetry))
    {
        std::cout
            << "Invalid TELEMETRY packet\n";

        close(sockfd);
        return 1;
    }

    // ============================================================
    // DISPLAY TELEMETRY
    // ============================================================

    std::cout
        << "\nTELEMETRY packet parsed successfully\n";

    std::cout
        << "Message type   : "
        << static_cast<int>(
            telemetryHeader.message_type
        )
        << "\n";

    std::cout
        << "Sequence       : "
        << telemetryHeader.sequence_number
        << "\n";

    std::cout
        << "Payload size   : "
        << telemetryHeader.payload_length
        << " bytes\n";

    std::cout
        << "Depth          : "
        << telemetry.depth_mm
        << " mm\n";

    std::cout
        << "Roll           : "
        << telemetry.roll_cdeg
        << " cdeg\n";

    std::cout
        << "Pitch          : "
        << telemetry.pitch_cdeg
        << " cdeg\n";

    std::cout
        << "Yaw            : "
        << telemetry.yaw_cdeg
        << " cdeg\n";

    std::cout
        << "Battery        : "
        << telemetry.battery_mV
        << " mV\n";

    std::cout
        << "Temperature    : "
        << telemetry.temperature_cdeg
        << " cdeg\n";

    std::cout
        << "System status  : "
        << static_cast<int>(
            telemetry.system_status
        )
        << "\n";

    // ============================================================
    // FINAL RESULT
    // ============================================================

    std::cout
        << "\n============================================\n";

    std::cout
        << "BIDIRECTIONAL COMMUNICATION TEST PASSED\n";

    std::cout
        << "CONTROL -> ROV          : OK\n";

    std::cout
        << "ACK ROV -> SURFACE      : OK\n";

    std::cout
        << "Sequence validation     : OK\n";

    std::cout
        << "Retry mechanism         : READY\n";

    std::cout
        << "TELEMETRY ROV -> SURFACE: OK\n";

    std::cout
        << "Attempts used           : "
        << attemptsUsed
        << "\n";

    std::cout
        << "============================================\n";

    // ============================================================
    // CLOSE SOCKET
    // ============================================================

    close(sockfd);

    return 0;
}