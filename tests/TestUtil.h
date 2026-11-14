#pragma once

// A tiny test harness: TEST(name) { CHECK(...); } registers itself, main() runs everything.

#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace testing
{

struct TestCase
{
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> r;
    return r;
}

inline int& failures()
{
    static int f = 0;
    return f;
}

struct Registrar
{
    Registrar (const char* name, std::function<void()> fn) { registry().push_back ({ name, std::move (fn) }); }
};

} // namespace testing

#define TEST(name)                                                              \
    static void test_##name();                                                  \
    static testing::Registrar registrar_##name (#name, test_##name);            \
    static void test_##name()

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (! (cond))                                                           \
        {                                                                       \
            ++testing::failures();                                              \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond << "\n"; \
        }                                                                       \
    } while (0)

#define CHECK_EQ(a, b)                                                          \
    do {                                                                        \
        const auto va = (a);                                                    \
        const auto vb = (b);                                                    \
        if (! (va == vb))                                                       \
        {                                                                       \
            ++testing::failures();                                              \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << "  " #a " == " #b \
                      << "  (" << static_cast<long long> (va) << " vs " << static_cast<long long> (vb) << ")\n"; \
        }                                                                       \
    } while (0)

#define CHECK_NEAR(a, b, tol) CHECK(std::fabs (static_cast<double> (a) - static_cast<double> (b)) <= (tol))
