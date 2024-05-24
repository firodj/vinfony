#pragma once

#include "BaseMidiOut.hpp"
#include <memory>
#include <string>

namespace vinfony
{
  std::unique_ptr<BaseMidiOutDevice> CreateTsfDev(std::string soundfontPath);
};
