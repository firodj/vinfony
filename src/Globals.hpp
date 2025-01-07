#pragma once

#include "hscpp/module/GlobalUserData.h"
#include "hscpp/mem/Ref.h"

struct ImGuiContext;

namespace vinfony {

class MainWidget;
class IDawSeq;

class Globals
{
public:
    static Globals* GetInstance();
    static Globals* Resolve() {
        return hscpp::GlobalUserData::GetAs<Globals>();
    }

    ImGuiContext* pImGuiContext{nullptr};
    hscpp::mem::UniqueRef<MainWidget> pMainWidget;
    IDawSeq *sequencer{nullptr};

    float toolbarSize{50};
	float menuBarHeight{0};
};

};