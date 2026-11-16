#include <iostream>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <cstdint>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

#include "protocol.hpp"


// ============================================================
// CONFIGURATION
// ============================================================

const int TOTAL_PACKETS = 1000;

const int PACKET_RATE = 100;

const int PACKET_SIZE = 1400;

const char* RECEIVER_IP = "127.0.0.1";

const int RECEIVER_PORT = 5000;

const int RESPONSE_TIMEOUT_MS = 50;


// ============================================================
// WRITE UINT16
// ============================================================

void writeUint16(
    std::vector<uint8_t>& buffer,
    std::size_t offset,
    uint16_t value)
{
    buffer[offset + 0] =
        static_cast<uint8_t>((value >> 8) & 0xFF);

    buffer[offset + 1] =
        static_cast<uint8_t>(value & 0xFF);
}


// ============================================================
// WRITE UINT32
// ============================================================

void writeUint32(
    std::vector<uint8_t>& buffer,
    std::size_t offset,
    uint32_t value)
{
    buffer[offset + 0] =
        static_cast<uint8_t>((value >> 24) & 0xFF);

    buffer[offset + 1] =
        static_cast<uint8_t>((value >> 16) & 0xFF);

    buffer[offset + 2] =
        static_cast<uint8_t>((value >> 8) & 0xFF);

    buffer[offset + 3] =
        static_cast<uint8_t>(value & 0xFF);
}


// ============================================================
// WRITE UINT64
// ============================================================

void writeUint64(
    std::vector<uint8_t>& buffer,
    std::size_t offset,
    uint64_t value)
{
    for (int i = 0; i < 8; ++i)
    {
        buffer[offset + i] =
            static_cast<uint8_t>(
                (value >> (56 - 8 * i)) & 0xFF
            );
    }
}


// ============================================================
// READ UINT16
// ============================================================

uint16_t readUint16(
    const uint8_t* data,
    std::size_t offset)
{
    return
        (static_cast<uint16_t>(data[offset]) << 8) |
        static_cast<uint16_t>(data[offset + 1]);
}


// ============================================================
// READ UINT32
// ============================================================

uint32_t readUint32(
    const uint8_t* data,
    std::size_t offset)
{
    return
        (static_cast<uint32_t>(data[offset]) << 24) |
        (static_cast<uint32_t>(data[offset + 1]) << 16) |
        (static_cast<uint32_t>(data[offset + 2]) << 8) |
        static_cast<uint32_t>(data[offset + 3]);
}


// ============================================================
// CREATE U-ROV PACKET
// ============================================================
//
// Total packet:
//     32 bytes header
//     1368 bytes payload
//     = 1400 bytes
//
// ============================================================

std::vector<uint8_t> createUrovPacket(
    uint32_t sequenceNumber,
    uint64_t timestamp,
    std::size_t totalPacketSize)
{
    if (totalPacketSize < urov::HEADER_SIZE)
    {
        throw std::runtime_error(
            "Packet size is smaller than U-ROV header"
        );
    }

    const std::size_t payloadSize =
        totalPacketSize - urov::HEADER_SIZE;

    std::vector<uint8_t> packet(
        totalPacketSize,
        static_cast<uint8_t>('X')
    );


    // --------------------------------------------------------
    // HEADER
    // --------------------------------------------------------

    // Bytes 0-1
    writeUint16(
        packet,
        0,
        urov::MAGIC
    );


    // Byte 2
    packet[2] = urov::VERSION;


    // Byte 3
    packet[3] =
        static_cast<uint8_t>(
            urov::MessageType::CONTROL
        );


    // Bytes 4-5
    writeUint16(
        packet,
        4,
        static_cast<uint16_t>(
            urov::HEADER_SIZE
        )
    );


    // Bytes 6-7
    writeUint16(
        packet,
        6,
        static_cast<uint16_t>(
            payloadSize
        )
    );


    // Bytes 8-9
    writeUint16(
        packet,
        8,
        urov::FLAG_NONE
    );


    // Bytes 10-13
    writeUint32(
        packet,
        10,
        sequenceNumber
    );


    // Bytes 14-21
    writeUint64(
        packet,
        14,
        timestamp
    );


    // Bytes 22-25
    // CRC/checksum will be implemented later.
    writeUint32(
        packet,
        22,
        0
    );


    // Bytes 26-31
    // Reserved for future protocol fields.
    for (std::size_t i = 26; i < 32; ++i)
    {
        packet[i] = 0;
    }


    // --------------------------------------------------------
    // PAYLOAD
    // --------------------------------------------------------

    const std::string payload =
        "U-ROV CONTROL PACKET";

    for (
        std::size_t i = 0;
        i < payload.size() && i < payloadSize;
        ++i)
    {
        packet[
            urov::HEADER_SIZE + i
        ] = static_cast<uint8_t>(
            payload[i]
        );
    }


    return packet;
}


// ============================================================
// MAIN
// ============================================================

int main()
{
    // ========================================================
    // CREATE UDP SOCKET
    // ========================================================

    int sockfd = socket(
        AF_INET,
        SOCK_DGRAM,
        0
    );

    if (sockfd < 0)
    {
        perror("Socket creation failed");
        return 1;
    }


    // ========================================================
    // ROV ADDRESS
    // ========================================================

    sockaddr_in rovAddress{};

    rovAddress.sin_family = AF_INET;

    rovAddress.sin_port =
        htons(RECEIVER_PORT);


    if (inet_pton(
            AF_INET,
            RECEIVER_IP,
            &rovAddress.sin_addr) <= 0)
    {
        std::cerr
            << "Invalid receiver IP address\n";

        close(sockfd);

        return 1;
    }


    // ========================================================
    // STATISTICS
    // ========================================================

    int packetsSent = 0;
    int packetsReceived = 0;
    int packetsLost = 0;
    int unexpectedPackets = 0;

    double totalRTT = 0.0;
    double minRTT = 1e9;
    double maxRTT = 0.0;

    double totalRTTVariation = 0.0;
    double previousRTT = -1.0;

    int variationSamples = 0;

    long long receivedBytes = 0;


    // ========================================================
    // TEST CONFIGURATION
    // ========================================================

    std::cout << "\n";
    std::cout
        << "============================================\n";

    std::cout
        << "       U-ROV UDP PROTOCOL TEST\n";

    std::cout
        << "============================================\n";

    std::cout
        << "Destination       : "
        << RECEIVER_IP
        << ":"
        << RECEIVER_PORT
        << "\n";

    std::cout
        << "Packet rate       : "
        << PACKET_RATE
        << " packets/s\n";

    std::cout
        << "Packet size       : "
        << PACKET_SIZE
        << " bytes\n";

    std::cout
        << "Header size       : "
        << urov::HEADER_SIZE
        << " bytes\n";

    std::cout
        << "Payload size      : "
        << PACKET_SIZE - urov::HEADER_SIZE
        << " bytes\n";

    std::cout
        << "Total packets     : "
        << TOTAL_PACKETS
        << "\n";

    std::cout
        << "Response timeout  : "
        << RESPONSE_TIMEOUT_MS
        << " ms\n";

    std::cout
        << "--------------------------------------------\n";


    // ========================================================
    // TEST TIMER
    // ========================================================

    auto testStart =
        std::chrono::steady_clock::now();


    // ========================================================
    // MAIN TEST LOOP
    // ========================================================

    for (
        int sequence = 0;
        sequence < TOTAL_PACKETS;
        sequence++)
    {
        // ----------------------------------------------------
        // SEND TIMESTAMP
        // ----------------------------------------------------

        auto sendTime =
            std::chrono::steady_clock::now();

        uint64_t timestamp =
            static_cast<uint64_t>(
                std::chrono::duration_cast<
                    std::chrono::nanoseconds
                >(
                    sendTime.time_since_epoch()
                ).count()
            );


        // ----------------------------------------------------
        // CREATE U-ROV PACKET
        // ----------------------------------------------------

        std::vector<uint8_t> packet =
            createUrovPacket(
                static_cast<uint32_t>(sequence),
                timestamp,
                PACKET_SIZE
            );


        // ----------------------------------------------------
        // SEND
        // ----------------------------------------------------

        ssize_t bytesSent = sendto(
            sockfd,
            packet.data(),
            packet.size(),
            0,
            reinterpret_cast<sockaddr*>(
                &rovAddress
            ),
            sizeof(rovAddress)
        );


        if (bytesSent < 0)
        {
            perror("sendto failed");
            continue;
        }


        packetsSent++;


        // ----------------------------------------------------
        // WAIT FOR ECHO
        // ----------------------------------------------------

        bool correctResponseReceived = false;


        auto responseDeadline =
            std::chrono::steady_clock::now()
            +
            std::chrono::milliseconds(
                RESPONSE_TIMEOUT_MS
            );


        while (!correctResponseReceived)
        {
            auto currentTime =
                std::chrono::steady_clock::now();


            auto remaining =
                std::chrono::duration_cast<
                    std::chrono::microseconds
                >(
                    responseDeadline - currentTime
                );


            if (remaining.count() <= 0)
            {
                break;
            }


            fd_set readfds;

            FD_ZERO(&readfds);

            FD_SET(sockfd, &readfds);


            timeval timeout{};

            timeout.tv_sec =
                remaining.count() / 1000000;

            timeout.tv_usec =
                remaining.count() % 1000000;


            int ready = select(
                sockfd + 1,
                &readfds,
                nullptr,
                nullptr,
                &timeout
            );


            if (ready < 0)
            {
                perror("select failed");
                break;
            }


            if (ready == 0)
            {
                break;
            }


            // ------------------------------------------------
            // RECEIVE
            // ------------------------------------------------

            uint8_t buffer[4096];

            sockaddr_in senderAddress{};

            socklen_t senderLength =
                sizeof(senderAddress);


            ssize_t bytesReceived =
                recvfrom(
                    sockfd,
                    buffer,
                    sizeof(buffer),
                    0,
                    reinterpret_cast<sockaddr*>(
                        &senderAddress
                    ),
                    &senderLength
                );


            if (bytesReceived <= 0)
            {
                continue;
            }


            // ------------------------------------------------
            // BASIC PACKET VALIDATION
            // ------------------------------------------------

            if (
                bytesReceived <
                static_cast<ssize_t>(
                    urov::HEADER_SIZE
                )
            )
            {
                unexpectedPackets++;
                continue;
            }


            // ------------------------------------------------
            // READ HEADER
            // ------------------------------------------------

            uint16_t magic =
                readUint16(buffer, 0);

            uint8_t version =
                buffer[2];

            uint8_t messageType =
                buffer[3];

            uint16_t headerLength =
                readUint16(buffer, 4);

            uint16_t payloadLength =
                readUint16(buffer, 6);

            uint32_t receivedSequence =
                readUint32(buffer, 10);


            // ------------------------------------------------
            // VALIDATE MAGIC
            // ------------------------------------------------

            if (magic != urov::MAGIC)
            {
                unexpectedPackets++;
                continue;
            }


            // ------------------------------------------------
            // VALIDATE VERSION
            // ------------------------------------------------

            if (version != urov::VERSION)
            {
                unexpectedPackets++;
                continue;
            }


            // ------------------------------------------------
            // VALIDATE HEADER
            // ------------------------------------------------

            if (
                headerLength !=
                urov::HEADER_SIZE
            )
            {
                unexpectedPackets++;
                continue;
            }


            // ------------------------------------------------
            // VALIDATE PAYLOAD LENGTH
            // ------------------------------------------------

            if (
                payloadLength +
                headerLength !=
                static_cast<uint16_t>(
                    bytesReceived
                )
            )
            {
                unexpectedPackets++;
                continue;
            }


            // ------------------------------------------------
            // VALIDATE MESSAGE TYPE
            // ------------------------------------------------

            if (
                messageType !=
                static_cast<uint8_t>(
                    urov::MessageType::CONTROL
                )
            )
            {
                unexpectedPackets++;
                continue;
            }


            // ------------------------------------------------
            // CHECK SEQUENCE
            // ------------------------------------------------

            if (
                receivedSequence !=
                static_cast<uint32_t>(
                    sequence
                )
            )
            {
                unexpectedPackets++;
                continue;
            }


            // ------------------------------------------------
            // CORRECT RESPONSE
            // ------------------------------------------------

            auto receiveTime =
                std::chrono::steady_clock::now();


            double rtt =
                std::chrono::duration<double, std::milli>(
                    receiveTime - sendTime
                ).count();


            packetsReceived++;

            receivedBytes += bytesReceived;

            correctResponseReceived = true;


            // ------------------------------------------------
            // RTT STATISTICS
            // ------------------------------------------------

            totalRTT += rtt;

            minRTT =
                std::min(
                    minRTT,
                    rtt
                );

            maxRTT =
                std::max(
                    maxRTT,
                    rtt
                );


            // ------------------------------------------------
            // RTT VARIATION
            // ------------------------------------------------

            if (previousRTT >= 0)
            {
                double variation =
                    std::abs(
                        rtt -
                        previousRTT
                    );

                totalRTTVariation +=
                    variation;

                variationSamples++;
            }


            previousRTT = rtt;


            // ------------------------------------------------
            // PRINT FIRST 10
            // ------------------------------------------------

            if (sequence < 10)
            {
                std::cout
                    << "SEQ "
                    << std::setw(4)
                    << sequence
                    << " | TYPE = "
                    << urov::messageTypeToString(
                        static_cast<
                            urov::MessageType
                        >(messageType)
                    )
                    << " | RTT = "
                    << std::fixed
                    << std::setprecision(3)
                    << rtt
                    << " ms\n";
            }
        }


        // ----------------------------------------------------
        // TIMEOUT
        // ----------------------------------------------------

        if (!correctResponseReceived)
        {
            packetsLost++;

            if (sequence < 20)
            {
                std::cout
                    << "SEQ "
                    << std::setw(4)
                    << sequence
                    << " | TIMEOUT\n";
            }
        }


        // ----------------------------------------------------
        // PACKET RATE
        // ----------------------------------------------------

        const int intervalMs =
            1000 / PACKET_RATE;


        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                intervalMs
            )
        );
    }


    // ========================================================
    // END TIMER
    // ========================================================

    auto testEnd =
        std::chrono::steady_clock::now();


    double testDuration =
        std::chrono::duration<double>(
            testEnd - testStart
        ).count();


    // ========================================================
    // CALCULATE RESULTS
    // ========================================================

    double packetLoss = 0.0;

    if (packetsSent > 0)
    {
        packetLoss =
            100.0 *
            packetsLost /
            packetsSent;
    }


    double averageRTT = 0.0;

    if (packetsReceived > 0)
    {
        averageRTT =
            totalRTT /
            packetsReceived;
    }


    double averageRTTVariation = 0.0;

    if (variationSamples > 0)
    {
        averageRTTVariation =
            totalRTTVariation /
            variationSamples;
    }


    // ========================================================
    // THROUGHPUT
    // ========================================================

    double receivedThroughputMbps = 0.0;

    if (testDuration > 0)
    {
        receivedThroughputMbps =
            (
                receivedBytes *
                8.0
            )
            /
            (
                testDuration *
                1000000.0
            );
    }


    double offeredThroughputMbps =
        (
            PACKET_RATE *
            PACKET_SIZE *
            8.0
        )
        /
        1000000.0;


    // ========================================================
    // RESULTS
    // ========================================================

    std::cout << "\n";

    std::cout
        << "============================================\n";

    std::cout
        << "                 RESULTS\n";

    std::cout
        << "============================================\n";

    std::cout
        << "Packets sent          : "
        << packetsSent
        << "\n";

    std::cout
        << "Packets received      : "
        << packetsReceived
        << "\n";

    std::cout
        << "Packets lost          : "
        << packetsLost
        << "\n";

    std::cout
        << "Unexpected packets    : "
        << unexpectedPackets
        << "\n";

    std::cout
        << "Packet loss           : "
        << std::fixed
        << std::setprecision(3)
        << packetLoss
        << " %\n";

    std::cout
        << "Average RTT           : "
        << averageRTT
        << " ms\n";

    std::cout
        << "Minimum RTT           : "
        << minRTT
        << " ms\n";

    std::cout
        << "Maximum RTT           : "
        << maxRTT
        << " ms\n";

    std::cout
        << "Mean RTT variation    : "
        << averageRTTVariation
        << " ms\n";

    std::cout
        << "Test duration         : "
        << testDuration
        << " s\n";

    std::cout
        << "Offered throughput    : "
        << offeredThroughputMbps
        << " Mbps\n";

    std::cout
        << "Received throughput   : "
        << receivedThroughputMbps
        << " Mbps\n";

    std::cout
        << "============================================\n";


    close(sockfd);

    return 0;
}