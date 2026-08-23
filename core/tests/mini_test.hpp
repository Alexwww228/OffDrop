// mini_test.hpp
// Нарочно без gtest/catch2 — для скелета портфолио-проекта лишняя внешняя
// зависимость только добавляет трения при сборке. Если проект вырастет,
// имеет смысл перейти на GoogleTest (CMake FetchContent), но для core-логики
// хватает и этого.

#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace mini_test {

struct Test {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<Test>& Registry() {
    static std::vector<Test> tests;
    return tests;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) {
        Registry().push_back({name, std::move(fn)});
    }
};

struct AssertionFailure {
    std::string message;
};

inline int RunAll() {
    int failed = 0;
    for (auto& t : Registry()) {
        try {
            t.fn();
            std::cout << "[ OK ] " << t.name << "\n";
        } catch (const AssertionFailure& e) {
            std::cout << "[FAIL] " << t.name << ": " << e.message << "\n";
            ++failed;
        } catch (const std::exception& e) {
            std::cout << "[FAIL] " << t.name << ": unexpected exception: " << e.what() << "\n";
            ++failed;
        }
    }
    std::cout << "\n" << (Registry().size() - failed) << "/" << Registry().size() << " passed\n";
    return failed;
}

}  // namespace mini_test

#define TEST(name)                                                                   \
    void name();                                                                     \
    static mini_test::Registrar registrar_##name(#name, name);                       \
    void name()

#define ASSERT_TRUE(cond)                                                            \
    if (!(cond)) throw mini_test::AssertionFailure{"ASSERT_TRUE failed: " #cond};

#define ASSERT_EQ(a, b)                                                              \
    if (!((a) == (b))) throw mini_test::AssertionFailure{"ASSERT_EQ failed: " #a " != " #b};

#define ASSERT_THROWS(expr)                                                          \
    {                                                                                \
        bool threw = false;                                                         \
        try {                                                                        \
            expr;                                                                    \
        } catch (...) {                                                              \
            threw = true;                                                            \
        }                                                                            \
        if (!threw) throw mini_test::AssertionFailure{"expected exception: " #expr}; \
    }
