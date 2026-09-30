#include "deskhub/qr/QrCode.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <span>

namespace deskhub {

namespace {

constexpr int kVersionTableSize = kQrMaxVersion + 1;

constexpr std::array<uint8_t, kVersionTableSize> kEccCodewordsPerBlockLevelM = {
    0, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26,
    26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28};

constexpr std::array<uint8_t, kVersionTableSize> kEccBlocksLevelM = {
    0, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5, 5, 8, 9, 9, 10, 10, 11, 13, 14, 16,
    17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49};

constexpr int kModulesPerVersionStep = 4;
constexpr int kVersionOneSize = 21;
constexpr int kBitsPerCodeword = 8;

constexpr int kModeIndicatorBits = 4;
constexpr uint32_t kByteModeIndicator = 0x4;
constexpr int kShortCharCountBits = 8;
constexpr int kLongCharCountBits = 16;
constexpr int kLastVersionWithShortCharCount = 9;
constexpr int kMaxTerminatorBits = 4;
constexpr std::array<uint8_t, 2> kPadCodewords = {0xEC, 0x11};

constexpr int kFinderSize = 7;
constexpr int kFinderWithSeparator = 8;
constexpr int kTimingIndex = 6;
constexpr int kAlignmentRadius = 2;
constexpr int kFormatAreaLength = 9;
constexpr int kFirstVersionWithVersionInfo = 7;
constexpr int kVersionInfoLength = 6;
constexpr int kVersionInfoWidth = 3;
constexpr int kVersionInfoOffset = 11;
constexpr int kAlignmentStepVersion32 = 26;
constexpr int kAlignmentStepFixedVersion = 32;

constexpr int kMaskBitsInFormat = 3;
constexpr int kFormatEccBits = 10;
constexpr uint32_t kFormatGeneratorPolynomial = 0x537;
constexpr uint32_t kFormatXorMask = 0x5412;
constexpr uint32_t kLevelMFormatBits = 0;
constexpr int kFormatBitCount = 15;
constexpr int kFormatBitsBesideTopRightFinder = 8;

struct ModulePosition {
    int x;
    int y;
};

constexpr std::array<ModulePosition, kFormatBitCount> kFormatBitPositionsTopLeft = {
    ModulePosition{8, 0}, ModulePosition{8, 1}, ModulePosition{8, 2}, ModulePosition{8, 3},
    ModulePosition{8, 4}, ModulePosition{8, 5}, ModulePosition{8, 7}, ModulePosition{8, 8},
    ModulePosition{7, 8}, ModulePosition{5, 8}, ModulePosition{4, 8}, ModulePosition{3, 8},
    ModulePosition{2, 8}, ModulePosition{1, 8}, ModulePosition{0, 8}};

constexpr int kVersionEccBits = 12;
constexpr uint32_t kVersionGeneratorPolynomial = 0x1F25;
constexpr int kVersionBitCount = 18;

constexpr int kMaskCount = 8;
constexpr uint32_t kGf256Modulus = 0x11D;
constexpr int kGf256HighBit = 7;

constexpr int kMinPenalizedRun = 5;
constexpr int kRunPenaltyBase = 3;
constexpr int kBlockPenalty = 3;
constexpr int kFinderLikePenalty = 40;
constexpr int kBalancePenaltyStep = 10;
constexpr int kFinderLikeLightMargin = 4;
constexpr std::array<uint8_t, 7> kFinderLikeRun = {1, 0, 1, 1, 1, 0, 1};

constexpr int kQuietZone = 1;
constexpr std::string_view kUpperHalfBlock = "▀";
constexpr std::string_view kLowerHalfBlock = "▄";
constexpr std::string_view kFullBlock = "█";
constexpr std::string_view kEmptyBlock = " ";

int SymbolSize(int version) {
    return kVersionOneSize + (version - kQrMinVersion) * kModulesPerVersionStep;
}

int AlignmentPatternCount(int version) {
    if (version == kQrMinVersion) return 0;
    return version / 7 + 2;
}

int RawDataModules(int version) {
    const int size = SymbolSize(version);
    int modules = size * size;
    modules -= 3 * kFinderWithSeparator * kFinderWithSeparator;
    modules -= 2 * (size - 2 * kFinderWithSeparator);
    modules -= 2 * kFormatBitCount + 1;
    const int alignCount = AlignmentPatternCount(version);
    if (alignCount > 0) {
        const int alignSide = 2 * kAlignmentRadius + 1;
        modules -= (alignCount * alignCount - 3) * alignSide * alignSide;
        modules += (alignCount - 2) * 2 * alignSide;
    }
    if (version >= kFirstVersionWithVersionInfo) modules -= 2 * kVersionBitCount;
    return modules;
}

int TotalCodewords(int version) {
    return RawDataModules(version) / kBitsPerCodeword;
}

int EccCodewordsPerBlock(int version) {
    return kEccCodewordsPerBlockLevelM[size_t(version)];
}

int EccBlockCount(int version) {
    return kEccBlocksLevelM[size_t(version)];
}

int EccCodewords(int version) {
    return EccCodewordsPerBlock(version) * EccBlockCount(version);
}

int DataCodewords(int version) {
    return TotalCodewords(version) - EccCodewords(version);
}

int CharCountBits(int version) {
    return version <= kLastVersionWithShortCharCount ? kShortCharCountBits : kLongCharCountBits;
}

size_t DataCapacityBytes(int version) {
    const int headerBits = kModeIndicatorBits + CharCountBits(version);
    return size_t((DataCodewords(version) * kBitsPerCodeword - headerBits) / kBitsPerCodeword);
}

std::optional<int> ChooseVersion(size_t byteCount) {
    for (int version = kQrMinVersion; version <= kQrMaxVersion; ++version) {
        if (byteCount <= DataCapacityBytes(version)) return version;
    }
    return std::nullopt;
}

class BitWriter {
public:
    void Append(uint32_t value, int bits) {
        for (int i = bits - 1; i >= 0; --i) AppendBit(((value >> i) & 1u) != 0);
    }

    void AppendBit(bool one) {
        if (bitCount_ % kBitsPerCodeword == 0) bytes_.push_back(0);
        if (one) bytes_.back() |= uint8_t(0x80u >> (bitCount_ % kBitsPerCodeword));
        ++bitCount_;
    }

    size_t BitCount() const {
        return bitCount_;
    }

    std::vector<uint8_t> TakeBytes() {
        return std::move(bytes_);
    }

private:
    std::vector<uint8_t> bytes_{};
    size_t bitCount_ = 0;
};

std::vector<uint8_t> AppendDataCodewords(std::string_view text, int version) {
    const size_t capacityBits = size_t(DataCodewords(version)) * kBitsPerCodeword;
    BitWriter writer;
    writer.Append(kByteModeIndicator, kModeIndicatorBits);
    writer.Append(uint32_t(text.size()), CharCountBits(version));
    for (const char c : text) writer.Append(uint8_t(c), kBitsPerCodeword);
    const size_t terminatorBits = std::min(size_t(kMaxTerminatorBits), capacityBits - writer.BitCount());
    writer.Append(0, int(terminatorBits));
    while (writer.BitCount() % kBitsPerCodeword != 0) writer.AppendBit(false);
    size_t padIndex = 0;
    while (writer.BitCount() < capacityBits) {
        writer.Append(kPadCodewords[padIndex], kBitsPerCodeword);
        padIndex = (padIndex + 1) % kPadCodewords.size();
    }
    return writer.TakeBytes();
}

uint8_t GfMultiply(uint8_t a, uint8_t b) {
    uint32_t product = 0;
    for (int i = kGf256HighBit; i >= 0; --i) {
        product = (product << 1) ^ ((product >> kGf256HighBit) * kGf256Modulus);
        product ^= ((b >> i) & 1u) * a;
    }
    return uint8_t(product);
}

std::vector<uint8_t> ReedSolomonGenerator(int degree) {
    std::vector<uint8_t> generator(size_t(degree), 0);
    generator.back() = 1;
    uint8_t root = 1;
    for (int i = 0; i < degree; ++i) {
        for (size_t j = 0; j < generator.size(); ++j) {
            generator[j] = GfMultiply(generator[j], root);
            if (j + 1 < generator.size()) generator[j] ^= generator[j + 1];
        }
        root = GfMultiply(root, 2);
    }
    return generator;
}

std::vector<uint8_t> ReedSolomonRemainder(std::span<const uint8_t> data, std::span<const uint8_t> generator) {
    std::vector<uint8_t> remainder(generator.size(), 0);
    for (const uint8_t byte : data) {
        const uint8_t factor = byte ^ remainder.front();
        remainder.erase(remainder.begin());
        remainder.push_back(0);
        for (size_t i = 0; i < remainder.size(); ++i) remainder[i] ^= GfMultiply(generator[i], factor);
    }
    return remainder;
}

std::vector<uint8_t> InterleaveBlocks(std::span<const uint8_t> data, int version) {
    const int blockCount = EccBlockCount(version);
    const int eccLength = EccCodewordsPerBlock(version);
    const int totalCodewords = TotalCodewords(version);
    const int shortBlockCount = blockCount - totalCodewords % blockCount;
    const int shortBlockLength = totalCodewords / blockCount;
    const int shortDataLength = shortBlockLength - eccLength;
    const auto generator = ReedSolomonGenerator(eccLength);

    std::vector<std::vector<uint8_t>> blocks;
    size_t consumed = 0;
    for (int i = 0; i < blockCount; ++i) {
        const int extraCodeword = i < shortBlockCount ? 0 : 1;
        const size_t dataLength = size_t(shortDataLength) + size_t(extraCodeword);
        const auto blockData = data.subspan(consumed, dataLength);
        std::vector<uint8_t> block(blockData.begin(), blockData.end());
        consumed += dataLength;
        const auto ecc = ReedSolomonRemainder(block, generator);
        if (i < shortBlockCount) block.push_back(0);
        block.insert(block.end(), ecc.begin(), ecc.end());
        blocks.push_back(std::move(block));
    }

    std::vector<uint8_t> result;
    result.reserve(size_t(totalCodewords));
    for (size_t i = 0; i < blocks.front().size(); ++i) {
        for (int j = 0; j < blockCount; ++j) {
            const bool paddingSlot = int(i) == shortDataLength && j < shortBlockCount;
            if (!paddingSlot) result.push_back(blocks[size_t(j)][i]);
        }
    }
    return result;
}

class Canvas {
public:
    explicit Canvas(int size)
        : size_(size), modules_(size_t(size) * size_t(size), 0), function_(size_t(size) * size_t(size), 0) {}

    int Size() const {
        return size_;
    }

    bool Dark(int x, int y) const {
        return modules_[Index(x, y)] != 0;
    }

    bool IsFunction(int x, int y) const {
        return function_[Index(x, y)] != 0;
    }

    void Paint(int x, int y, bool dark) {
        modules_[Index(x, y)] = dark ? 1 : 0;
    }

    void PaintFunction(int x, int y, bool dark) {
        Paint(x, y, dark);
        function_[Index(x, y)] = 1;
    }

    void Flip(int x, int y) {
        modules_[Index(x, y)] ^= 1;
    }

    std::vector<uint8_t> TakeModules() {
        return std::move(modules_);
    }

private:
    size_t Index(int x, int y) const {
        return size_t(y) * size_t(size_) + size_t(x);
    }

    int size_;
    std::vector<uint8_t> modules_;
    std::vector<uint8_t> function_;
};

void PlaceFinderPattern(Canvas& canvas, int centerX, int centerY) {
    const int reach = kFinderSize / 2 + 1;
    for (int dy = -reach; dy <= reach; ++dy) {
        for (int dx = -reach; dx <= reach; ++dx) {
            const int x = centerX + dx;
            const int y = centerY + dy;
            if (x < 0 || y < 0 || x >= canvas.Size() || y >= canvas.Size()) continue;
            const int distance = std::max(std::abs(dx), std::abs(dy));
            const bool dark = distance != 2 && distance != reach;
            canvas.PaintFunction(x, y, dark);
        }
    }
}

void PlaceAlignmentPattern(Canvas& canvas, int centerX, int centerY) {
    for (int dy = -kAlignmentRadius; dy <= kAlignmentRadius; ++dy) {
        for (int dx = -kAlignmentRadius; dx <= kAlignmentRadius; ++dx) {
            const bool dark = std::max(std::abs(dx), std::abs(dy)) != 1;
            canvas.PaintFunction(centerX + dx, centerY + dy, dark);
        }
    }
}

std::vector<int> AlignmentPositions(int version) {
    const int count = AlignmentPatternCount(version);
    if (count == 0) return {};
    const int size = SymbolSize(version);
    const int step = version == kAlignmentStepFixedVersion
                         ? kAlignmentStepVersion32
                         : (version * kModulesPerVersionStep + count * 2 + 1) / (count * 2 - 2) * 2;
    std::vector<int> positions(size_t(count), 0);
    positions.front() = kTimingIndex;
    int position = size - kFinderSize;
    for (int i = count - 1; i >= 1; --i, position -= step) positions[size_t(i)] = position;
    return positions;
}

void PlaceAlignmentPatterns(Canvas& canvas, int version) {
    const auto positions = AlignmentPositions(version);
    const size_t last = positions.empty() ? 0 : positions.size() - 1;
    for (size_t i = 0; i < positions.size(); ++i) {
        for (size_t j = 0; j < positions.size(); ++j) {
            const bool overlapsFinder = (i == 0 && j == 0) || (i == 0 && j == last) || (i == last && j == 0);
            if (!overlapsFinder) PlaceAlignmentPattern(canvas, positions[i], positions[j]);
        }
    }
}

void PlaceTimingPatterns(Canvas& canvas) {
    for (int i = 0; i < canvas.Size(); ++i) {
        const bool dark = i % 2 == 0;
        canvas.PaintFunction(kTimingIndex, i, dark);
        canvas.PaintFunction(i, kTimingIndex, dark);
    }
}

void ReserveFormatArea(Canvas& canvas) {
    const int size = canvas.Size();
    for (int i = 0; i < kFormatAreaLength; ++i) {
        if (i == kTimingIndex) continue;
        canvas.PaintFunction(kFinderWithSeparator, i, false);
        canvas.PaintFunction(i, kFinderWithSeparator, false);
    }
    for (int i = 0; i < kFinderWithSeparator; ++i) {
        canvas.PaintFunction(size - 1 - i, kFinderWithSeparator, false);
        canvas.PaintFunction(kFinderWithSeparator, size - 1 - i, false);
    }
}

void ReserveVersionArea(Canvas& canvas, int version) {
    if (version < kFirstVersionWithVersionInfo) return;
    const int size = canvas.Size();
    for (int i = 0; i < kVersionInfoLength; ++i) {
        for (int j = 0; j < kVersionInfoWidth; ++j) {
            canvas.PaintFunction(size - kVersionInfoOffset + j, i, false);
            canvas.PaintFunction(i, size - kVersionInfoOffset + j, false);
        }
    }
}

void PlaceFunctionPatterns(Canvas& canvas, int version) {
    const int size = canvas.Size();
    PlaceTimingPatterns(canvas);
    const int finderCenter = kFinderSize / 2;
    PlaceFinderPattern(canvas, finderCenter, finderCenter);
    PlaceFinderPattern(canvas, size - 1 - finderCenter, finderCenter);
    PlaceFinderPattern(canvas, finderCenter, size - 1 - finderCenter);
    PlaceAlignmentPatterns(canvas, version);
    ReserveFormatArea(canvas);
    ReserveVersionArea(canvas, version);
}

void PlaceData(Canvas& canvas, std::span<const uint8_t> codewords) {
    const int size = canvas.Size();
    const size_t totalBits = codewords.size() * kBitsPerCodeword;
    size_t bit = 0;
    for (int right = size - 1; right >= 1; right -= 2) {
        if (right == kTimingIndex) right = kTimingIndex - 1;
        const bool upward = ((right + 1) & 2) == 0;
        for (int step = 0; step < size; ++step) {
            const int y = upward ? size - 1 - step : step;
            for (int x = right; x >= right - 1; --x) {
                if (canvas.IsFunction(x, y) || bit >= totalBits) continue;
                const bool dark = ((codewords[bit / kBitsPerCodeword] >> (7 - bit % kBitsPerCodeword)) & 1u) != 0;
                canvas.Paint(x, y, dark);
                ++bit;
            }
        }
    }
}

bool MaskBit(int mask, int x, int y) {
    switch (mask) {
        case 0: return (x + y) % 2 == 0;
        case 1: return y % 2 == 0;
        case 2: return x % 3 == 0;
        case 3: return (x + y) % 3 == 0;
        case 4: return (x / 3 + y / 2) % 2 == 0;
        case 5: return x * y % 2 + x * y % 3 == 0;
        case 6: return (x * y % 2 + x * y % 3) % 2 == 0;
        default: return ((x + y) % 2 + x * y % 3) % 2 == 0;
    }
}

void ApplyMask(Canvas& canvas, int mask) {
    for (int y = 0; y < canvas.Size(); ++y) {
        for (int x = 0; x < canvas.Size(); ++x) {
            if (!canvas.IsFunction(x, y) && MaskBit(mask, x, y)) canvas.Flip(x, y);
        }
    }
}

int RunPenalty(std::span<const uint8_t> line) {
    int penalty = 0;
    size_t runStart = 0;
    for (size_t i = 1; i <= line.size(); ++i) {
        if (i < line.size() && line[i] == line[runStart]) continue;
        const int runLength = int(i - runStart);
        if (runLength >= kMinPenalizedRun) penalty += kRunPenaltyBase + runLength - kMinPenalizedRun;
        runStart = i;
    }
    return penalty;
}

bool AllLight(std::span<const uint8_t> line, int from, int to) {
    const int begin = std::max(from, 0);
    const int end = std::min(to, int(line.size()));
    for (int i = begin; i < end; ++i) {
        if (line[size_t(i)] != 0) return false;
    }
    return true;
}

int FindFinderLikeRun(std::span<const uint8_t> line, int from) {
    const int runLength = int(kFinderLikeRun.size());
    for (int start = from; start + runLength <= int(line.size()); ++start) {
        if (std::equal(kFinderLikeRun.begin(), kFinderLikeRun.end(), line.begin() + start)) return start;
    }
    return -1;
}

int FinderLikePenalty(std::span<const uint8_t> line) {
    const int runLength = int(kFinderLikeRun.size());
    const int lastStart = int(line.size()) - runLength;
    int penalty = 0;
    int start = FindFinderLikeRun(line, 0);
    while (start >= 0) {
        const int end = start + runLength;
        const bool lightBefore = AllLight(line, start - kFinderLikeLightMargin, start);
        const bool lightAfter = AllLight(line, end, end + kFinderLikeLightMargin);
        int next = end;
        if (start == 0 || start == lastStart || lightBefore || lightAfter) {
            penalty += kFinderLikePenalty;
        } else {
            next = start + kFinderLikeLightMargin;
        }
        start = FindFinderLikeRun(line, next);
    }
    return penalty;
}

int BlockPenalty(const Canvas& canvas) {
    int penalty = 0;
    for (int y = 1; y < canvas.Size(); ++y) {
        for (int x = 1; x < canvas.Size(); ++x) {
            const bool dark = canvas.Dark(x, y);
            const bool uniform = canvas.Dark(x - 1, y) == dark && canvas.Dark(x, y - 1) == dark &&
                                 canvas.Dark(x - 1, y - 1) == dark;
            if (uniform) penalty += kBlockPenalty;
        }
    }
    return penalty;
}

int BalancePenalty(const Canvas& canvas) {
    int darkCount = 0;
    for (int y = 0; y < canvas.Size(); ++y) {
        for (int x = 0; x < canvas.Size(); ++x) darkCount += canvas.Dark(x, y) ? 1 : 0;
    }
    const int total = canvas.Size() * canvas.Size();
    const int deviationSteps = std::abs(darkCount * 20 - total * 10) / total;
    return deviationSteps * kBalancePenaltyStep;
}

int PenaltyScore(const Canvas& canvas) {
    const int size = canvas.Size();
    std::vector<uint8_t> row(size_t(size), 0);
    std::vector<uint8_t> column(size_t(size), 0);
    int penalty = 0;
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            row[size_t(j)] = canvas.Dark(j, i) ? 1 : 0;
            column[size_t(j)] = canvas.Dark(i, j) ? 1 : 0;
        }
        penalty += RunPenalty(row) + RunPenalty(column);
        penalty += FinderLikePenalty(row) + FinderLikePenalty(column);
    }
    return penalty + BlockPenalty(canvas) + BalancePenalty(canvas);
}

int ChooseMask(Canvas& canvas) {
    int bestMask = 0;
    int bestPenalty = 0;
    for (int mask = 0; mask < kMaskCount; ++mask) {
        ApplyMask(canvas, mask);
        const int penalty = PenaltyScore(canvas);
        ApplyMask(canvas, mask);
        if (mask == 0 || penalty < bestPenalty) {
            bestMask = mask;
            bestPenalty = penalty;
        }
    }
    ApplyMask(canvas, bestMask);
    return bestMask;
}

uint32_t BchRemainder(uint32_t value, int eccBits, uint32_t generator) {
    uint32_t remainder = value;
    for (int i = 0; i < eccBits; ++i) remainder = (remainder << 1) ^ ((remainder >> (eccBits - 1)) * generator);
    return remainder;
}

uint32_t FormatBits(int mask) {
    const uint32_t data = (kLevelMFormatBits << kMaskBitsInFormat) | uint32_t(mask);
    const uint32_t remainder = BchRemainder(data, kFormatEccBits, kFormatGeneratorPolynomial);
    return ((data << kFormatEccBits) | remainder) ^ kFormatXorMask;
}

void WriteFormatBits(Canvas& canvas, int mask) {
    const uint32_t bits = FormatBits(mask);
    const int size = canvas.Size();
    for (int i = 0; i < kFormatBitCount; ++i) {
        const bool dark = ((bits >> i) & 1u) != 0;
        const ModulePosition topLeft = kFormatBitPositionsTopLeft[size_t(i)];
        canvas.PaintFunction(topLeft.x, topLeft.y, dark);
        if (i < kFormatBitsBesideTopRightFinder) {
            canvas.PaintFunction(size - 1 - i, kFinderWithSeparator, dark);
        } else {
            canvas.PaintFunction(kFinderWithSeparator, size - kFormatBitCount + i, dark);
        }
    }
    canvas.PaintFunction(kFinderWithSeparator, size - kFinderWithSeparator, true);
}

void WriteVersionBits(Canvas& canvas, int version) {
    if (version < kFirstVersionWithVersionInfo) return;
    const uint32_t remainder = BchRemainder(uint32_t(version), kVersionEccBits, kVersionGeneratorPolynomial);
    const uint32_t bits = (uint32_t(version) << kVersionEccBits) | remainder;
    const int size = canvas.Size();
    for (int i = 0; i < kVersionBitCount; ++i) {
        const bool dark = ((bits >> i) & 1u) != 0;
        const int a = size - kVersionInfoOffset + i % kVersionInfoWidth;
        const int b = i / kVersionInfoWidth;
        canvas.PaintFunction(a, b, dark);
        canvas.PaintFunction(b, a, dark);
    }
}

void AppendBlock(std::string& out, bool upperDark, bool lowerDark) {
    if (upperDark && lowerDark) {
        out += kFullBlock;
    } else if (upperDark) {
        out += kUpperHalfBlock;
    } else if (lowerDark) {
        out += kLowerHalfBlock;
    } else {
        out += kEmptyBlock;
    }
}

}

bool QrCode::Dark(int x, int y) const {
    if (x < 0 || y < 0 || x >= size || y >= size) return false;
    const size_t index = size_t(y) * size_t(size) + size_t(x);
    return index < modules.size() && modules[index] != 0;
}

std::optional<QrCode> EncodeQr(std::string_view text) {
    if (text.empty()) return std::nullopt;
    const auto version = ChooseVersion(text.size());
    if (!version) return std::nullopt;

    const auto dataCodewords = AppendDataCodewords(text, *version);
    const auto codewords = InterleaveBlocks(dataCodewords, *version);

    Canvas canvas(SymbolSize(*version));
    PlaceFunctionPatterns(canvas, *version);
    PlaceData(canvas, codewords);
    const int mask = ChooseMask(canvas);
    WriteFormatBits(canvas, mask);
    WriteVersionBits(canvas, *version);

    QrCode code;
    code.size = canvas.Size();
    code.modules = canvas.TakeModules();
    return code;
}

std::string RenderQrText(const QrCode& code) {
    const int extent = code.size + 2 * kQuietZone;
    std::string out;
    for (int row = 0; row < extent; row += 2) {
        for (int column = 0; column < extent; ++column) {
            const int x = column - kQuietZone;
            const bool upperDark = code.Dark(x, row - kQuietZone);
            const bool lowerDark = code.Dark(x, row + 1 - kQuietZone);
            AppendBlock(out, upperDark, lowerDark);
        }
        out += '\n';
    }
    return out;
}

}
