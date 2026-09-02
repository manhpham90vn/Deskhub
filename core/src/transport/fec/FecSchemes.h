#pragma once
#include "deskhub/transport/FecScheme.h"

#include <memory>

namespace deskhub::fec {

std::unique_ptr<FecScheme> MakeXor();

std::unique_ptr<FecScheme> MakeReedSolomon();

}
