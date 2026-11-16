#include <iostream>
#include <cstdint>
#include <chrono>

#include "packet.hpp"

int main()
{
    std::cout << "============================================\n";
    std::cout << "          U-ROV ACK PACKET TEST\n";
    std::cout << "============================================\n\n";


    // ============================================================
    // Create ACK payload
    // ============================================================

    urov::AckPayload ack{};

    ack.acknowledged_sequence = 1;

    ack.acknowledged_message_type =
        static_cast<uint8_t>(
            urov::MessageType::CONTROL
        );

    ack.status =
        static_cast<uint8_t>(
            urov::AckStatus::SUCCESS
        );

    ack.reserved = 0;


    // ============================================================
    // Timestamp
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
    // Build ACK packet
    // ============================================================

    uint8_t packet[
        urov::ACK_PACKET_SIZE
    ];

    bool buildResult =
        urov::buildAckPacket(
            ack,
            1,
            timestamp,
            urov::FLAG_NONE,
            packet
        );

    if (!buildResult)
    {
        std::cout
            << "ERROR: ACK packet construction failed\n";

        return 1;
    }


    // ============================================================
    // Display constructed packet
    // ============================================================

    std::cout
        << "ACK packet constructed successfully\n";

    std::cout
        << "Packet size                : "
        << sizeof(packet)
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
        << "Status                     : "
        << static_cast<int>(
            ack.status
        )
        << "\n\n";


    // ============================================================
    // Parse ACK packet
    // ============================================================

    urov::PacketHeader parsedHeader{};
    urov::AckPayload parsedAck{};

    bool parseResult =
        urov::parseAckPacket(
            packet,
            sizeof(packet),
            parsedHeader,
            parsedAck
        );

    if (!parseResult)
    {
        std::cout
            << "ERROR: ACK packet parsing failed\n";

        return 1;
    }


    // ============================================================
    // Display parsed packet
    // ============================================================

    std::cout
        << "ACK packet parsed successfully\n";

    std::cout
        << "Message type               : "
        << static_cast<int>(
            parsedHeader.message_type
        )
        << "\n";

    std::cout
        << "Sequence                   : "
        << parsedHeader.sequence_number
        << "\n";

    std::cout
        << "Payload size               : "
        << parsedHeader.payload_length
        << " bytes\n";

    std::cout
        << "Acknowledged sequence      : "
        << parsedAck.acknowledged_sequence
        << "\n";

    std::cout
        << "Acknowledged message type : "
        << static_cast<int>(
            parsedAck.acknowledged_message_type
        )
        << "\n";

    std::cout
        << "Status                     : "
        << static_cast<int>(
            parsedAck.status
        )
        << "\n";


    // ============================================================
    // Verify values
    // ============================================================

    bool passed = true;

    if (parsedHeader.message_type !=
        static_cast<uint8_t>(
            urov::MessageType::ACK))
    {
        passed = false;
    }

    if (parsedHeader.sequence_number != 1)
    {
        passed = false;
    }

    if (parsedHeader.payload_length !=
        sizeof(urov::AckPayload))
    {
        passed = false;
    }

    if (parsedAck.acknowledged_sequence != 1)
    {
        passed = false;
    }

    if (parsedAck.acknowledged_message_type !=
        static_cast<uint8_t>(
            urov::MessageType::CONTROL))
    {
        passed = false;
    }

    if (parsedAck.status !=
        static_cast<uint8_t>(
            urov::AckStatus::SUCCESS))
    {
        passed = false;
    }


    // ============================================================
    // Result
    // ============================================================

    std::cout
        << "\n============================================\n";

    if (passed)
    {
        std::cout
            << "ACK TEST : PASSED\n";
    }
    else
    {
        std::cout
            << "ACK TEST : FAILED\n";
    }

    std::cout
        << "============================================\n";


    return passed ? 0 : 1;
}