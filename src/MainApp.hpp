#pragma once

#include <memory>
#include <kosongg/Engine.h>

#if defined(__APPLE__) && defined(BUILD_APPLE_BUNDLE)
std::string GetBundleResourcePath(const char * path);
#endif

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
    void Clean() override;
    std::string GetResourcePath(const char *path, const char *file) override;
    void StdAudioCallback(uint8_t *stream, int len);
};
