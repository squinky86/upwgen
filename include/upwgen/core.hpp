// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "upwgen/version.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace upwgen {

inline constexpr std::size_t maximum_length = 4096;
inline constexpr std::size_t maximum_count = 10000;
inline constexpr std::size_t maximum_total_length = 1000000;

void wipe(void* address, std::size_t bytes) noexcept;

// Always heap backed: no small-string optimization can bypass the destructor.
// This is best-effort cleanup; the terminal and operating system may keep copies.
template <class T>
class SecretBuffer {
public:
    explicit SecretBuffer(std::size_t size) : size_(size) {
        if (size > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::length_error("secret buffer is too large");
        }
        data_ = std::make_unique<T[]>(size);
    }
    ~SecretBuffer() { clear(); }
    SecretBuffer(const SecretBuffer&) = delete;
    SecretBuffer& operator=(const SecretBuffer&) = delete;
    SecretBuffer(SecretBuffer&& other) noexcept
        : size_(std::exchange(other.size_, 0)), data_(std::move(other.data_)) {}
    SecretBuffer& operator=(SecretBuffer&& other) noexcept {
        if (this != &other) {
            clear();
            size_ = std::exchange(other.size_, 0);
            data_ = std::move(other.data_);
        }
        return *this;
    }
    T* data() noexcept { return data_.get(); }
    const T* data() const noexcept { return data_.get(); }
    std::size_t size() const noexcept { return size_; }
    T& operator[](std::size_t index) noexcept { return data_[index]; }
    const T& operator[](std::size_t index) const noexcept { return data_[index]; }
private:
    void clear() noexcept { wipe(data_.get(), size_ * sizeof(T)); }
    std::size_t size_;
    std::unique_ptr<T[]> data_;
};

class RandomSource {
public:
    virtual ~RandomSource() = default;
    virtual std::uint32_t next() = 0;
};

class SystemRandom final : public RandomSource {
public:
    SystemRandom() : words_(256) {}
    std::uint32_t next() override;
private:
    SecretBuffer<std::uint32_t> words_;
    std::size_t cursor_ = 256;
};

// SplitMix64 is used only for reproducible, explicitly non-secret test corpora.
class CorpusRandom final : public RandomSource {
public:
    explicit CorpusRandom(std::uint64_t seed) : state_(seed) {}
    std::uint32_t next() override;
private:
    std::uint64_t state_;
};

std::uint32_t bounded(RandomSource& random, std::uint32_t bound);
std::size_t utf8_width(char32_t character);
SecretBuffer<char> utf8(const SecretBuffer<char32_t>& characters);

struct Policy {
    std::string profile;
    std::vector<std::string> categories;
    std::u32string alphabet;
    std::vector<std::u32string> required;
    std::size_t length = 24;
    std::size_t max_bytes = 0;
};

Policy make_policy(
    std::string_view profile, const std::vector<std::string>& include,
    const std::vector<std::string>& exclude, const std::vector<std::string>& require,
    std::size_t length, std::size_t max_bytes);

SecretBuffer<char32_t> generate(const Policy& policy, RandomSource& random,
                               std::size_t* draws_remaining = nullptr);

} // namespace upwgen
