#pragma once

// Minimal zero-dependency test runner for the ZarlinoSource test target.

#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace zstest
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

inline int& currentFailures()
{
    static int c = 0;
    return c;
}

struct Registrar
{
    Registrar (const char* name, std::function<void()> fn)
    {
        registry().push_back ({ name, std::move (fn) });
    }
};

inline int runAll()
{
    int passed = 0;
    int failed = 0;

    for (auto& t : registry())
    {
        currentFailures() = 0;
        std::cout << "[ RUN  ] " << t.name << std::endl;
        t.fn();

        if (currentFailures() == 0)
        {
            ++passed;
            std::cout << "[  OK  ] " << t.name << std::endl;
        }
        else
        {
            ++failed;
            std::cout << "[ FAIL ] " << t.name << " (" << currentFailures() << " failure(s))" << std::endl;
        }
    }

    std::cout << std::endl << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}

} // namespace zstest

#define ZS_TEST(name)                                                  \
    static void name();                                                \
    static ::zstest::Registrar zs_registrar_##name (#name, name);      \
    static void name()

#define ZS_EXPECT(cond)                                                \
    do {                                                               \
        if (! (cond)) {                                                \
            std::cout << "  FAIL: " #cond " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
            ++::zstest::currentFailures();                             \
        }                                                              \
    } while (0)

#define ZS_EXPECT_NEAR(a, b, tol)                                      \
    do {                                                               \
        const double zs_a = (double) (a);                              \
        const double zs_b = (double) (b);                              \
        const double zs_err = std::abs (zs_a - zs_b);                  \
        if (zs_err > (double) (tol)) {                                 \
            std::cout << "  FAIL: |" #a " - " #b "| = " << zs_err      \
                      << " > " << (double) (tol) << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
            ++::zstest::currentFailures();                             \
        }                                                              \
    } while (0)
