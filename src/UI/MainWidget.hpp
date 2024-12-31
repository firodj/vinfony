#pragma once

#include "hscpp/module/Tracker.h"

namespace vinfony {

class MainWidget {

    HSCPP_TRACK(MainWidget, "MainWidget");

public:
    hscpp_virtual ~MainWidget();
    MainWidget();
    hscpp_virtual void Update();

protected:
    void DockSpaceUI();
    void ToolbarUI();
};

};
