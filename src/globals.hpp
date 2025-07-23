#pragma once

#include <functional>
#include <memory>

#ifdef _USE_HSCPP_
#include "hscpp/module/GlobalUserData.h"
#include "hscpp/mem/Ref.h"
#include "hscpp/mem/MemoryManager.h"
#endif

struct HsCppProgress;
struct ImGuiContext;
struct tsf;

// replace from: class ImKosongg;
namespace kosongg {
class ImControl;
}


namespace vinfony {

class MainWidget;
class IDawSeq;

class Globals
{
public:
    static Globals* GetInstance();
    static Globals* Resolve() {
#ifdef _USE_HSCPP_
		return hscpp::GlobalUserData::GetAs<Globals>();
#else
		return m_g;
#endif
    }

    ImGuiContext* pImGuiContext{nullptr};

#ifdef _USE_HSCPP_
	std::unique_ptr<HsCppProgress> pHsCppProgress;

	hscpp::mem::MemoryManager *pMemoryManager{nullptr};
	hscpp::mem::UniqueRef<MainWidget> pMainWidget;
	hscpp::mem::UniqueRef<kosongg::ImControl> pImControl;
#else
	std::unique_ptr<MainWidget> pMainWidget;
	std::unique_ptr<kosongg::ImControl> pImControl;
#endif

    IDawSeq *sequencer{nullptr};
    tsf *pTinySoundFont{nullptr};

    // hscpp::mem::UniqueRef<CustomControls> pCustomControls;
    std::function<std::string(const char*, const char*)> getResourcePath;

    bool showHsCppProgress{true};
    float toolbarSize{50};
	float menuBarHeight{0};
    bool showHotswapStatus{true};
    bool showSoundFont{false};
    bool showPianoRoll{false};

#ifndef _USE_HSCPP_
	static void SetGlobalUserData(Globals *g) { m_g = g; }
protected:
	static Globals *m_g;
#endif
};

};