#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "encode/IVideoEncoder.h"
#include "gpu/GpuSelect.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "deskhub/media/EncoderBackend.h"
#include "deskhubp/system/Clock.h"

using Microsoft::WRL::ComPtr;

namespace {

constexpr size_t kBytesPerPixel = 4;
constexpr size_t kUploadRingSize = 4;

constexpr const char* kFieldNames =
    "backend,requested,width,height,fps,bitrate_bps,frames,enc_us_p50,enc_us_p99,enc_us_max,"
    "cpu_pct,wall_ms,bytes_out,keyframes,ltr,intra_refresh";

struct BenchOptions {
    std::string clipPath;
    std::string bitstreamPath;
    std::string backend{deskhub::media::kEncoderBackendAuto};
    uint32_t width = 1280;
    uint32_t height = 720;
    uint32_t fps = 60;
    uint32_t bitrateBps = 20'000'000;
    uint32_t loops = 1;
};

std::string BackendList(const char* separator, bool withAuto) {
    std::string list;
    if (withAuto) list = std::string(deskhub::media::kEncoderBackendAuto);
    for (std::string_view id : BuiltInEncoderBackends()) {
        if (!list.empty()) list += separator;
        list += std::string(id);
    }
    return list;
}

void PrintUsage() {
    const std::string choices = BackendList("|", true);
    std::fprintf(stderr,
        "deskhub-encbench --clip FILE --width W --height H [options]\n"
        "\n"
        "  --clip FILE        raw BGRA frames, width*height*4 bytes each, no header\n"
        "  --width W          frame width in pixels\n"
        "  --height H         frame height in pixels\n"
        "  --fps N            frame rate handed to the encoder (default 60)\n"
        "  --bitrate BPS      target bitrate in bits per second (default 20000000)\n"
        "  --loops N          replay the clip N times (default 1)\n"
        "  --encoder NAME     %s (default auto)\n"
        "  --bitstream FILE   where to write the elementary stream\n"
        "  --fields           print the CSV header this tool emits, then exit\n"
        "  --backends         print the backends this build can start, then exit\n"
        "\n"
        "Frames go in at --fps, the way a live share submits them, so enc_us means the same\n"
        "thing here as it does on a host. Prints one CSV row on stdout. Feed every backend\n"
        "the same clip, the same size, the same fps and the same bitrate, or the rows do not\n"
        "belong in one table.\n",
        choices.c_str());
}

bool ParseU32(const char* text, uint32_t& out) {
    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value == 0 || value > 0xFFFFFFFFul) return false;
    out = uint32_t(value);
    return true;
}

bool ParseOptions(int argc, char** argv, BenchOptions& out) {
    for (int at = 1; at < argc; ++at) {
        const std::string flag = argv[at];
        const bool hasValue = at + 1 < argc;
        const char* value = hasValue ? argv[at + 1] : nullptr;

        if (flag == "--clip" && hasValue) {
            out.clipPath = value;
        } else if (flag == "--bitstream" && hasValue) {
            out.bitstreamPath = value;
        } else if (flag == "--encoder" && hasValue) {
            out.backend = value;
        } else if (flag == "--width" && hasValue) {
            if (!ParseU32(value, out.width)) return false;
        } else if (flag == "--height" && hasValue) {
            if (!ParseU32(value, out.height)) return false;
        } else if (flag == "--fps" && hasValue) {
            if (!ParseU32(value, out.fps)) return false;
        } else if (flag == "--bitrate" && hasValue) {
            if (!ParseU32(value, out.bitrateBps)) return false;
        } else if (flag == "--loops" && hasValue) {
            if (!ParseU32(value, out.loops)) return false;
        } else {
            return false;
        }
        ++at;
    }
    return !out.clipPath.empty();
}

bool ReadClip(const BenchOptions& options, std::vector<uint8_t>& bytes, size_t& frameCount) {
    const size_t frameBytes = size_t(options.width) * options.height * kBytesPerPixel;
    FILE* file = nullptr;
    if (fopen_s(&file, options.clipPath.c_str(), "rb") != 0 || file == nullptr) {
        std::fprintf(stderr, "encbench: cannot open %s\n", options.clipPath.c_str());
        return false;
    }
    _fseeki64(file, 0, SEEK_END);
    const long long size = _ftelli64(file);
    _fseeki64(file, 0, SEEK_SET);
    if (size <= 0 || size_t(size) % frameBytes != 0) {
        std::fprintf(stderr,
            "encbench: %s holds %lld bytes, which is not a whole number of %ux%u BGRA frames "
            "(%zu bytes each) - the clip and the --width/--height given here must be the same "
            "ones ffmpeg wrote, or every backend is measured on different pixels\n",
            options.clipPath.c_str(), size, options.width, options.height, frameBytes);
        std::fclose(file);
        return false;
    }
    bytes.resize(size_t(size));
    const size_t read = std::fread(bytes.data(), 1, bytes.size(), file);
    std::fclose(file);
    if (read != bytes.size()) {
        std::fprintf(stderr, "encbench: short read on %s\n", options.clipPath.c_str());
        return false;
    }
    frameCount = bytes.size() / frameBytes;
    return true;
}

struct UploadRing {
    ComPtr<ID3D11Texture2D> staging;
    std::vector<ComPtr<ID3D11Texture2D>> frames;
    size_t at = 0;

    bool Create(ID3D11Device* device, uint32_t width, uint32_t height) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) return false;

        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.CPUAccessFlags = 0;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        frames.resize(kUploadRingSize);
        for (ComPtr<ID3D11Texture2D>& texture : frames)
            if (FAILED(device->CreateTexture2D(&desc, nullptr, &texture))) return false;
        return true;
    }

    ID3D11Texture2D* Upload(ID3D11DeviceContext* context, const uint8_t* pixels, uint32_t width,
        uint32_t height) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_WRITE, 0, &mapped))) return nullptr;
        const size_t rowBytes = size_t(width) * kBytesPerPixel;
        for (uint32_t row = 0; row < height; ++row)
            std::memcpy(static_cast<uint8_t*>(mapped.pData) + size_t(row) * mapped.RowPitch,
                pixels + size_t(row) * rowBytes, rowBytes);
        context->Unmap(staging.Get(), 0);

        ID3D11Texture2D* target = frames[at].Get();
        at = (at + 1) % frames.size();
        context->CopyResource(target, staging.Get());
        context->Flush();
        return target;
    }
};

uint64_t PercentileUs(std::vector<uint64_t>& sorted, double fraction) {
    if (sorted.empty()) return 0;
    const size_t index = size_t(fraction * double(sorted.size() - 1) + 0.5);
    return sorted[std::min(index, sorted.size() - 1)];
}

uint64_t ProcessCpuUs() {
    FILETIME created{};
    FILETIME exited{};
    FILETIME kernel{};
    FILETIME user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) return 0;
    const auto ticks = [](const FILETIME& time) {
        return (uint64_t(time.dwHighDateTime) << 32) | time.dwLowDateTime;
    };
    return (ticks(kernel) + ticks(user)) / 10;
}

}

int main(int argc, char** argv) {
    for (int at = 1; at < argc; ++at) {
        const std::string arg(argv[at]);
        if (arg == "--fields") {
            std::printf("%s\n", kFieldNames);
            return 0;
        }
        if (arg == "--backends") {
            std::printf("%s\n", BackendList(" ", false).c_str());
            return 0;
        }
    }

    BenchOptions options;
    if (!ParseOptions(argc, argv, options)) {
        PrintUsage();
        return 2;
    }
    std::fprintf(stderr, "encbench: pid %lu\n",
        static_cast<unsigned long>(GetCurrentProcessId()));
    std::fflush(stderr);

    std::vector<uint8_t> clip;
    size_t clipFrames = 0;
    if (!ReadClip(options, clip, clipFrames)) return 1;

    GpuChoice gpu;
    if (!CreateBestDevice({GpuVendor::Nvidia, GpuVendor::Intel, GpuVendor::Amd}, gpu)) {
        std::fprintf(stderr, "encbench: no D3D11 device on this machine\n");
        return 1;
    }

    UploadRing ring;
    if (!ring.Create(gpu.device.Get(), options.width, options.height)) {
        std::fprintf(stderr, "encbench: could not create the %ux%u upload textures\n",
            options.width, options.height);
        return 1;
    }

    FILE* bitstream = nullptr;
    if (!options.bitstreamPath.empty() &&
        fopen_s(&bitstream, options.bitstreamPath.c_str(), "wb") != 0) {
        std::fprintf(stderr, "encbench: cannot write %s\n", options.bitstreamPath.c_str());
        return 1;
    }

    uint64_t bytesOut = 0;
    uint64_t keyframes = 0;
    EncoderConfig config;
    config.width = options.width;
    config.height = options.height;
    config.srcWidth = options.width;
    config.srcHeight = options.height;
    config.fps = options.fps;
    config.bitrateBps = options.bitrateBps;
    config.onPacket = [&](const uint8_t* data, size_t size, uint64_t, bool keyframe) {
        bytesOut += size;
        if (keyframe) ++keyframes;
        if (bitstream != nullptr) std::fwrite(data, 1, size, bitstream);
    };

    std::unique_ptr<IVideoEncoder> encoder =
        CreateEncoder(gpu.device.Get(), config, options.backend);
    if (!encoder) {
        if (bitstream != nullptr) std::fclose(bitstream);
        std::fprintf(stderr,
            "encbench: %s would not start on this machine, so there is no row to print - a "
            "backend that is absent must be missing from the table, never measured under "
            "another backend's name\n",
            options.backend.c_str());
        return 1;
    }

    const size_t totalFrames = clipFrames * options.loops;
    const size_t frameBytes = size_t(options.width) * options.height * kBytesPerPixel;
    const uint64_t frameIntervalUs = 1'000'000ull / options.fps;

    std::vector<uint64_t> encodeUs;
    encodeUs.reserve(totalFrames);
    const uint64_t cpuStartUs = ProcessCpuUs();
    const uint64_t wallStartUs = NowUs();

    for (size_t frame = 0; frame < totalFrames; ++frame) {
        const uint8_t* pixels = clip.data() + (frame % clipFrames) * frameBytes;
        ID3D11Texture2D* texture =
            ring.Upload(gpu.context.Get(), pixels, options.width, options.height);
        if (texture == nullptr) {
            std::fprintf(stderr, "encbench: frame %zu could not be uploaded\n", frame);
            break;
        }
        const uint64_t startedUs = NowUs();
        const bool encoded = encoder->Encode(texture, frame * frameIntervalUs, frame == 0);
        encodeUs.push_back(NowUs() - startedUs);
        if (!encoded) {
            std::fprintf(stderr, "encbench: the encoder refused frame %zu\n", frame);
            break;
        }
        const uint64_t dueUs = wallStartUs + (frame + 1) * frameIntervalUs;
        const uint64_t nowUs = NowUs();
        if (nowUs < dueUs) SleepUs(dueUs - nowUs);
    }
    encoder->Finish();

    const uint64_t wallUs = NowUs() - wallStartUs;
    const uint64_t cpuUs = ProcessCpuUs() - cpuStartUs;
    if (bitstream != nullptr) std::fclose(bitstream);

    std::vector<uint64_t> sorted = encodeUs;
    std::sort(sorted.begin(), sorted.end());
    const deskhub::media::EncoderRecoveryCaps caps = encoder->RecoveryCaps();
    const std::string_view id = encoder->BackendId();

    std::printf("%.*s,%s,%u,%u,%u,%u,%zu,%llu,%llu,%llu,%.1f,%.1f,%llu,%llu,%d,%d\n",
        int(id.size()), id.data(), options.backend.c_str(), options.width, options.height,
        options.fps, options.bitrateBps, sorted.size(),
        static_cast<unsigned long long>(PercentileUs(sorted, 0.50)),
        static_cast<unsigned long long>(PercentileUs(sorted, 0.99)),
        static_cast<unsigned long long>(sorted.empty() ? 0 : sorted.back()),
        wallUs ? 100.0 * double(cpuUs) / double(wallUs) : 0.0, double(wallUs) / 1000.0,
        static_cast<unsigned long long>(bytesOut), static_cast<unsigned long long>(keyframes),
        caps.longTermReference ? 1 : 0, caps.intraRefresh ? 1 : 0);
    return 0;
}
