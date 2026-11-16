#ifndef UROV_MESSAGE_PAYLOADS_HPP
#define UROV_MESSAGE_PAYLOADS_HPP

#include <cstdint>

namespace urov {

// ============================================================
// CONTROL PAYLOAD
// ============================================================
//
// Sent from SURFACE -> ROV
//
// Size = 16 bytes
// ============================================================

struct ControlPayload
{
    int16_t forward;
    int16_t lateral;
    int16_t vertical;
    int16_t yaw;

    uint8_t mode;
    uint8_t emergency_stop;

    uint16_t reserved;

    uint32_t command_id;
};


// ============================================================
// TELEMETRY PAYLOAD
// ============================================================
//
// Sent from ROV -> SURFACE
//
// Size = 16 bytes
// ============================================================

struct TelemetryPayload
{
    int32_t depth_mm;

    int16_t roll_cdeg;
    int16_t pitch_cdeg;
    int16_t yaw_cdeg;

    uint16_t battery_mV;

    int16_t temperature_cdeg;

    uint8_t system_status;
    uint8_t reserved;
};


// ============================================================
// ACK PAYLOAD
// ============================================================
//
// Sent by receiver to acknowledge a packet.
//
// Size = 8 bytes
//
// Byte layout:
//
// 0-3    Acknowledged sequence number
// 4      Acknowledged message type
// 5      ACK status
// 6-7    Reserved
// ============================================================

struct AckPayload
{
    uint32_t acknowledged_sequence;

    uint8_t acknowledged_message_type;

    uint8_t status;

    uint16_t reserved;
};


// ============================================================
// STATIC SIZE CHECKS
// ============================================================

static_assert(
    sizeof(ControlPayload) == 16,
    "ControlPayload must be exactly 16 bytes"
);

static_assert(
    sizeof(TelemetryPayload) == 16,
    "TelemetryPayload must be exactly 16 bytes"
);

static_assert(
    sizeof(AckPayload) == 8,
    "AckPayload must be exactly 8 bytes"
);

} // namespace urov

#endif