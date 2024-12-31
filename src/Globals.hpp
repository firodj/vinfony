#pragma once

struct ImGuiContext;

namespace vinfony {

class Globals
{
public:
    static Globals* GetInstance();
    static Globals* Resolve();

    ImGuiContext* pImGuiContext;
};

};