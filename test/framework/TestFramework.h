// Minimal dependency-free test framework.
//
//   TEST(name) { EXPECT_EQ(a, b); EXPECT_TRUE(x); ASSERT_TRUE(x); }
//
//   KNOWN_ISSUE("description", correctBehaviourCondition);
//     Documents a bug that is still present in the library without failing the
//     run. When the condition becomes true (bug fixed) the runner reports it so
//     the check can be promoted to a real EXPECT.
//
// mock::reset() runs before every test.
#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace tf {

struct TestCase {
    const char *name;
    std::function<void()> fn;
};

struct Registry {
    std::vector<TestCase> tests;
    int failures = 0;          // failed expectations in the current test
    int knownOpen = 0;
    int knownFixed = 0;
    const char *current = "";
    static Registry &get() {
        static Registry r;
        return r;
    }
};

struct Registrar {
    Registrar(const char *name, std::function<void()> fn) {
        Registry::get().tests.push_back({name, fn});
    }
};

// Value printer; specialise tf::Printer<T> for custom types.
template <typename T, typename Enable = void>
struct Printer {
    static void print(std::ostream &os, const T &v) { os << v; }
};
template <typename T>
struct Printer<T, typename std::enable_if<std::is_arithmetic<T>::value>::type> {
    static void print(std::ostream &os, const T &v) { os << +v; }
};
template <typename T>
struct Printer<std::vector<T>> {
    static void print(std::ostream &os, const std::vector<T> &v) {
        os << "{";
        for (size_t i = 0; i < v.size(); i++) {
            if (i) os << ", ";
            Printer<T>::print(os, v[i]);
        }
        os << "}";
    }
};

template <typename A, typename B>
inline void expectEq(const A &a, const B &b, const char *ea, const char *eb,
                     const char *file, int line) {
    if (!(a == b)) {
        std::ostringstream os;
        os << "    " << file << ":" << line << ": EXPECT_EQ(" << ea << ", " << eb
           << ")\n      actual:   ";
        Printer<A>::print(os, a);
        os << "\n      expected: ";
        Printer<B>::print(os, b);
        std::cout << os.str() << "\n";
        Registry::get().failures++;
    }
}

inline bool expectTrue(bool v, const char *expr, const char *file, int line) {
    if (!v) {
        std::cout << "    " << file << ":" << line << ": EXPECT_TRUE(" << expr << ") failed\n";
        Registry::get().failures++;
    }
    return v;
}

inline void knownIssue(const char *desc, bool fixed, const char *file, int line) {
    Registry &r = Registry::get();
    if (fixed) {
        r.knownFixed++;
        std::cout << "    [KNOWN ISSUE NOW PASSES — promote to EXPECT] " << desc
                  << " (" << file << ":" << line << ")\n";
    } else {
        r.knownOpen++;
        std::cout << "    [known issue] " << desc << "\n";
    }
}

int runAll();

}  // namespace tf

#define TF_CAT2(a, b) a##b
#define TF_CAT(a, b) TF_CAT2(a, b)

#define TEST(name)                                                         \
    static void name();                                                    \
    static tf::Registrar TF_CAT(reg_, name)(#name, name);                  \
    static void name()

#define EXPECT_EQ(a, b) tf::expectEq((a), (b), #a, #b, __FILE__, __LINE__)
#define EXPECT_TRUE(x) tf::expectTrue(!!(x), #x, __FILE__, __LINE__)
#define EXPECT_FALSE(x) tf::expectTrue(!(x), "!(" #x ")", __FILE__, __LINE__)
#define ASSERT_TRUE(x) \
    do { if (!tf::expectTrue(!!(x), #x, __FILE__, __LINE__)) return; } while (0)
#define KNOWN_ISSUE(desc, fixedCond) tf::knownIssue(desc, !!(fixedCond), __FILE__, __LINE__)
