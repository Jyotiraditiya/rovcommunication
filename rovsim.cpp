// UROV prototype - simulated vehicle (C++17, POSIX sockets, MAVLink 2 C headers).
//
// Runs on the Pi (or any Linux/macOS box). Streams DUMMY sensor data as MAVLink over UDP
// and reacts to GCS heartbeats, joystick input and arm/disarm commands.
//
//   rov_sim  --UDP-->  GCS laptop :14550
//   rov_sim  <--UDP--  GCS laptop  (heartbeat, MANUAL_CONTROL, COMMAND_LONG)
//
// Usage: ./rov_sim --gcs-ip 192.168.2.1 [--gcs-port 14550] [--sysid 1] [--duration 0]

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

#include <mavlink/common/mavlink.h>

using Clock = std::chrono::steady_clock;
constexpr double kPi = 3.14159265358979323846;
static volatile std::sig_atomic_t g_stop = 0;
static void on_sigint(int) { g_stop = 1; }

// MAVLink pack functions copy a FIXED-size char array (statustext 50, named_value 10,
// param_id 16). Pad strings into a zeroed buffer of that size so we never read past a literal.
template <size_t N>
struct FixedStr {
    char data[N];
    explicit FixedStr(const char* s) {
        std::memset(data, 0, N);
        std::strncpy(data, s, N);
    }
};

static double seconds_since(Clock::time_point t0) {
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

// ---------------------------------------------------------------- dummy sensors
struct Sensors {
    double depth, water_temp, roll, pitch, yaw, vn, ve, vd, x, y;
    double volts, amps, hull_hum, hull_tmp;
    int heading, remaining;
};

class DummySensors {
public:
    DummySensors() : t0_(Clock::now()), last_(t0_), rng_(42) {}

    double thr = 0.0;  // |joystick| 0..100, makes fake current react to commands

    Sensors step(bool armed) {
        auto now = Clock::now();
        double t = std::chrono::duration<double>(now - t0_).count();
        double dt = std::chrono::duration<double>(now - last_).count();
        last_ = now;
        std::normal_distribution<double> n01(0.0, 1.0);

        Sensors s{};
        s.depth = 2.5 + 2.5 * std::sin(t / 20.0);                  // m      (MS5837)
        s.water_temp = 22.0 - 0.3 * s.depth + 0.02 * n01(rng_);    // degC
        s.roll = 3.0 * kPi / 180.0 * std::sin(t / 3.0);
        s.pitch = 2.0 * kPi / 180.0 * std::sin(t / 4.0 + 1.0);
        s.yaw = std::fmod(t * 0.1 + kPi, 2 * kPi) - kPi;
        double vx = 0.30 + 0.05 * std::sin(t);                     // m/s    (DVL-A50)
        s.vn = vx * std::cos(s.yaw);
        s.ve = vx * std::sin(s.yaw);
        s.vd = 0.125 * std::cos(t / 20.0);                         // + = descending
        x_ += s.vn * dt;
        y_ += s.ve * dt;
        s.x = x_;
        s.y = y_;
        s.volts = std::max(13.0, 16.4 - 0.002 * t) + 0.01 * n01(rng_);          // (INA219)
        s.amps = (armed ? 1.0 + thr / 100.0 * 12.0 : 0.8) + 0.05 * n01(rng_);
        s.remaining = (int)std::clamp((s.volts - 13.0) / (16.8 - 13.0) * 100.0, 0.0, 100.0);
        s.hull_hum = 35.0 + 0.5 * std::sin(t / 10.0);              // %RH    (SHT31-D)
        s.hull_tmp = 28.0 + 0.2 * std::sin(t / 15.0);              // degC
        int h = (int)(s.yaw * 180.0 / kPi);
        s.heading = ((h % 360) + 360) % 360;
        return s;
    }

private:
    Clock::time_point t0_, last_;
    std::mt19937 rng_;
    double x_ = 0.0, y_ = 0.0;
};

// ---------------------------------------------------------------- UDP helpers
class UdpLink {
public:
    UdpLink(const std::string& ip, int port) {
        fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd_ < 0) { std::perror("socket"); std::exit(1); }
        ::fcntl(fd_, F_SETFL, ::fcntl(fd_, F_GETFL, 0) | O_NONBLOCK);
        std::memset(&peer_, 0, sizeof(peer_));
        peer_.sin_family = AF_INET;
        peer_.sin_port = htons(port);
        if (::inet_pton(AF_INET, ip.c_str(), &peer_.sin_addr) != 1) {
            std::fprintf(stderr, "bad IP: %s\n", ip.c_str());
            std::exit(1);
        }
    }
    ~UdpLink() { if (fd_ >= 0) ::close(fd_); }

    void send(const mavlink_message_t& msg) {
        uint8_t buf[MAVLINK_MAX_PACKET_LEN];
        uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
        ::sendto(fd_, buf, len, 0, (const sockaddr*)&peer_, sizeof(peer_));
    }

    // returns bytes read (0 if nothing waiting)
    ssize_t recv(uint8_t* buf, size_t cap) { return ::recvfrom(fd_, buf, cap, 0, nullptr, nullptr); }

private:
    int fd_ = -1;
    sockaddr_in peer_{};
};

// ---------------------------------------------------------------- main
int main(int argc, char** argv) {
    std::string gcs_ip = "127.0.0.1";
    int gcs_port = 14550, sysid = 1;
    double duration = 0;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) { std::fprintf(stderr, "%s needs a value\n", name); std::exit(1); }
            return argv[++i];
        };
        if (a == "--gcs-ip") gcs_ip = next("--gcs-ip");
        else if (a == "--gcs-port") gcs_port = std::atoi(next("--gcs-port"));
        else if (a == "--sysid") sysid = std::atoi(next("--sysid"));
        else if (a == "--duration") duration = std::atof(next("--duration"));
        else { std::fprintf(stderr, "unknown arg %s\n", a.c_str()); return 1; }
    }
    std::signal(SIGINT, on_sigint);

    const uint8_t SYS = (uint8_t)sysid, COMP = 1;
    UdpLink link(gcs_ip, gcs_port);
    std::printf("[rov] sending MAVLink to %s:%d as sysid %d\n", gcs_ip.c_str(), gcs_port, sysid);

    DummySensors sensors;
    const auto start = Clock::now();
    bool armed = false, gcs_seen = false;
    int16_t cx = 0, cy = 0, cz = 0, cr = 0;
    Sensors s{};

    enum { HB, SYSST, ATT, VFR, PRESS, DVL, HULL, N_STREAMS };
    const double period[N_STREAMS] = {1.0, 1.0, 0.1, 0.2, 0.2, 0.1, 1.0};
    double next_t[N_STREAMS] = {0};

    const uint32_t sensor_bits = MAV_SYS_STATUS_SENSOR_3D_GYRO | MAV_SYS_STATUS_SENSOR_3D_ACCEL |
                                 MAV_SYS_STATUS_SENSOR_3D_MAG | MAV_SYS_STATUS_SENSOR_ABSOLUTE_PRESSURE;

    mavlink_message_t msg;
    mavlink_msg_statustext_pack(SYS, COMP, &msg, MAV_SEVERITY_INFO,
                                FixedStr<50>("UROV dummy-sensor sim online").data);
    link.send(msg);

    mavlink_status_t rx_status{};
    double last_print = 0;

    while (!g_stop) {
        double now = seconds_since(start);
        if (duration > 0 && now > duration) break;
        uint32_t ms = (uint32_t)(now * 1000.0);

        // ---- outgoing: dummy sensor streams ----
        bool any_due = false;
        for (int k = 0; k < N_STREAMS; ++k) any_due |= (now >= next_t[k]);
        if (any_due) s = sensors.step(armed);

        for (int k = 0; k < N_STREAMS; ++k) {
            if (now < next_t[k]) continue;
            next_t[k] = now + period[k];
            switch (k) {
            case HB: {
                uint8_t base = MAV_MODE_FLAG_CUSTOM_MODE_ENABLED;
                if (armed) base |= MAV_MODE_FLAG_SAFETY_ARMED;
                mavlink_msg_heartbeat_pack(SYS, COMP, &msg, MAV_TYPE_SUBMARINE,
                                           MAV_AUTOPILOT_ARDUPILOTMEGA, base, 0, MAV_STATE_ACTIVE);
                link.send(msg);
                break;
            }
            case SYSST:
                mavlink_msg_sys_status_pack(SYS, COMP, &msg, sensor_bits, sensor_bits, sensor_bits, 150,
                                            (uint16_t)(s.volts * 1000), (int16_t)(s.amps * 100),
                                            (int8_t)s.remaining, 0, 0, 0, 0, 0, 0);
                link.send(msg);
                break;
            case ATT:
                mavlink_msg_attitude_pack(SYS, COMP, &msg, ms, (float)s.roll, (float)s.pitch,
                                          (float)s.yaw, 0.f, 0.f, 0.f);
                link.send(msg);
                break;
            case VFR:
                mavlink_msg_vfr_hud_pack(SYS, COMP, &msg, 0.f, (float)std::hypot(s.vn, s.ve),
                                         (int16_t)s.heading, (uint16_t)sensors.thr,
                                         (float)-s.depth, (float)-s.vd);
                link.send(msg);
                break;
            case PRESS:
                mavlink_msg_scaled_pressure_pack(SYS, COMP, &msg, ms,
                                                 (float)(1013.25 + s.depth * 98.0665), 0.f,
                                                 (int16_t)(s.water_temp * 100));
                link.send(msg);
                break;
            case DVL:
                mavlink_msg_local_position_ned_pack(SYS, COMP, &msg, ms, (float)s.x, (float)s.y,
                                                    (float)s.depth, (float)s.vn, (float)s.ve, (float)s.vd);
                link.send(msg);
                break;
            case HULL:
                mavlink_msg_named_value_float_pack(SYS, COMP, &msg, ms, FixedStr<10>("HULL_HUM").data,
                                                   (float)s.hull_hum);
                link.send(msg);
                mavlink_msg_named_value_float_pack(SYS, COMP, &msg, ms, FixedStr<10>("HULL_TMP").data,
                                                   (float)s.hull_tmp);
                link.send(msg);
                break;
            }
        }

        // ---- incoming: whatever the GCS sends ----
        uint8_t buf[512];
        ssize_t n;
        while ((n = link.recv(buf, sizeof(buf))) > 0) {
            for (ssize_t i = 0; i < n; ++i) {
                mavlink_message_t in;
                if (!mavlink_parse_char(MAVLINK_COMM_0, buf[i], &in, &rx_status)) continue;

                switch (in.msgid) {
                case MAVLINK_MSG_ID_HEARTBEAT:
                    if (!gcs_seen) {
                        gcs_seen = true;
                        std::printf("[rov] GCS heartbeat received (sysid %d)\n", in.sysid);
                    }
                    break;
                case MAVLINK_MSG_ID_MANUAL_CONTROL: {
                    mavlink_manual_control_t mc;
                    mavlink_msg_manual_control_decode(&in, &mc);
                    cx = mc.x; cy = mc.y; cz = mc.z; cr = mc.r;
                    int m = std::max({std::abs((int)mc.x), std::abs((int)mc.y), std::abs((int)mc.r)});
                    sensors.thr = std::min(100.0, m / 10.0);
                    break;
                }
                case MAVLINK_MSG_ID_COMMAND_LONG: {
                    mavlink_command_long_t cmd;
                    mavlink_msg_command_long_decode(&in, &cmd);
                    if (cmd.target_system != 0 && cmd.target_system != SYS) break;
                    uint8_t result = MAV_RESULT_UNSUPPORTED;
                    if (cmd.command == MAV_CMD_COMPONENT_ARM_DISARM) {
                        armed = (cmd.param1 == 1.0f);
                        std::printf("[rov] %s\n", armed ? "ARMED" : "DISARMED");
                        result = MAV_RESULT_ACCEPTED;
                    }
                    mavlink_msg_command_ack_pack(SYS, COMP, &msg, cmd.command, result);
                    link.send(msg);
                    break;
                }
                case MAVLINK_MSG_ID_PARAM_REQUEST_LIST:
                    // minimal answer so QGC does not wait forever for parameters
                    mavlink_msg_param_value_pack(SYS, COMP, &msg, FixedStr<16>("SYSID_THISMAV").data,
                                                 (float)sysid, MAV_PARAM_TYPE_UINT8, 1, 0);
                    link.send(msg);
                    break;
                default: break;
                }
            }
        }

        if (now - last_print > 1.0) {
            last_print = now;
            std::printf("[rov] armed=%d depth=%.2fm batt=%.2fV ctrl(x,y,z,r)=(%d,%d,%d,%d)\n", armed,
                        s.depth, s.volts, cx, cy, cz, cr);
        }
        usleep(5000);
    }
    std::printf("[rov] done\n");
    return 0;
}
