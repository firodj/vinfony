#pragma once

#include <memory>
#include <string>
#include <functional>

#include "../DawDisplay.hpp"
#include "../DawSeq.hpp"

namespace vinfony {
  struct DawProp;
  class DawTrack;

  struct DawPropDrawParam {
    DawProp * self;
    DawTrack * track;
    int r,c;
  };

  using DawPropDrawFunc = std::function<void(DawPropDrawParam * param, DawSeq * seq)>;

  struct DawProp {
    int         id;
    std::string name;
    int         w;
    DawPropDrawFunc DrawProp{};
  };

  void DawMain(const char *label, DawSeq * seq);
};
