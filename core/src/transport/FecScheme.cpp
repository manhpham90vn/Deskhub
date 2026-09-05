#include "deskhub/transport/FecScheme.h"

#include "fec/FecSchemes.h"

namespace deskhub {
namespace {

constexpr std::string_view kNames[] = {"xor", "rs"};

}

std::unique_ptr<FecScheme> MakeFecScheme(std::string_view name) {
    if (name == "xor") return fec::MakeXor();
    if (name == "rs") return fec::MakeReedSolomon();
    return nullptr;
}

std::span<const std::string_view> FecSchemeNames() {
    return kNames;
}

bool IsFecSchemeName(std::string_view name) {
    for (std::string_view known : kNames)
        if (known == name) return true;
    return false;
}

}
