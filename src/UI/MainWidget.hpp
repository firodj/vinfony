#pragma once

#include "hscpp/module/Tracker.h"
#include "hscpp/mem/Ref.h"

namespace vinfony {

class DawMainProject;
class DawSoundFont;

class MainWidget {

    HSCPP_TRACK(MainWidget, "MainWidget");

public:
    hscpp_virtual ~MainWidget();
    MainWidget();
    hscpp_virtual void Update();
    void Creating();
    void Destroying();

    hscpp::mem::UniqueRef<DawMainProject> m_pDawMainProject;
    hscpp::mem::UniqueRef<DawSoundFont>   m_pDawSoundFont;
protected:
    void DockSpaceUI();
    void ToolbarUI();
    void DemoUI();
    void MainMenuUI();

    bool m_showDemoWindow;
    bool m_showToolMetrics;
    bool m_showToolDebugLog;
    bool m_showToolAbout;
};

};
