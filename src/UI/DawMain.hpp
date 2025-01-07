#pragma once

#include <memory>
#include <string>
#include <functional>

#include "hscpp/module/PreprocessorMacros.h" // Added so macros are available when using a tracked class.

#include "DawDisplay.hpp"
#include "../IDawSeq.hpp"
#include "../IDawTrack.hpp"

namespace vinfony {
  struct DawProp;
  class DawTrack;

  struct DawPropDrawParam {
    DawProp * self;
    IDawTrack * track;
    int r,c;
  };

  using DawPropDrawFunc = std::function<void(DawPropDrawParam * param, IDawSeq * seq)>;

  struct DawProp {
    int         id;
    std::string name;
    int         w;
    DawPropDrawFunc DrawProp{};
  };

  void DawMain(const char *label, IDawSeq * seq);
};
