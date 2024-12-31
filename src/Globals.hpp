#pragma once

#include "hscpp/module/GlobalUserData.h"

struct ImGuiContext;

namespace vinfony {

class MainWidget;

class Globals
{
public:
    static Globals* GetInstance();
    static Globals* Resolve() {
        return hscpp::GlobalUserData::GetAs<Globals>();
    }

    ImGuiContext* pImGuiContext{nullptr};
    MainWidget * pMainWidget{nullptr};

    float toolbarSize{50};
	float menuBarHeight{0};
};

};