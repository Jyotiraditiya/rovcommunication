#ifndef UROV_PACKET_HPP
#define UROV_PACKET_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>

#include "protocol.hpp"
#include "message_payloads.hpp"
#include "serialization.hpp"

namespace urov {

// ============================================================
// PACKET SIZE CONSTANTS
// ============================================================

constexpr std::size_t CONTROL_PACKET_SIZE =
    HEADER_SIZE + sizeof(ControlPayload);

constexpr std::size_t TELEMETRY_PACKET_SIZE =
    HEADER_SIZE + sizeof(TelemetryPayload);

constexpr std::size_t ACK_PACKET_SIZE =
    HEADER_SIZE + sizeof(AckPayload);


// ============================================================
// HEADER SERIALIZATION
// ============================================================

inline void serializeHeader(
    const PacketHeader& header,
    uint8_t* buffer)
{
    // Magic
    buffer[0] =
        static_cast<uint8_t>(
            (header.magic >> 8) & 0xFF
        );

    buffer[1] =
        static_cast<uint8_t>(
            header.magic & 0xFF
        );

    // Version
    buffer[2] =
        header.version;

    // Message type
    buffer[3] =
        header.message_type;

    // Header length
    buffer[4] =
        static_cast<uint8_t>(
            (header.header_length >> 8) & 0xFF
        );

    buffer[5] =
        static_cast<uint8_t>(
            header.header_length & 0xFF
        );

    // Payload length
    buffer[6] =
        static_cast<uint8_t>(
            (header.payload_length >> 8) & 0xFF
        );

    buffer[7] =
        static_cast<uint8_t>(
            header.payload_length & 0xFF
        );

    // Flags
    buffer[8] =
        static_cast<uint8_t>(
            (header.flags >> 8) & 0xFF
        );

    buffer[9] =
        static_cast<uint8_t>(
            header.flags & 0xFF
        );

    // Sequence number
    buffer[10] =
        static_cast<uint8_t>(
            (header.sequence_number >> 24) & 0xFF
        );

    buffer[11] =
        static_cast<uint8_t>(
            (header.sequence_number >> 16) & 0xFF
        );

    buffer[12] =
        static_cast<uint8_t>(
            (header.sequence_number >> 8) & 0xFF
        );

    buffer[13] =
        static_cast<uint8_t>(
            header.sequence_number & 0xFF
        );

    // Timestamp
    for (int i = 0; i < 8; ++i)
    {
        buffer[14 + i] =
            static_cast<uint8_t>(
                (header.timestamp >>
                 (56 - i * 8)) & 0xFF
            );
    }

    // Checksum
    buffer[22] =
        static_cast<uint8_t>(
            (header.checksum >> 24) & 0xFF
        );

    buffer[23] =
        static_cast<uint8_t>(
            (header.checksum >> 16) & 0xFF
        );

    buffer[24] =
        static_cast<uint8_t>(
            (header.checksum >> 8) & 0xFF
        );

    buffer[25] =
        static_cast<uint8_t>(
            header.checksum & 0xFF
        );

    // Reserved
    for (int i = 26; i < 32; ++i)
    {
        buffer[i] = 0;
    }
}


// ============================================================
// HEADER DESERIALIZATION
// ============================================================

inline PacketHeader deserializeHeader(
    const uint8_t* buffer)
{
    PacketHeader header{};

    // Magic
    header.magic =
        (static_cast<uint16_t>(buffer[0]) << 8) |
        static_cast<uint16_t>(buffer[1]);

    // Version
    header.version =
        buffer[2];

    // Message type
    header.message_type =
        buffer[3];

    // Header length
    header.header_length =
        (static_cast<uint16_t>(buffer[4]) << 8) |
        static_cast<uint16_t>(buffer[5]);

    // Payload length
    header.payload_length =
        (static_cast<uint16_t>(buffer[6]) << 8) |
        static_cast<uint16_t>(buffer[7]);

    // Flags
    header.flags =
        (static_cast<uint16_t>(buffer[8]) << 8) |
        static_cast<uint16_t>(buffer[9]);

    // Sequence
    header.sequence_number =
        (static_cast<uint32_t>(buffer[10]) << 24) |
        (static_cast<uint32_t>(buffer[11]) << 16) |
        (static_cast<uint32_t>(buffer[12]) << 8) |
        static_cast<uint32_t>(buffer[13]);

    // Timestamp
    header.timestamp = 0;

    for (int i = 0; i < 8; ++i)
    {
        header.timestamp =
            (header.timestamp << 8) |
            static_cast<uint64_t>(
                buffer[14 + i]
            );
    }

    // Checksum
    header.checksum =
        (static_cast<uint32_t>(buffer[22]) << 24) |
        (static_cast<uint32_t>(buffer[23]) << 16) |
        (static_cast<uint32_t>(buffer[24]) << 8) |
        static_cast<uint32_t>(buffer[25]);

    return header;
}


// ============================================================
// HEADER VALIDATION
// ============================================================

inline bool validateHeader(
    const PacketHeader& header,
    std::size_t receivedSize)
{
    if (header.magic != MAGIC)
        return false;

    if (header.version != VERSION)
        return false;

    if (header.header_length != HEADER_SIZE)
        return false;

    if (header.payload_length +
        header.header_length != receivedSize)
    {
        return false;
    }

    if (header.payload_length > receivedSize)
        return false;

    return true;
}


// ============================================================
// BUILD CONTROL PACKET
// ============================================================

inline bool buildControlPacket(
    const ControlPayload& payload,
    uint32_t sequenceNumber,
    uint64_t timestamp,
    uint16_t flags,
    uint8_t* buffer)
{
    PacketHeader header{};

    header.magic =
        MAGIC;

    header.version =
        VERSION;

    header.message_type =
        static_cast<uint8_t>(
            MessageType::CONTROL
        );

    header.header_length =
        HEADER_SIZE;

    header.payload_length =
        sizeof(ControlPayload);

    header.flags =
        flags;

    header.sequence_number =
        sequenceNumber;

    header.timestamp =
        timestamp;

    // Checksum not implemented yet
    header.checksum =
        0;

    serializeHeader(
        header,
        buffer
    );

    serializeControl(
        payload,
        buffer + HEADER_SIZE
    );

    return true;
}


// ============================================================
// BUILD TELEMETRY PACKET
// ============================================================

inline bool buildTelemetryPacket(
    const TelemetryPayload& payload,
    uint32_t sequenceNumber,
    uint64_t timestamp,
    uint16_t flags,
    uint8_t* buffer)
{
    PacketHeader header{};

    header.magic =
        MAGIC;

    header.version =
        VERSION;

    header.message_type =
        static_cast<uint8_t>(
            MessageType::TELEMETRY
        );

    header.header_length =
        HEADER_SIZE;

    header.payload_length =
        sizeof(TelemetryPayload);

    header.flags =
        flags;

    header.sequence_number =
        sequenceNumber;

    header.timestamp =
        timestamp;

    // Checksum not implemented yet
    header.checksum =
        0;

    serializeHeader(
        header,
        buffer
    );

    serializeTelemetry(
        payload,
        buffer + HEADER_SIZE
    );

    return true;
}


// ============================================================
// BUILD ACK PACKET
// ============================================================

inline bool buildAckPacket(
    const AckPayload& payload,
    uint32_t sequenceNumber,
    uint64_t timestamp,
    uint16_t flags,
    uint8_t* buffer)
{
    PacketHeader header{};

    header.magic =
        MAGIC;

    header.version =
        VERSION;

    header.message_type =
        static_cast<uint8_t>(
            MessageType::ACK
        );

    header.header_length =
        HEADER_SIZE;

    header.payload_length =
        sizeof(AckPayload);

    header.flags =
        flags;

    header.sequence_number =
        sequenceNumber;

    header.timestamp =
        timestamp;

    // Checksum not implemented yet
    header.checksum =
        0;

    serializeHeader(
        header,
        buffer
    );

    serializeAck(
        payload,
        buffer + HEADER_SIZE
    );

    return true;
}


// ============================================================
// PARSE CONTROL PACKET
// ============================================================

inline bool parseControlPacket(
    const uint8_t* buffer,
    std::size_t receivedSize,
    PacketHeader& header,
    ControlPayload& payload)
{
    if (receivedSize < HEADER_SIZE)
        return false;

    header =
        deserializeHeader(buffer);

    if (!validateHeader(
            header,
            receivedSize))
    {
        return false;
    }

    if (header.message_type !=
        static_cast<uint8_t>(
            MessageType::CONTROL
        ))
    {
        return false;
    }

    if (header.payload_length !=
        sizeof(ControlPayload))
    {
        return false;
    }

    payload =
        deserializeControl(
            buffer + HEADER_SIZE
        );

    return true;
}


// ============================================================
// PARSE TELEMETRY PACKET
// ============================================================

inline bool parseTelemetryPacket(
    const uint8_t* buffer,
    std::size_t receivedSize,
    PacketHeader& header,
    TelemetryPayload& payload)
{
    if (receivedSize < HEADER_SIZE)
        return false;

    header =
        deserializeHeader(buffer);

    if (!validateHeader(
            header,
            receivedSize))
    {
        return false;
    }

    if (header.message_type !=
        static_cast<uint8_t>(
            MessageType::TELEMETRY
        ))
    {
        return false;
    }

    if (header.payload_length !=
        sizeof(TelemetryPayload))
    {
        return false;
    }

    payload =
        deserializeTelemetry(
            buffer + HEADER_SIZE
        );

    return true;
}


// ============================================================
// PARSE ACK PACKET
// ============================================================

inline bool parseAckPacket(
    const uint8_t* buffer,
    std::size_t receivedSize,
    PacketHeader& header,
    AckPayload& payload)
{
    if (receivedSize < HEADER_SIZE)
        return false;

    header =
        deserializeHeader(buffer);

    if (!validateHeader(
            header,
            receivedSize))
    {
        return false;
    }

    if (header.message_type !=
        static_cast<uint8_t>(
            MessageType::ACK
        ))
    {
        return false;
    }

    if (header.payload_length !=
        sizeof(AckPayload))
    {
        return false;
    }

    payload =
        deserializeAck(
            buffer + HEADER_SIZE
        );

    return true;
}

} // namespace urov

#endif