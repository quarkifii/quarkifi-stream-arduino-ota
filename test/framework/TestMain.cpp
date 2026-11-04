#include "TestFramework.h"
#include "MockState.h"
#include "TestHelpers.h"

int tf::runAll() {
    Registry &r = Registry::get();
    int failedTests = 0;
    for (const TestCase &t : r.tests) {
        mock::reset();
        th::clear();
        r.failures = 0;
        r.current = t.name;
        t.fn();
        std::cout << (r.failures ? "[FAIL] " : "[ OK ] ") << t.name << "\n";
        if (r.failures) failedTests++;
    }
    std::cout << "\n" << PLATFORM_NAME << ": " << r.tests.size() << " tests, "
              << failedTests << " failed, " << r.knownOpen << " known issues open";
    if (r.knownFixed) std::cout << ", " << r.knownFixed << " known issues now fixed";
    std::cout << "\n";
    return failedTests ? 1 : 0;
}

int main() { return tf::runAll(); }
