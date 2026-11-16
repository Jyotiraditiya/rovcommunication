#include <iostream>
#include <cstdint>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "protocol.hpp"


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
        htons(5000);

    rovAddress.sin_addr.s_addr =
        inet_addr("127.0.0.1");


    // ========================================================
    // BIND
    // ========================================================

    if (
        bind(
            sockfd,
            reinterpret_cast<sockaddr*>(
                &rovAddress
            ),
            sizeof(rovAddress)
        ) < 0
    )
    {
        perror("Bind failed");

        close(sockfd);

        return 1;
    }


    std::cout
        << "ROV receiver listening on "
        << "127.0.0.1:5000\n";


    uint8_t buffer[4096];


    // ========================================================
    // RECEIVE LOOP
    // ========================================================

    while (true)
    {
        sockaddr_in surfaceAddress{};

        socklen_t surfaceLength =
            sizeof(surfaceAddress);


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


        // ====================================================
        // CHECK HEADER SIZE
        // ====================================================

        if (
            bytesReceived <
            static_cast<ssize_t>(
                urov::HEADER_SIZE
            )
        )
        {
            std::cout
                << "Packet too small: "
                << bytesReceived
                << " bytes\n";

            continue;
        }


        // ====================================================
        // READ HEADER
        // ====================================================

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

        uint16_t flags =
            readUint16(buffer, 8);

        uint32_t sequence =
            readUint32(buffer, 10);


        // ====================================================
        // VALIDATE MAGIC
        // ====================================================

        if (magic != urov::MAGIC)
        {
            std::cout
                << "Invalid MAGIC\n";

            continue;
        }


        // ====================================================
        // VALIDATE VERSION
        // ====================================================

        if (version != urov::VERSION)
        {
            std::cout
                << "Unsupported protocol version: "
                << static_cast<int>(version)
                << "\n";

            continue;
        }


        // ====================================================
        // VALIDATE HEADER LENGTH
        // ====================================================

        if (
            headerLength !=
            urov::HEADER_SIZE
        )
        {
            std::cout
                << "Invalid header length: "
                << headerLength
                << "\n";

            continue;
        }


        // ====================================================
        // VALIDATE PAYLOAD LENGTH
        // ====================================================

        if (
            static_cast<std::size_t>(
                headerLength
            )
            +
            static_cast<std::size_t>(
                payloadLength
            )
            !=
            static_cast<std::size_t>(
                bytesReceived
            )
        )
        {
            std::cout
                << "Invalid payload length\n";

            continue;
        }


        // ====================================================
        // PRINT PACKET INFORMATION
        // ====================================================

        if (sequence < 10)
        {
            std::cout
                << "\n";
            std::cout
                << "-------------------------------\n";

            std::cout
                << "U-ROV PACKET\n";

            std::cout
                << "Bytes received : "
                << bytesReceived
                << "\n";

            std::cout
                << "Message type   : "
                << urov::messageTypeToString(
                    static_cast<
                        urov::MessageType
                    >(messageType)
                )
                << "\n";

            std::cout
                << "Header length  : "
                << headerLength
                << " bytes\n";

            std::cout
                << "Payload length : "
                << payloadLength
                << " bytes\n";

            std::cout
                << "Sequence       : "
                << sequence
                << "\n";

            std::cout
                << "Flags          : 0x"
                << std::hex
                << flags
                << std::dec
                << "\n";
        }


        // ====================================================
        // ECHO PACKET
        // ====================================================

        ssize_t bytesSent =
            sendto(
                sockfd,
                buffer,
                bytesReceived,
                0,
                reinterpret_cast<sockaddr*>(
                    &surfaceAddress
                ),
                surfaceLength
            );


        if (bytesSent < 0)
        {
            perror("sendto failed");
        }
    }


    close(sockfd);

    return 0;
}