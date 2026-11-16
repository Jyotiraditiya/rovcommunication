#include <iostream>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <algorithm>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>


// ============================================================
// CONFIGURATION
// ============================================================

const int TOTAL_PACKETS = 1000;

// Number of packets sent per second
const int PACKET_RATE = 100;

// Requested packet size in bytes
const int PACKET_SIZE = 1400;

// ROV receiver
const char* RECEIVER_IP = "127.0.0.1";
const int RECEIVER_PORT = 5000;

// Maximum time to wait for an echo
const int RESPONSE_TIMEOUT_MS = 50;


// ============================================================
// GET CURRENT TIME IN NANOSECONDS
// ============================================================

long long getTimestampNs()
{
    auto now = std::chrono::steady_clock::now();

    return std::chrono::duration_cast<
        std::chrono::nanoseconds
    >(
        now.time_since_epoch()
    ).count();
}


// ============================================================
// PACKET STRUCTURE
//
// Example:
//
// SEQ=25;TIME=123456789;DEPTH=12.40;XXXXX...
//
// ============================================================

std::string createPacket(
    int sequenceNumber,
    long long timestamp,
    int packetSize)
{
    std::string packet =
        "SEQ=" +
        std::to_string(sequenceNumber) +
        ";TIME=" +
        std::to_string(timestamp) +
        ";DEPTH=12.40;";


    // Pad packet to requested size
    if (packet.size() < static_cast<size_t>(packetSize))
    {
        packet += std::string(
            packetSize - packet.size(),
            'X'
        );
    }

    // If header itself exceeds requested size,
    // send the complete header rather than truncating it.
    return packet;
}


// ============================================================
// EXTRACT SEQUENCE NUMBER
// ============================================================

int extractSequenceNumber(const std::string& packet)
{
    const std::string key = "SEQ=";

    size_t start = packet.find(key);

    if (start == std::string::npos)
        return -1;

    start += key.length();

    size_t end = packet.find(';', start);

    if (end == std::string::npos)
        return -1;

    try
    {
        return std::stoi(
            packet.substr(start, end - start)
        );
    }
    catch (...)
    {
        return -1;
    }
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
    // PRINT TEST CONFIGURATION
    // ========================================================

    std::cout << "\n";
    std::cout
        << "============================================\n";

    std::cout
        << "        U-ROV UDP MEASUREMENT TEST\n";

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
    // START TEST TIMER
    // ========================================================

    auto testStart =
        std::chrono::steady_clock::now();


    // ========================================================
    // MAIN TEST LOOP
    // ========================================================

    for (int sequence = 0;
         sequence < TOTAL_PACKETS;
         sequence++)
    {
        // ----------------------------------------------------
        // Timestamp immediately before sending
        // ----------------------------------------------------

        auto sendTime =
            std::chrono::steady_clock::now();

        long long timestamp =
            std::chrono::duration_cast<
                std::chrono::nanoseconds
            >(
                sendTime.time_since_epoch()
            ).count();


        // ----------------------------------------------------
        // Create packet
        // ----------------------------------------------------

        std::string packet =
            createPacket(
                sequence,
                timestamp,
                PACKET_SIZE
            );


        // ----------------------------------------------------
        // Send UDP packet
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
        // Wait for response
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


            // Calculate remaining timeout
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
            // Receive packet
            // ------------------------------------------------

            char buffer[4096];

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
            // Convert to string
            // ------------------------------------------------

            std::string receivedPacket(
                buffer,
                bytesReceived
            );


            // ------------------------------------------------
            // Determine sequence number
            // ------------------------------------------------

            int receivedSequence =
                extractSequenceNumber(
                    receivedPacket
                );


            // ------------------------------------------------
            // Ignore unexpected packet
            // ------------------------------------------------

            if (receivedSequence != sequence)
            {
                unexpectedPackets++;

                continue;
            }


            // ------------------------------------------------
            // Correct response received
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
            // RTT statistics
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
            // RTT variation
            // ------------------------------------------------

            if (previousRTT >= 0)
            {
                double variation =
                    std::abs(
                        rtt - previousRTT
                    );

                totalRTTVariation +=
                    variation;

                variationSamples++;
            }

            previousRTT = rtt;


            // ------------------------------------------------
            // Print first 10 packets
            // ------------------------------------------------

            if (sequence < 10)
            {
                std::cout
                    << "SEQ "
                    << std::setw(4)
                    << sequence
                    << " | RTT = "
                    << std::fixed
                    << std::setprecision(3)
                    << rtt
                    << " ms\n";
            }
        }


        // ----------------------------------------------------
        // Timeout
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
        // Packet rate control
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
    // END TEST TIMER
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

    double packetLoss =
        0.0;


    if (packetsSent > 0)
    {
        packetLoss =
            100.0 *
            packetsLost /
            packetsSent;
    }


    double averageRTT =
        0.0;


    if (packetsReceived > 0)
    {
        averageRTT =
            totalRTT /
            packetsReceived;
    }


    double averageRTTVariation =
        0.0;


    if (variationSamples > 0)
    {
        averageRTTVariation =
            totalRTTVariation /
            variationSamples;
    }


    // --------------------------------------------------------
    // Received throughput
    // --------------------------------------------------------

    double receivedThroughputMbps =
        0.0;


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


    // --------------------------------------------------------
    // Offered throughput
    // --------------------------------------------------------

    double offeredThroughputMbps =
        (
            PACKET_RATE *
            PACKET_SIZE *
            8.0
        )
        /
        1000000.0;


    // ========================================================
    // PRINT FINAL RESULTS
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


    // ========================================================
    // CLOSE SOCKET
    // ========================================================

    close(sockfd);

    return 0;
}