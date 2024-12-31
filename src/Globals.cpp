#include "Globals.hpp"

#include <hscpp/Hotswapper.h>
#include <thread>

static std::unique_ptr<vinfony::Globals> g_globals;
static std::mutex g_mtxGlobals;

namespace vinfony {

Globals::Globals() { }

Globals* Globals::GetInstance() {
    std::lock_guard<std::mutex> lock(g_mtxGlobals);
	if (g_globals == nullptr) {
		struct MkUniqEnablr : public Globals {};
		g_globals = std::make_unique<MkUniqEnablr>(/* dependency */);
	}
	return g_globals.get();
}

Globals* Globals::Resolve() {
    if (hscpp::GlobalUserData::IsNull())
    {
        return Globals::GetInstance();
    } else {
        return hscpp::GlobalUserData::GetAs<Globals>();
    }
}

}