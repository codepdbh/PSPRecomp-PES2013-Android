#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
namespace psprecomp { class Runtime; }
namespace vcs {
struct AlarmInvocation { std::int32_t uid; std::uint32_t function, argument; };
class AlarmServices {
public:
    struct Record { std::uint64_t deadline; std::uint32_t function, argument; bool running{}; };
    std::function<std::uint64_t()> now;
    std::map<std::int32_t, Record> records;
    std::int32_t next_uid{0x900};
    std::optional<AlarmInvocation> take_due();
    void complete(std::int32_t uid, std::int32_t delay);
};
std::shared_ptr<AlarmServices> install_alarm_services(psprecomp::Runtime &, std::function<std::uint64_t()> now);
}
