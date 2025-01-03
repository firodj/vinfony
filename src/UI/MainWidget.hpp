#pragma once

#include "hscpp/module/Tracker.h"

namespace vinfony {

class MainWidget {

    HSCPP_TRACK(MainWidget, "MainWidget");

public:
    hscpp_virtual ~MainWidget();
    MainWidget();
    hscpp_virtual void Update();
    void Creating();
    void Destroying();

    bool m_showDemoWindow;
    bool m_showToolMetrics;
    bool m_showToolDebugLog;
    bool m_showToolAbout;
    bool m_showSoundFont;

protected:
    void DockSpaceUI();
    void ToolbarUI();
    void DemoUI();
    void MainMenuUI();
};

};
