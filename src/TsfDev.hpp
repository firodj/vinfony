#pragma once

#include "BaseMidiOut.hpp"
#include <memory>

namespace vinfony
{
  std::unique_ptr<BaseMidiOutDevice> CreateTsfDev();
};
