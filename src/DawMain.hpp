#pragma once

#include <memory>
#include <string>
#include <functional>

struct DawProp;
struct DawTrack;
struct DawPropDrawParam {
  DawProp * self;
  DawTrack * track;
  int r,c;
};

using DawPropDrawFunc = std::function<void(DawPropDrawParam * param)>;

struct DawProp {
  int         id;
  std::string name;
  int         w;
  DawPropDrawFunc DrawTrack{};
};

struct DawTrack {
  int id;
  std::string name;
  int h;
};

class DawMain {
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

public:
    DawMain();
    ~DawMain();

    void Begin();
    void End();
    int NewProp(std::string name, DawPropDrawFunc func);
    int NewTrack(std::string name);
};
