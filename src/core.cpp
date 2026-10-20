// SPDX-License-Identifier: GPL-3.0-or-later
#include "upwgen/core.hpp"
#include "upwgen/categories_generated.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iterator>
#include <set>
#include <system_error>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#elif defined(__linux__)
#include <sys/random.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__) || defined(__DragonFly__)
#include <cstdlib>
#else
#error "No supported operating-system cryptographic random source on this platform."
#endif

namespace upwgen {

void wipe(void* address, std::size_t bytes) noexcept {
    // Volatile writes are observable side effects and cannot be optimized away.
    auto* current = static_cast<volatile unsigned char*>(address);
    while (bytes != 0) {
        *current++ = 0;
        --bytes;
    }
}

namespace {

void system_random(void* destination, std::size_t size) {
#if defined(_WIN32)
    if (size > std::numeric_limits<ULONG>::max() ||
        BCryptGenRandom(nullptr, static_cast<PUCHAR>(destination),
                        static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
        throw std::runtime_error("Windows cryptographic random source failed");
    }
#elif defined(__linux__)
    auto* bytes = static_cast<unsigned char*>(destination);
    while (size != 0) {
        const auto received = getrandom(bytes, size, 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::system_error(errno, std::generic_category(),
                                    "operating-system cryptographic random source failed");
        }
        if (received == 0) {
            throw std::runtime_error("operating-system random source returned no data");
        }
        const auto used = static_cast<std::size_t>(received);
        bytes += used;
        size -= used;
    }
#else
    // arc4random_buf aborts internally if the OS cannot supply secure randomness.
    arc4random_buf(destination, size);
#endif
}

const unicode_data::Category& category(std::string_view name) {
    for (const auto& entry : unicode_data::categories) {
        if (entry.id == name) {
            return entry;
        }
    }
    throw std::invalid_argument("unknown category: " + std::string(name));
}

std::u32string normalized_pool(std::u32string_view input) {
    std::u32string pool(input);
    for (const auto character : pool) {
        (void)utf8_width(character);
    }
    std::sort(pool.begin(), pool.end());
    pool.erase(std::unique(pool.begin(), pool.end()), pool.end());
    return pool;
}

bool contains(const std::u32string& pool, char32_t character) {
    return std::binary_search(pool.begin(), pool.end(), character);
}

} // namespace

std::uint32_t SystemRandom::next() {
    if (cursor_ == words_.size()) {
        system_random(words_.data(), words_.size() * sizeof(std::uint32_t));
        cursor_ = 0;
    }
    const auto result = words_[cursor_];
    words_[cursor_++] = 0;
    return result;
}

std::uint32_t CorpusRandom::next() {
    state_ += UINT64_C(0x9E3779B97F4A7C15);
    auto value = state_;
    value = (value ^ (value >> 30U)) * UINT64_C(0xBF58476D1CE4E5B9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94D049BB133111EB);
    value ^= value >> 31U;
    return static_cast<std::uint32_t>(value >> 32U);
}

std::uint32_t bounded(RandomSource& random, std::uint32_t bound) {
    if (bound == 0) {
        throw std::invalid_argument("cannot choose from an empty alphabet");
    }
    const auto threshold = static_cast<std::uint32_t>(0U - bound) % bound;
    // A broken injected source must not hang the process indefinitely.
    for (std::size_t attempt = 0; attempt < 1024; ++attempt) {
        const auto value = random.next();
        if (value >= threshold) {
            return value % bound;
        }
    }
    throw std::runtime_error("random source repeatedly failed bounded sampling");
}

std::size_t utf8_width(char32_t character) {
    if (character <= 0x7FU) {
        return 1;
    }
    if (character <= 0x7FFU) {
        return 2;
    }
    if (character >= 0xD800U && character <= 0xDFFFU) {
        throw std::invalid_argument("surrogates are not Unicode scalar values");
    }
    if (character <= 0xFFFFU) {
        return 3;
    }
    if (character <= 0x10FFFFU) {
        return 4;
    }
    throw std::invalid_argument("value is outside the Unicode scalar range");
}

SecretBuffer<char> utf8(const SecretBuffer<char32_t>& characters) {
    std::size_t size = 0;
    for (std::size_t i = 0; i < characters.size(); ++i) {
        const auto width = utf8_width(characters[i]);
        if (size > std::numeric_limits<std::size_t>::max() - width) {
            throw std::length_error("UTF-8 output is too large");
        }
        size += width;
    }
    SecretBuffer<char> result(size);
    std::size_t offset = 0;
    for (std::size_t i = 0; i < characters.size(); ++i) {
        const auto character = characters[i];
        if (character <= 0x7FU) {
            result[offset++] = static_cast<char>(character);
        } else if (character <= 0x7FFU) {
            result[offset++] = static_cast<char>(0xC0U | (character >> 6U));
            result[offset++] = static_cast<char>(0x80U | (character & 0x3FU));
        } else if (character <= 0xFFFFU) {
            result[offset++] = static_cast<char>(0xE0U | (character >> 12U));
            result[offset++] = static_cast<char>(0x80U | ((character >> 6U) & 0x3FU));
            result[offset++] = static_cast<char>(0x80U | (character & 0x3FU));
        } else {
            result[offset++] = static_cast<char>(0xF0U | (character >> 18U));
            result[offset++] = static_cast<char>(0x80U | ((character >> 12U) & 0x3FU));
            result[offset++] = static_cast<char>(0x80U | ((character >> 6U) & 0x3FU));
            result[offset++] = static_cast<char>(0x80U | (character & 0x3FU));
        }
    }
    return result;
}

Policy make_policy(
    std::string_view profile, const std::vector<std::string>& include,
    const std::vector<std::string>& exclude, const std::vector<std::string>& require,
    std::size_t length, std::size_t max_bytes) {
    if (profile == "stress") {
        profile = "unspeakable";
    }
    if (profile != "portable" && profile != "unicode" && profile != "unspeakable") {
        throw std::invalid_argument("profile must be portable, unicode, or unspeakable");
    }
    if (length == 0 || length > maximum_length) {
        throw std::invalid_argument("length must be between 1 and 4096 code points");
    }
    if (max_bytes > maximum_length * 4) {
        throw std::invalid_argument("max-bytes must be between 1 and 16384");
    }
    Policy policy;
    policy.profile = profile;
    policy.length = length;
    policy.max_bytes = max_bytes;
    std::set<std::string> selected;
    for (const auto& entry : unicode_data::categories) {
        if (profile == "unspeakable" ||
            (profile == "unicode" && !entry.visible.empty()) ||
            (profile == "portable" && (entry.id == "symbols" || entry.id == "upper" ||
                                       entry.id == "lower" || entry.id == "digits"))) {
            selected.emplace(entry.id);
        }
    }
    if (!include.empty()) {
        selected.clear();
        for (const auto& name : include) {
            (void)category(name);
            selected.insert(name);
        }
    }
    for (const auto& name : exclude) {
        (void)category(name);
        selected.erase(name);
    }
    // Catalog order is stable across platforms and independent of option order.
    for (const auto& entry : unicode_data::categories) {
        if (selected.count(std::string(entry.id)) == 0) {
            continue;
        }
        auto pool = normalized_pool(profile == "unicode" ? entry.visible : entry.characters);
        if (pool.empty()) {
            throw std::invalid_argument("category has no characters in this profile: " +
                                        std::string(entry.id));
        }
        if (profile == "portable" &&
            std::any_of(pool.begin(), pool.end(), [](char32_t c) { return c > 0x7FU; })) {
            throw std::invalid_argument("portable profile only permits ASCII categories: " +
                                        std::string(entry.id));
        }
        policy.categories.emplace_back(entry.id);
        policy.alphabet.append(pool);
    }
    policy.alphabet = normalized_pool(policy.alphabet);
    if (policy.alphabet.empty()) {
        throw std::invalid_argument("all character sets disabled; alphabet is empty");
    }
    std::set<std::string> required_names;
    for (const auto& name : require) {
        const auto& entry = category(name);
        if (selected.count(name) == 0) {
            throw std::invalid_argument("required category is not included: " + name);
        }
        if (required_names.insert(name).second) {
            policy.required.push_back(normalized_pool(
                profile == "unicode" ? entry.visible : entry.characters));
        }
    }
    if (policy.required.size() > length) {
        throw std::invalid_argument("length is smaller than the number of required categories");
    }
    if (max_bytes != 0) {
        auto minimum = std::size_t{4};
        for (const auto character : policy.alphabet) {
            minimum = std::min(minimum, utf8_width(character));
        }
        auto minimum_bytes = minimum * (length - policy.required.size());
        for (const auto& pool : policy.required) {
            auto required_minimum = std::size_t{4};
            for (const auto character : pool) {
                required_minimum = std::min(required_minimum, utf8_width(character));
            }
            minimum_bytes += required_minimum;
        }
        if (minimum_bytes > max_bytes) {
            throw std::invalid_argument("max-bytes cannot fit this length and required categories");
        }
    }
    return policy;
}

SecretBuffer<char32_t> generate(const Policy& policy, RandomSource& random,
                               std::size_t* draws_remaining) {
    if (policy.alphabet.empty() || policy.alphabet.size() > UINT32_MAX ||
        policy.length == 0 || policy.length > maximum_length) {
        throw std::invalid_argument("invalid generator policy");
    }
    SecretBuffer<char32_t> result(policy.length);
    std::size_t local_budget = 4000000;
    auto& budget = draws_remaining == nullptr ? local_budget : *draws_remaining;
    const auto retries = std::min(std::size_t{100000}, std::size_t{4000000} / policy.length);
    const auto bound = static_cast<std::uint32_t>(policy.alphabet.size());
    for (std::size_t attempt = 0; attempt < retries; ++attempt) {
        if (budget < policy.length) {
            throw std::runtime_error(
                "batch sampling budget exhausted; relax constraints or reduce count");
        }
        budget -= policy.length;
        std::size_t bytes = 0;
        for (std::size_t i = 0; i < result.size(); ++i) {
            result[i] = policy.alphabet[bounded(random, bound)];
            bytes += utf8_width(result[i]);
        }
        if (policy.max_bytes != 0 && bytes > policy.max_bytes) {
            continue;
        }
        bool valid = true;
        for (const auto& required : policy.required) {
            bool found = false;
            for (std::size_t i = 0; i < result.size(); ++i) {
                if (contains(required, result[i])) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                valid = false;
                break;
            }
        }
        if (valid) {
            return result;
        }
    }
    throw std::runtime_error(
        "constraints were too unlikely to sample within the attempt limit; "
        "increase length, increase max-bytes, or require fewer categories");
}

} // namespace upwgen
