// SPDX-License-Identifier: GPL-3.0-or-later
#include "upwgen/options.hpp"
#include "upwgen/categories_generated.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <streambuf>
#include <type_traits>

namespace {

void check(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

template <class Action>
void rejects(Action action, std::string_view message) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception&) {
        failed = true;
    }
    check(failed, message);
}

class SequenceRandom final : public upwgen::RandomSource {
public:
    explicit SequenceRandom(std::vector<std::uint32_t> values) : values_(std::move(values)) {}
    std::uint32_t next() override {
        if (cursor_ == values_.size()) {
            throw std::runtime_error("fake entropy source exhausted");
        }
        return values_[cursor_++];
    }
    std::size_t used() const { return cursor_; }
private:
    std::vector<std::uint32_t> values_;
    std::size_t cursor_ = 0;
};

class ZeroRandom final : public upwgen::RandomSource {
public:
    std::uint32_t next() override { return 0; }
};

class FailedOutput final : public std::streambuf {
protected:
    std::streamsize xsputn(const char*, std::streamsize) override { return 0; }
    int_type overflow(int_type) override { return traits_type::eof(); }
};

std::string bytes(const upwgen::SecretBuffer<char32_t>& value) {
    const auto encoded = upwgen::utf8(value);
    return std::string(encoded.data(), encoded.size());
}

void test_random() {
    SequenceRandom random({0, 5, 6, 7, 4294967295U});
    check(upwgen::bounded(random, 10) == 6, "bounded sampling must reject biased prefix");
    check(random.used() == 3, "bounded sampling consumed incorrect count");
    check(upwgen::bounded(random, 10) == 7, "bounded sampling mapped incorrectly");
    check(upwgen::bounded(random, 10) == 5, "maximum unsigned value mapped incorrectly");
    rejects([&] { (void)upwgen::bounded(random, 0); }, "empty random bound accepted");
    rejects([&] { (void)upwgen::bounded(random, 2); }, "entropy failure was swallowed");
    ZeroRandom zero;
    rejects([&] { (void)upwgen::bounded(zero, 10); }, "broken RNG loops forever");
    upwgen::CorpusRandom corpus(0);
    check(corpus.next() == 0xE220A839U, "seeded corpus changed across platforms");
    check(corpus.next() == 0x6E789E6AU, "seeded corpus second word changed");
    upwgen::SystemRandom system;
    for (std::size_t i = 0; i < 300; ++i) {
        (void)system.next();
    }
}

void test_secret_and_utf8() {
    static_assert(!std::is_copy_constructible<upwgen::SecretBuffer<char>>::value,
                  "secret buffers must not be copied");
    std::array<unsigned char, 8> secret{{1, 2, 3, 4, 5, 6, 7, 8}};
    upwgen::wipe(secret.data(), secret.size());
    check(std::all_of(secret.begin(), secret.end(), [](unsigned char c) { return c == 0; }),
          "memory wipe failed");
    rejects([] {
        upwgen::SecretBuffer<std::uint64_t> too_large(std::numeric_limits<std::size_t>::max());
    }, "allocation size overflow accepted");
    upwgen::SecretBuffer<char32_t> value(4);
    value[0] = U'A';
    value[1] = U'\u00E9';
    value[2] = U'\u20AC';
    value[3] = U'\U0001F600';
    check(bytes(value) == std::string("A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", 10),
          "UTF-8 encoding failed");
    auto moved = std::move(value);
    check(value.size() == 0 && value.data() == nullptr && moved.size() == 4,
          "secret move retained ownership");
    upwgen::SecretBuffer<char32_t> other(1);
    other = std::move(moved);
    check(moved.size() == 0 && other.size() == 4, "secret move assignment failed");
    for (const auto invalid : {char32_t{0xD800}, char32_t{0xDFFF}, char32_t{0x110000}}) {
        rejects([&] { (void)upwgen::utf8_width(invalid); }, "invalid Unicode scalar accepted");
    }
    check(upwgen::utf8_width(0x10FFFF) == 4, "maximum Unicode scalar rejected");
}

void test_options() {
    using upwgen::parse_options;
    auto options = parse_options({});
    check(options.length == 24 && options.count == 1 && options.profile == "portable" &&
          options.format == upwgen::Format::raw, "default options changed");
    options = parse_options({"corpus", "--seed=18446744073709551615", "--length", "7"});
    check(options.command == upwgen::Command::corpus &&
          options.format == upwgen::Format::jsonl && options.seed == UINT64_MAX &&
          options.profile == "unspeakable" && options.length == 7, "corpus options incorrect");
    options = parse_options({"generate", "-nmx", "12", "3"});
    check(options.exclude == std::vector<std::string>({"tags", "combining", "historic"}) &&
          options.length == 12 && options.count == 3, "grouped legacy flags failed");
    options = parse_options({"--profile=stress", "--require=digits,upper", "--max-bytes=40"});
    check(options.profile == "unspeakable" && options.require.size() == 2 &&
          options.max_bytes == 40, "named options failed");
    const std::vector<std::vector<std::string_view>> invalid{
        {"--length", "0"}, {"--length", "-1"}, {"--length", "+1"}, {"--length", "1x"},
        {"--length", " 1"}, {"--length", "1 "}, {"--length", "4097"}, {"--count", "10001"},
        {"--length", "999999999999999999999999999999999"}, {"--length"}, {"--length="},
        {"--unknown"}, {"-Q"}, {"--seed", "1"}, {"generate", "--seed", "0"},
        {"corpus", "--seed", "18446744073709551616"}, {"corpus", "--seed", "-1"},
        {"--include", "upper,"}, {"--exclude", ",digits"}, {"--require", ""},
        {"--format", "xml"}, {"--profile", "unknown"}, {"--max-bytes", "0"},
        {"--max-bytes", "16385"}, {"--length", "4000", "--count", "1000"},
        {"1", "2", "3"}, {"1", "--length", "2"}, {"--count", "1", "--count", "2"},
        {"sets", "--length", "1"}, {"--help=1"}, {"--", "-1"}
    };
    for (const auto& arguments : invalid) {
        rejects([&] { (void)parse_options(arguments); }, "invalid CLI input accepted");
    }
}

void test_policy_and_generation() {
    using upwgen::make_policy;
    upwgen::CorpusRandom random(42);
    for (const auto length : {std::size_t{1}, std::size_t{9}, std::size_t{24}, std::size_t{4096}}) {
        const auto policy = make_policy("portable", {}, {}, {}, length, length);
        const auto generated = upwgen::generate(policy, random);
        check(generated.size() == length && bytes(generated).size() == length,
              "exact length or portable byte count violated");
        for (std::size_t i = 0; i < generated.size(); ++i) {
            check(generated[i] >= 0x21 && generated[i] <= 0x7E, "nonportable character generated");
        }
    }
    const auto constrained = make_policy("portable", {}, {}, {"upper", "lower", "digits"}, 24, 24);
    for (std::size_t sample = 0; sample < 30; ++sample) {
        const auto generated = upwgen::generate(constrained, random);
        for (const auto& pool : constrained.required) {
            bool found = false;
            for (std::size_t i = 0; i < generated.size(); ++i) {
                found = found || pool.find(generated[i]) != std::u32string::npos;
            }
            check(found, "required category absent");
        }
    }
    const auto unicode = make_policy("unicode", {}, {}, {}, 80, 0);
    check(std::find(unicode.categories.begin(), unicode.categories.end(), "tags") ==
              unicode.categories.end(), "Unicode profile includes tag characters");
    check(std::find(unicode.categories.begin(), unicode.categories.end(), "combining") ==
              unicode.categories.end(), "Unicode profile includes standalone marks");
    const auto stress = make_policy("stress", {"tags"}, {}, {"tags"}, 5, 20);
    check(upwgen::utf8(upwgen::generate(stress, random)).size() == 20,
          "tag-only stress profile failed byte accounting");
    const auto twice = make_policy("portable", {"digits", "digits"}, {}, {"digits", "digits"}, 1, 1);
    check(twice.alphabet.size() == 10 && twice.required.size() == 1, "pool deduplication failed");
    rejects([&] { (void)make_policy("portable", {}, {"symbols", "upper", "lower", "digits"}, {}, 1, 0); },
            "empty alphabet accepted");
    rejects([&] { (void)make_policy("portable", {"emoji"}, {}, {}, 1, 0); },
            "non-ASCII portable category accepted");
    rejects([&] { (void)make_policy("unicode", {"tags"}, {}, {}, 1, 0); },
            "empty visible category accepted");
    rejects([&] { (void)make_policy("portable", {}, {"upper"}, {"upper"}, 1, 0); },
            "excluded required category accepted");
    rejects([&] { (void)make_policy("portable", {}, {}, {"upper", "lower"}, 1, 0); },
            "impossible required length accepted");
    rejects([&] { (void)make_policy("unspeakable", {"emoji"}, {}, {}, 5, 19); },
            "impossible byte limit accepted");
    rejects([&] { (void)make_policy("portable", {}, {"typo"}, {}, 1, 0); },
            "unknown category accepted");
    SequenceRandom failed({});
    rejects([&] { (void)upwgen::generate(constrained, failed); },
            "generation swallowed entropy source failure");

    // Rejection sampling retries an over-budget candidate, preserving uniformity.
    upwgen::Policy budget;
    budget.alphabet = U"A\U0001F600";
    budget.length = 2;
    budget.max_bytes = 2;
    SequenceRandom choices({1, 0, 0, 0});
    check(bytes(upwgen::generate(budget, choices)) == "AA" && choices.used() == 4,
          "UTF-8 budget did not reject over-budget candidate");
    std::size_t batch_budget = 3;
    SequenceRandom batch_choices({0, 0, 0, 0});
    (void)upwgen::generate(budget, batch_choices, &batch_budget);
    rejects([&] { (void)upwgen::generate(budget, batch_choices, &batch_budget); },
            "batch sampling budget was not enforced");
    upwgen::Policy impossible;
    impossible.alphabet = U"AB";
    impossible.required = {U"B"};
    impossible.length = 1;
    ZeroRandom zero;
    rejects([&] { (void)upwgen::generate(impossible, zero); }, "constraint retry limit missing");
}

void test_outputs() {
    upwgen::SecretBuffer<char32_t> value(4);
    value[0] = U'"';
    value[1] = U'\\';
    value[2] = U'\U000E0020';
    value[3] = U'\U0001F600';
    const auto policy = upwgen::make_policy("unspeakable", {}, {}, {}, 4, 0);
    upwgen::Options options;
    std::ostringstream output;
    upwgen::write_result(output, options, policy, value, 1);
    check(output.str() == bytes(value) + "\n", "raw output adds metadata");
    options.format = upwgen::Format::escaped;
    output.str("");
    upwgen::write_result(output, options, policy, value, 1);
    check(output.str() == "U+0022 U+005C U+E0020 U+1F600\n", "escaped output incorrect");
    options.format = upwgen::Format::jsonl;
    options.command = upwgen::Command::corpus;
    options.seed = 42;
    output.str("");
    upwgen::write_result(output, options, policy, value, 1);
    check(output.str().find("\"value\":\"\\u0022\\u005C\\uDB40\\uDC20\\uD83D\\uDE00\"") !=
              std::string::npos, "JSON UTF-16 escaping incorrect");
    check(output.str().find("\"utf8_bytes\":10") != std::string::npos &&
          output.str().find("\"seed\":\"42\"") != std::string::npos, "JSON metadata incorrect");
    options.format = upwgen::Format::numbered;
    output.str("");
    upwgen::write_result(output, options, policy, value, 2);
    check(output.str() == "2: |" + bytes(value) + "|\n", "numbered output incorrect");
    FailedOutput sink;
    std::ostream broken(&sink);
    rejects([&] { upwgen::write_result(broken, options, policy, value, 1); },
            "output failure was ignored");
}

} // namespace

int main() {
    try {
        test_random();
        test_secret_and_utf8();
        test_options();
        test_policy_and_generation();
        test_outputs();
        std::cout << "Native tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Native test failure: " << error.what() << '\n';
        return 1;
    }
}
