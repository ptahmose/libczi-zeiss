// SPDX-FileCopyrightText: 2026 Carl Zeiss Microscopy GmbH
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "../JxrDecode/JxrDecode.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace
{
    constexpr int kWarmup = 5;
    constexpr int kIterations = 50;
    constexpr std::uint32_t kWidth = 2048;
    constexpr std::uint32_t kHeight = 2048;
    constexpr char kExpectedPixelsSha256[] = "5ab3bc773ddd5b65c810b240022bdfb6a3caaebfaa29f29520025b5088e843c8";

    struct EncodedImage
    {
        const std::uint8_t* data;
        std::size_t size;
    };

    EncodedImage GetFixture()
    {
        const HMODULE module = GetModuleHandleW(nullptr);
        const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(101), MAKEINTRESOURCEW(10));
        if (!resource)
        {
            throw std::runtime_error("Cannot find embedded JPEG-XR fixture.");
        }

        const DWORD size = SizeofResource(module, resource);
        const HGLOBAL loaded = LoadResource(module, resource);
        const auto data = static_cast<const std::uint8_t*>(LockResource(loaded));
        if (!size || !loaded || !data)
        {
            throw std::runtime_error("Cannot load embedded JPEG-XR fixture.");
        }

        return { data, size };
    }

    std::vector<std::uint8_t> Decode(const EncodedImage& image)
    {
        std::vector<std::uint8_t> pixels;
        using JxrDecode = libCZI::detail::JxrDecode;
        JxrDecode::Decode(image.data, image.size,
            [&pixels](JxrDecode::PixelFormat format, std::uint32_t width, std::uint32_t height)
                -> std::tuple<void*, std::uint32_t>
            {
                if (format != JxrDecode::PixelFormat::kGray16 || width != kWidth || height != kHeight)
                {
                    throw std::runtime_error("Fixture is not the expected 2048x2048 Gray16 image.");
                }

                const std::uint32_t stride = width * 2;
                pixels.resize(static_cast<std::size_t>(stride) * height);
                return std::make_tuple(pixels.data(), stride);
            });
        return pixels;
    }

    std::string Sha256(const std::uint8_t* data, std::size_t size)
    {
        if (size > (std::numeric_limits<ULONG>::max)())
        {
            throw std::runtime_error("Input is too large to hash.");
        }

        BCRYPT_ALG_HANDLE algorithm = nullptr;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        {
            throw std::runtime_error("Cannot open SHA-256 provider.");
        }

        std::array<std::uint8_t, 32> hash = {};
        const NTSTATUS status = BCryptHash(
            algorithm, nullptr, 0, const_cast<PUCHAR>(data), static_cast<ULONG>(size),
            hash.data(), static_cast<ULONG>(hash.size()));
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (status < 0)
        {
            throw std::runtime_error("Cannot calculate SHA-256.");
        }

        std::ostringstream result;
        result << std::hex << std::setfill('0');
        for (const auto byte : hash)
        {
            result << std::setw(2) << static_cast<unsigned int>(byte);
        }
        return result.str();
    }
}

int main()
{
    try
    {
        const auto image = GetFixture();
        std::cout << "WPF JPEG-XR fixture: " << kWidth << "x" << kHeight << " Gray16, "
                  << image.size << " encoded bytes\n"
                  << "Encoded SHA-256: " << Sha256(image.data, image.size) << "\n"
                  << "Warmup/iterations: " << kWarmup << "/" << kIterations << "\n";

        for (int i = 0; i < kWarmup; ++i)
        {
            Decode(image);
        }

        std::vector<double> timings;
        timings.reserve(kIterations);
        std::vector<std::uint8_t> pixels;
        for (int i = 0; i < kIterations; ++i)
        {
            const auto start = std::chrono::steady_clock::now();
            auto decoded = Decode(image);
            const auto stop = std::chrono::steady_clock::now();
            timings.push_back(std::chrono::duration<double, std::milli>(stop - start).count());
            pixels = std::move(decoded);
        }

        const std::string pixelHash = Sha256(pixels.data(), pixels.size());
        std::cout << "Decoded pixel SHA-256: " << pixelHash << "\n";
        if (pixelHash != kExpectedPixelsSha256)
        {
            throw std::runtime_error("Decoded pixels differ from the WPF reference.");
        }

        std::sort(timings.begin(), timings.end());
        std::cout << std::fixed << std::setprecision(3)
                  << "Decode time: min " << timings.front()
                  << " ms, median " << timings[(kIterations - 1) / 2]
                  << " ms, max " << timings.back() << " ms\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Benchmark failed: " << e.what() << "\n";
        return 1;
    }
}
