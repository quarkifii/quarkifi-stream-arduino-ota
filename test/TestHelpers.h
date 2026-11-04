// Shared helpers: callback recorder and platform selection.
#pragma once

#include <utility>
#include <vector>

#include "Arduino.h"
#include "MockState.h"
#include "framework/TestFramework.h"
#include "interfaces/OtaStatus.h"

#if defined(ESP32)
#include "esp32/ESP32OtaPlatform.h"
typedef ESP32OtaPlatform Platform;
#elif defined(ESP8266)
#include "esp8266/ESP8266OtaPlatform.h"
// Same as the real platform, but the system clock comes from mock::state.epoch.
class Platform : public ESP8266OtaPlatform {
protected:
    uint32_t systemEpoch() override { return mock::state.epoch; }
};
#endif

namespace th {

struct Event {
    int status;
    int progress;
    bool operator==(const Event &o) const { return status == o.status && progress == o.progress; }
};

inline std::vector<Event> &events() {
    static std::vector<Event> e;
    return e;
}

inline void record(const byte status, const byte progress) {
    events().push_back({status, progress});
}

// Status codes only (drops progress), in order.
inline std::vector<int> statuses() {
    std::vector<int> s;
    for (const Event &e : events()) s.push_back(e.status);
    return s;
}

// Status codes, excluding DOWNLOADING progress ticks.
inline std::vector<int> lifecycle() {
    std::vector<int> s;
    for (const Event &e : events())
        if (e.status != OtaStatus::DOWNLOADING) s.push_back(e.status);
    return s;
}

inline int count(int status) {
    int n = 0;
    for (const Event &e : events()) n += e.status == status;
    return n;
}

inline std::vector<int> progressValues() {
    std::vector<int> p;
    for (const Event &e : events())
        if (e.status == OtaStatus::DOWNLOADING) p.push_back(e.progress);
    return p;
}

inline void clear() { events().clear(); }

}  // namespace th

namespace tf {
template <>
struct Printer<th::Event> {
    static void print(std::ostream &os, const th::Event &e) {
        os << "(" << e.status << "," << e.progress << ")";
    }
};
}  // namespace tf
