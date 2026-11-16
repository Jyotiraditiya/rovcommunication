#ifndef UROV_PROTOCOL_HPP
#define UROV_PROTOCOL_HPP

#include <cstdint>
#include <cstddef>

namespace urov {

// ============================================================
// PROTOCOL CONSTANTS
// ============================================================

constexpr uint16_t MAGIC = 0x5552;       // "UR"
constexpr uint8_t  VERSION = 1;

constexpr std::size_t HEADER_SIZE = 32;


// ============================================================
// MESSAGE TYPES
// ============================================================

enum class MessageType : uint8_t
{
    CONTROL        = 0x01,
    TELEMETRY      = 0x02,
    HEARTBEAT      = 0x03,
    ACK            = 0x04,
    EMERGENCY_STOP = 0x05,
    STATUS         = 0x06,
    SENSOR_DATA    = 0x07
};


// ============================================================
// FLAGS
// ============================================================

enum PacketFlags : uint16_t
{
    FLAG_NONE     = 0x0000,
    FLAG_ACK_REQ  = 0x0001,
    FLAG_PRIORITY = 0x0002,
    FLAG_ERROR    = 0x0004
};


// ============================================================
// LOGICAL HEADER
//
// This structure describes the protocol.
// We DO NOT send this struct directly over UDP.
//
// Actual wire format = exactly 32 bytes.
// ============================================================

struct PacketHeader
{
    uint16_t magic;
    uint8_t  version;
    uint8_t  message_type;

    uint16_t header_length;
    uint16_t payload_length;

    uint16_t flags;

    uint32_t sequence_number;

    uint64_t timestamp;

    uint32_t checksum;
};


// ============================================================
// MESSAGE TYPE TO STRING
// ============================================================

inline const char* messageTypeToString(MessageType type)
{
    switch (type)
    {
        case MessageType::CONTROL:
            return "CONTROL";

        case MessageType::TELEMETRY:
            return "TELEMETRY";

        case MessageType::HEARTBEAT:
            return "HEARTBEAT";

        case MessageType::ACK:
            return "ACK";

        case MessageType::EMERGENCY_STOP:
            return "EMERGENCY_STOP";

        case MessageType::STATUS:
            return "STATUS";

        case MessageType::SENSOR_DATA:
            return "SENSOR_DATA";

        default:
            return "UNKNOWN";
    }
}

} // namespace urov

#endif