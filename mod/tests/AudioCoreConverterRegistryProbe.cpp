#include "audio_v2/AudioCoreConverterRegistry.h"

#include <cstdint>
#include <cstdio>

using namespace ammod::audio_v2;

namespace {
constexpr std::uint32_t kAlac = 0x616C6163;
constexpr std::uint32_t kQlac = 0x716C6163;

bool Check(bool condition, const char* name) {
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", name);
    return condition;
}

bool LatePrecisionOnRegisteredConverter() {
    AudioCoreConverterRegistry registry;
    auto* converter = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1000));

    ConverterRegistration registration{};
    registration.converter = converter;
    registration.outputFormat = {
        44100, 2, 24, 32, PcmEncoding::SignedInteger, true, 0
    };
    registration.sourceBitDepth = 0; // ALAC precision has not arrived yet.
    registration.encodedFormat = kAlac;
    registration.observationId = 102;
    registration.mediaGeneration = 1;

    if (!registry.Register(registration)) return false;

    void* provisional{};
    std::uint32_t provisionalDepth = UINT32_MAX;
    if (!registry.FindCandidateAnyPrecision(44100, 2, 102,
                                            provisional, provisionalDepth) ||
        provisional != converter || provisionalDepth != 0) {
        return false;
    }

    if (!registry.UpdateSourcePrecision(converter, 16, 44100, 2)) return false;

    void* resolved{};
    if (registry.FindCandidate(44100, 2, 16, 101, resolved)) return false;
    if (!registry.FindCandidate(44100, 2, 16, 102, resolved) ||
        resolved != converter) {
        return false;
    }
    if (!registry.Bind(resolved, 1)) return false;

    ConverterRegistrationSnapshot snapshot{};
    return registry.Snapshot(converter, snapshot) &&
        snapshot.registration.sourceBitDepth == 16 &&
        snapshot.bound;
}

bool CookiePrecisionMatchesCrossHandleRegistration() {
    AudioCoreConverterRegistry registry;
    auto* p0Converter = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x2000));

    ConverterRegistration registration{};
    registration.converter = p0Converter;
    registration.outputFormat = {
        96000, 2, 24, 32, PcmEncoding::SignedInteger, true, 0
    };
    registration.sourceBitDepth = 0;
    registration.encodedFormat = kQlac;
    registration.observationId = 207;
    registration.mediaGeneration = 1;
    if (!registry.Register(registration)) return false;

    void* matched{};
    if (!registry.UpdateCompressedPrecision(24, 96000, 2, 0, matched) ||
        matched != p0Converter) {
        return false;
    }

    void* resolved{};
    if (!registry.FindCandidate(96000, 2, 24, 207, resolved) ||
        resolved != p0Converter) {
        return false;
    }
    return registry.Bind(resolved, 1);
}
} // namespace

int main() {
    int failures = 0;
    if (!Check(LatePrecisionOnRegisteredConverter(),
               "late ALAC precision resolves the exact graph observation")) {
        ++failures;
    }
    if (!Check(CookiePrecisionMatchesCrossHandleRegistration(),
               "cookie precision resolves a cross-handle QLAC registration")) {
        ++failures;
    }
    std::printf("2 registry cases: %d failed\n", failures);
    return failures == 0 ? 0 : 1;
}
