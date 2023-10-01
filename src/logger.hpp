#pragma once
#include <fmt/format.h>
#include <fmt/color.h>

#define logD(...) fmt::print("[DEBUG] {}\n", fmt::format(__VA_ARGS__))
#define logW(...) fmt::print(fg(fmt::terminal_color::yellow), "[WARN] {}\n", fmt::format(__VA_ARGS__))
#define logE(...) fmt::print(fmt::emphasis::bold | fg(fmt::terminal_color::red), "[ERROR] {}\n", fmt::format(__VA_ARGS__))
#define assert(...)                                                                                                              \
    if (!(__VA_ARGS__)) {                                                                                                        \
        logE("Assert failed (line {} of {}): \"{}\"", __LINE__, __FILE__, #__VA_ARGS__);                                         \
        exit(1);                                                                                                                 \
    }