#pragma once

#include <memory>
#include <string>
#include <functional>

#include "DawDisplay.hpp"
#include "DawSeq.hpp"

namespace vinfony {
  struct DawProp;
  struct DawTrack;

  struct DawPropDrawParam {
    DawProp * self;
    DawTrack * track;
    int r,c;
  };

  using DawPropDrawFunc = std::function<void(DawPropDrawParam * param)>;
  using SplitterOnDraggingFunc = std::function<void()>;

  struct DawProp {
    int         id;
    std::string name;
    int         w;
    DawPropDrawFunc DrawTrack{};
  };

  void DawMain(const char *label, DawSeq * seq);
};
