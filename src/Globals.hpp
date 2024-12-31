#pragma once

struct ImGuiContext;

namespace vinfony {

struct MainWidget;

class Globals
{
public:
    Globals();

    static Globals* GetInstance();
    static Globals* Resolve();

    ImGuiContext* pImGuiContext{nullptr};
    MainWidget * pMainWidget{nullptr};


    float toolbarSize{50};
	float menuBarHeight{0};
};

};