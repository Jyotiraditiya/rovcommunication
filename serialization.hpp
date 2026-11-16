#ifndef UROV_SERIALIZATION_HPP
#define UROV_SERIALIZATION_HPP

#include <cstdint>
#include <cstddef>

#include "message_payloads.hpp"

namespace urov {

// ============================================================
// WRITE FUNCTIONS
// ============================================================

inline void writeUint16(
    uint8_t* buffer,
    uint16_t value)
{
    buffer[0] =
        static_cast<uint8_t>((value >> 8) & 0xFF);

    buffer[1] =
        static_cast<uint8_t>(value & 0xFF);
}


inline void writeInt16(
    uint8_t* buffer,
    int16_t value)
{
    writeUint16(
        buffer,
        static_cast<uint16_t>(value)
    );
}


inline void writeUint32(
    uint8_t* buffer,
    uint32_t value)
{
    buffer[0] =
        static_cast<uint8_t>((value >> 24) & 0xFF);

    buffer[1] =
        static_cast<uint8_t>((value >> 16) & 0xFF);

    buffer[2] =
        static_cast<uint8_t>((value >> 8) & 0xFF);

    buffer[3] =
        static_cast<uint8_t>(value & 0xFF);
}


// ============================================================
// READ FUNCTIONS
// ============================================================

inline uint16_t readUint16(
    const uint8_t* buffer)
{
    return
        (static_cast<uint16_t>(buffer[0]) << 8) |
        static_cast<uint16_t>(buffer[1]);
}


inline int16_t readInt16(
    const uint8_t* buffer)
{
    return static_cast<int16_t>(
        readUint16(buffer)
    );
}


inline uint32_t readUint32(
    const uint8_t* buffer)
{
    return
        (static_cast<uint32_t>(buffer[0]) << 24) |
        (static_cast<uint32_t>(buffer[1]) << 16) |
        (static_cast<uint32_t>(buffer[2]) << 8) |
        static_cast<uint32_t>(buffer[3]);
}


// ============================================================
// CONTROL SERIALIZATION
// ============================================================

inline void serializeControl(
    const ControlPayload& payload,
    uint8_t* buffer)
{
    writeInt16(
        buffer + 0,
        payload.forward
    );

    writeInt16(
        buffer + 2,
        payload.lateral
    );

    writeInt16(
        buffer + 4,
        payload.vertical
    );

    writeInt16(
        buffer + 6,
        payload.yaw
    );

    buffer[8] =
        payload.mode;

    buffer[9] =
        payload.emergency_stop;

    writeUint16(
        buffer + 10,
        payload.reserved
    );

    writeUint32(
        buffer + 12,
        payload.command_id
    );
}


// ============================================================
// CONTROL DESERIALIZATION
// ============================================================

inline ControlPayload deserializeControl(
    const uint8_t* buffer)
{
    ControlPayload payload{};

    payload.forward =
        readInt16(buffer + 0);

    payload.lateral =
        readInt16(buffer + 2);

    payload.vertical =
        readInt16(buffer + 4);

    payload.yaw =
        readInt16(buffer + 6);

    payload.mode =
        buffer[8];

    payload.emergency_stop =
        buffer[9];

    payload.reserved =
        readUint16(buffer + 10);

    payload.command_id =
        readUint32(buffer + 12);

    return payload;
}


// ============================================================
// TELEMETRY SERIALIZATION
// ============================================================

inline void serializeTelemetry(
    const TelemetryPayload& payload,
    uint8_t* buffer)
{
    writeUint32(
        buffer + 0,
        static_cast<uint32_t>(
            payload.depth_mm
        )
    );

    writeInt16(
        buffer + 4,
        payload.roll_cdeg
    );

    writeInt16(
        buffer + 6,
        payload.pitch_cdeg
    );

    writeInt16(
        buffer + 8,
        payload.yaw_cdeg
    );

    writeUint16(
        buffer + 10,
        payload.battery_mV
    );

    writeInt16(
        buffer + 12,
        payload.temperature_cdeg
    );

    buffer[14] =
        payload.system_status;

    buffer[15] =
        payload.reserved;
}


// ============================================================
// TELEMETRY DESERIALIZATION
// ============================================================

inline TelemetryPayload deserializeTelemetry(
    const uint8_t* buffer)
{
    TelemetryPayload payload{};

    payload.depth_mm =
        static_cast<int32_t>(
            readUint32(buffer + 0)
        );

    payload.roll_cdeg =
        readInt16(buffer + 4);

    payload.pitch_cdeg =
        readInt16(buffer + 6);

    payload.yaw_cdeg =
        readInt16(buffer + 8);

    payload.battery_mV =
        readUint16(buffer + 10);

    payload.temperature_cdeg =
        readInt16(buffer + 12);

    payload.system_status =
        buffer[14];

    payload.reserved =
        buffer[15];

    return payload;
}


// ============================================================
// ACK SERIALIZATION
// ============================================================
//
// ACK payload:
//
// 0-3    acknowledged_sequence
// 4      acknowledged_message_type
// 5      status
// 6-7    reserved
// ============================================================

inline void serializeAck(
    const AckPayload& payload,
    uint8_t* buffer)
{
    writeUint32(
        buffer + 0,
        payload.acknowledged_sequence
    );

    buffer[4] =
        payload.acknowledged_message_type;

    buffer[5] =
        payload.status;

    writeUint16(
        buffer + 6,
        payload.reserved
    );
}


// ============================================================
// ACK DESERIALIZATION
// ============================================================

inline AckPayload deserializeAck(
    const uint8_t* buffer)
{
    AckPayload payload{};

    payload.acknowledged_sequence =
        readUint32(buffer + 0);

    payload.acknowledged_message_type =
        buffer[4];

    payload.status =
        buffer[5];

    payload.reserved =
        readUint16(buffer + 6);

    return payload;
}

} // namespace urov

#endif