// SPDX-License-Identifier: GPL-3.0-or-later
#include "upwgen/options.hpp"
#include "upwgen/categories_generated.hpp"

#include <charconv>
#include <ostream>
#include <set>
#include <system_error>

namespace upwgen {
namespace {

std::uint64_t number(std::string_view value, std::string_view label, std::uint64_t maximum,
                     bool allow_zero = false) {
    std::uint64_t parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (value.empty() || result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
        (!allow_zero && parsed == 0) || parsed > maximum) {
        throw std::invalid_argument("invalid " + std::string(label) + ": " + std::string(value));
    }
    return parsed;
}

std::vector<std::string> csv(std::string_view value) {
    std::vector<std::string> result;
    while (true) {
        const auto end = value.find(',');
        const auto part = value.substr(0, end);
        if (part.empty()) {
            throw std::invalid_argument("category lists cannot contain empty entries");
        }
        result.emplace_back(part);
        if (end == std::string_view::npos) {
            return result;
        }
        value.remove_prefix(end + 1);
    }
}

void hexadecimal(std::ostream& output, std::uint32_t value, unsigned int digits) {
    constexpr std::string_view alphabet = "0123456789ABCDEF";
    for (auto position = digits; position != 0; --position) {
        const auto shift = (position - 1U) * 4U;
        output.put(alphabet[(value >> shift) & 0xFU]);
    }
}

void json_scalar(std::ostream& output, char32_t character) {
    (void)utf8_width(character);
    // Escape every scalar: JSON stays ASCII even with invisible or bidi input.
    if (character <= 0xFFFFU) {
        output << "\\u";
        hexadecimal(output, static_cast<std::uint32_t>(character), 4);
    } else {
        const auto adjusted = static_cast<std::uint32_t>(character - 0x10000U);
        output << "\\u";
        hexadecimal(output, 0xD800U + (adjusted >> 10U), 4);
        output << "\\u";
        hexadecimal(output, 0xDC00U + (adjusted & 0x3FFU), 4);
    }
}

void codepoint(std::ostream& output, char32_t character) {
    output << "U+";
    const auto digits = character <= 0xFFFFU ? 4U : (character <= 0xFFFFFU ? 5U : 6U);
    hexadecimal(output, static_cast<std::uint32_t>(character), digits);
}

} // namespace

Options parse_options(const std::vector<std::string_view>& arguments) {
    Options options;
    std::size_t first = 0;
    if (!arguments.empty()) {
        if (arguments[0] == "generate") {
            first = 1;
        } else if (arguments[0] == "corpus") {
            options.command = Command::corpus;
            options.profile = "unspeakable";
            options.format = Format::jsonl;
            first = 1;
        } else if (arguments[0] == "sets") {
            options.command = Command::sets;
            first = 1;
        } else if (arguments[0] == "help") {
            options.command = Command::help;
            first = 1;
        } else if (arguments[0] == "version") {
            options.command = Command::show_version;
            first = 1;
        }
    }
    const bool is_corpus = options.command == Command::corpus;
    std::set<std::string> specified;
    std::size_t positional_count = 0;
    bool positional_only = false;
    bool generation_argument = false;
    const auto unique = [&specified](std::string_view key) {
        if (!specified.emplace(key).second) {
            throw std::invalid_argument("option specified more than once: " + std::string(key));
        }
    };
    for (std::size_t i = first; i < arguments.size(); ++i) {
        auto argument = arguments[i];
        if (argument == "--" && !positional_only) {
            positional_only = true;
            continue;
        }
        if (!positional_only && argument.size() > 2 && argument.substr(0, 2) == "--") {
            const auto equal = argument.find('=');
            const auto name = argument.substr(2, equal == std::string_view::npos
                                                     ? equal : equal - 2);
            if (name == "help" || name == "version") {
                if (equal != std::string_view::npos) {
                    throw std::invalid_argument("this option does not accept a value");
                }
                options.command = name == "help" ? Command::help : Command::show_version;
                continue;
            }
            if (name != "profile" && name != "length" && name != "count" &&
                name != "max-bytes" && name != "format" && name != "include" &&
                name != "exclude" && name != "require" && name != "seed") {
                throw std::invalid_argument("unknown option: --" + std::string(name));
            }
            unique(name);
            generation_argument = true;
            std::string_view value;
            if (equal != std::string_view::npos) {
                value = argument.substr(equal + 1);
            } else {
                if (++i == arguments.size()) {
                    throw std::invalid_argument("missing value for --" + std::string(name));
                }
                value = arguments[i];
            }
            if (name == "profile") {
                if (value != "portable" && value != "unicode" &&
                    value != "unspeakable" && value != "stress") {
                    throw std::invalid_argument("unknown profile: " + std::string(value));
                }
                options.profile = value == "stress" ? "unspeakable" : value;
            } else if (name == "length") {
                options.length = static_cast<std::size_t>(number(value, name, maximum_length));
            } else if (name == "count") {
                options.count = static_cast<std::size_t>(number(value, name, maximum_count));
            } else if (name == "max-bytes") {
                options.max_bytes = static_cast<std::size_t>(
                    number(value, name, maximum_length * 4));
            } else if (name == "seed") {
                if (!is_corpus) {
                    throw std::invalid_argument("--seed is available only for corpus test data");
                }
                options.seed = number(value, name, UINT64_MAX, true);
            } else if (name == "include") {
                options.include = csv(value);
            } else if (name == "exclude") {
                const auto values = csv(value);
                options.exclude.insert(options.exclude.end(), values.begin(), values.end());
            } else if (name == "require") {
                options.require = csv(value);
            } else if (value == "raw") {
                options.format = Format::raw;
            } else if (value == "numbered") {
                options.format = Format::numbered;
            } else if (value == "escaped") {
                options.format = Format::escaped;
            } else if (value == "jsonl") {
                options.format = Format::jsonl;
            } else {
                throw std::invalid_argument("format must be raw, numbered, escaped, or jsonl");
            }
        } else if (!positional_only && argument.size() > 1 && argument[0] == '-') {
            argument.remove_prefix(1);
            for (const auto flag : argument) {
                if (flag == 'h') {
                    options.command = Command::help;
                    continue;
                }
                bool found = false;
                for (const auto& entry : unicode_data::categories) {
                    if (entry.legacy_flag == flag) {
                        options.exclude.emplace_back(entry.id);
                        generation_argument = true;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    throw std::invalid_argument("unknown short option: -" + std::string(1, flag));
                }
            }
        } else {
            generation_argument = true;
            if (positional_count == 0) {
                unique("length");
                options.length = static_cast<std::size_t>(
                    number(argument, "length", maximum_length));
            } else if (positional_count == 1) {
                unique("count");
                options.count = static_cast<std::size_t>(
                    number(argument, "count", maximum_count));
            } else {
                throw std::invalid_argument("too many positional arguments");
            }
            ++positional_count;
        }
    }
    if (options.command == Command::sets && generation_argument) {
        throw std::invalid_argument("sets does not accept generation options");
    }
    if (options.length > maximum_total_length / options.count) {
        throw std::invalid_argument("length times count must not exceed 1000000 code points");
    }
    return options;
}

void print_help(std::ostream& output) {
    output << "upwgen " << version << " - Unspeakable Password Generator\n"
        "Usage: upwgen [generate|corpus] [options] [length [count]]\n"
        "       upwgen sets | help | version\n\n"
        "  --profile portable|unicode|unspeakable (stress is an alias)\n"
        "  --length N      Exact code-point count, 1..4096 (default 24)\n"
        "  --count N       Number of values, 1..10000 (default 1)\n"
        "  --max-bytes N   Maximum UTF-8 bytes, 1..16384\n"
        "  --include CSV   Replace the profile's selected categories\n"
        "  --exclude CSV   Remove selected categories\n"
        "  --require CSV   Require at least one from each selected category\n"
        "  --format raw|numbered|escaped|jsonl\n"
        "  --seed N        Corpus only: unsigned 64-bit seed (default 0)\n"
        "  -h, --help      Show help; --version shows the version\n\n"
        "Default: portable, 24 code points, one value, raw output, no requirements.\n"
        "Corpus defaults: unspeakable, JSONL. Corpus values are TEST DATA, not secrets.\n"
        "Limits: length * count <= 1000000; constrained sampling may fail if too rare.\n"
        "Raw output is one value per line. Errors go to stderr.\n"
        "Unicode length counts code points, not bytes or visible graphemes.\n"
        "Unspeakable may include invisible characters and combining marks.\n"
        "Legacy exclusions (combinable): -s symbols, -c upper, -l lower, -d digits,\n"
        "  -n tags, -m combining, -e emoji, -x historic, -o braille, -g games.\n"
        "Legacy flags preserve the chosen profile; categories are not required by default.\n";
}

void print_sets(std::ostream& output) {
    output << "Unicode " << unicode_data::unicode_version << "\n";
    for (const auto& entry : unicode_data::categories) {
        output << entry.id << "\t" << entry.characters.size() << " assigned\t"
               << entry.visible.size() << " visible\t" << entry.description << "\n";
    }
}

void write_result(std::ostream& output, const Options& options, const Policy& policy,
                  const SecretBuffer<char32_t>& value, std::size_t index) {
    const auto encoded = utf8(value);
    if (options.format == Format::raw || options.format == Format::numbered) {
        if (options.format == Format::numbered) {
            output << index << ": |";
        }
        output.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
        if (options.format == Format::numbered) {
            output.put('|');
        }
    } else if (options.format == Format::escaped) {
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (i != 0) {
                output.put(' ');
            }
            codepoint(output, value[i]);
        }
    } else {
        output << "{\"index\":" << index << ",\"value\":\"";
        for (std::size_t i = 0; i < value.size(); ++i) {
            json_scalar(output, value[i]);
        }
        output << "\",\"mode\":\""
               << (options.command == Command::corpus ? "corpus" : "generate")
               << "\",\"profile\":\"" << policy.profile
               << "\",\"code_points\":" << value.size()
               << ",\"utf8_bytes\":" << encoded.size()
               << ",\"unicode_version\":\"" << unicode_data::unicode_version
               << "\",\"normalization\":\"not_applied\",\"categories\":[";
        for (std::size_t i = 0; i < policy.categories.size(); ++i) {
            if (i != 0) {
                output.put(',');
            }
            output << '"' << policy.categories[i] << '"';
        }
        output << "],\"codepoint_sequence\":[";
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (i != 0) {
                output.put(',');
            }
            output.put('"');
            codepoint(output, value[i]);
            output.put('"');
        }
        output.put(']');
        if (options.command == Command::corpus) {
            output << ",\"seed\":\"" << options.seed << '"';
        }
        output.put('}');
    }
    output.put('\n');
    output.flush();
    if (!output) {
        throw std::runtime_error("cannot write output");
    }
}

} // namespace upwgen
