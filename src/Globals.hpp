#pragma once

#include "hscpp/module/GlobalUserData.h"
#include "hscpp/mem/Ref.h"
#include "hscpp/mem/MemoryManager.h"

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
    hscpp::mem::MemoryManager *pMemoryManager{nullptr};

    IDawSeq *sequencer{nullptr};
    hscpp::mem::UniqueRef<MainWidget> pMainWidget;

    float toolbarSize{50};
	float menuBarHeight{0};


};

};