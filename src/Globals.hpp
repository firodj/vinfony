#pragma once

#include <functional>

#include "hscpp/module/GlobalUserData.h"
#include "hscpp/mem/Ref.h"
#include "hscpp/mem/MemoryManager.h"

struct ImGuiContext;
struct tsf;

class ImKosongg;

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
    tsf *pTinySoundFont{nullptr};
    hscpp::mem::UniqueRef<MainWidget> pMainWidget;
    hscpp::mem::UniqueRef<ImKosongg> pImKosongg;

    std::function<std::string(const char*, const char*)> getResourcePath;

    float toolbarSize{50};
	float menuBarHeight{0};
    bool showHotswapStatus{true};
    bool showSoundFont{false};
    bool showPianoRoll{false};
};

};