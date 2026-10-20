// SPDX-License-Identifier: GPL-3.0-or-later
#include "upwgen/options.hpp"
#include "upwgen/categories_generated.hpp"

#include <iostream>
#include <memory>
#include <new>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#endif

int main(int argc, char* argv[]) {
    try {
#ifdef _WIN32
        if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
            throw std::runtime_error("cannot set standard output to binary UTF-8 mode");
        }
        DWORD console_mode = 0;
        if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &console_mode) != 0 &&
            SetConsoleOutputCP(CP_UTF8) == 0) {
            throw std::runtime_error("cannot set console output to UTF-8");
        }
#endif
        std::vector<std::string_view> arguments;
        for (int i = 1; i < argc; ++i) {
            arguments.emplace_back(argv[i]);
        }
        const auto options = upwgen::parse_options(arguments);
        switch (options.command) {
        case upwgen::Command::help:
            upwgen::print_help(std::cout);
            break;
        case upwgen::Command::show_version:
            std::cout << "upwgen " << upwgen::version << " (Unicode "
                      << upwgen::unicode_data::unicode_version << ")\n";
            break;
        case upwgen::Command::sets:
            upwgen::print_sets(std::cout);
            break;
        default: {
            const auto policy = upwgen::make_policy(
                options.profile, options.include, options.exclude, options.require,
                options.length, options.max_bytes);
            std::unique_ptr<upwgen::RandomSource> random;
            if (options.command == upwgen::Command::corpus) {
                random = std::make_unique<upwgen::CorpusRandom>(options.seed);
            } else {
                random = std::make_unique<upwgen::SystemRandom>();
            }
            std::size_t draws_remaining = 4000000;
            for (std::size_t i = 0; i < options.count; ++i) {
                const auto value = upwgen::generate(policy, *random, &draws_remaining);
                upwgen::write_result(std::cout, options, policy, value, i + 1);
            }
            break;
        }
        }
        std::cout.flush();
        if (!std::cout) {
            throw std::runtime_error("cannot write output");
        }
        return 0;
    } catch (const std::bad_alloc&) {
        std::cerr << "upwgen: not enough memory\n";
    } catch (const std::exception& error) {
        std::cerr << "upwgen: " << error.what() << '\n';
    }
    return 1;
}
