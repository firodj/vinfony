#pragma once

#include <memory>
#include <kosongg/Engine.h>

class MainApp: public kosongg::EngineBase {
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl{};

protected:
    MainApp(/* dependency */);
    void RunImGui() override;
    void ReadIniConfig();
    void DockSpaceUI();
    void ToolbarUI();

public:
    MainApp(MainApp &other) = delete;
    ~MainApp();
    void operator=(const MainApp &) = delete;

    static MainApp *GetInstance(/* dependency */);

    void Init() override;
};