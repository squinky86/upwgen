// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "upwgen/core.hpp"
#include <iosfwd>

namespace upwgen {

enum class Command { generate, corpus, sets, help, show_version };
enum class Format { raw, numbered, escaped, jsonl };

struct Options {
    Command command = Command::generate;
    Format format = Format::raw;
    std::string profile = "portable";
    std::size_t length = 24;
    std::size_t count = 1;
    std::size_t max_bytes = 0;
    std::uint64_t seed = 0;
    std::vector<std::string> include;
    std::vector<std::string> exclude;
    std::vector<std::string> require;
};

Options parse_options(const std::vector<std::string_view>& arguments);
void print_help(std::ostream& output);
void print_sets(std::ostream& output);
void write_result(std::ostream& output, const Options& options, const Policy& policy,
                  const SecretBuffer<char32_t>& value, std::size_t index);

} // namespace upwgen
