#include "MainApp.hpp"
#include <thread>
#include <mutex>
#include <iostream>
#include "imgui.h"
#include "imgui_internal.h"
#include "DawMain.hpp"

#include <kosongg/INIReader.h>

static std::unique_ptr<MainApp> g_mainapp;

static std::mutex g_mtxMainapp;

struct MainApp::Impl {
  /* private implementations */
  std::unique_ptr<DawMain> dawMain;
};

MainApp *MainApp::GetInstance(/* dependency */) {
  std::lock_guard<std::mutex> lock(g_mtxMainapp);
  if (g_mainapp == nullptr) {
    struct MkUniqEnablr : public MainApp {};
    g_mainapp = std::make_unique<MkUniqEnablr>(/* dependency */);
  }
  return g_mainapp.get();
}

MainApp::MainApp(/* dependency */): kosongg::EngineBase(/* dependency */) {
  m_impl = std::make_unique<Impl>();
  m_windowTitle = "vinfony";
}

MainApp::~MainApp() {
}

void MainApp::RunImGui() {
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  ImColor clearColor(m_clearColor);

  // 1. Show the big demo window and another window
  EngineBase::RunImGui();

  // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
  if (ImGui::BeginMainMenuBar())
  {
    if (ImGui::BeginMenu("File"))
    {
      if (ImGui::MenuItem("New", "CTRL+N")) {}
      ImGui::Separator();
      if (ImGui::MenuItem("Exit", "CTRL+W")) {}
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window"))
    {
        ImGui::MenuItem("Demo Window",    __null, &m_showDemoWindow);
        ImGui::MenuItem("Another Window", __null, &m_showAnotherWindow);
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

  static int counter = 0;

  if (ImGui::Begin("Welcome to vinfony!")) {      // Create a window called "Hello, world!" and append into it.
    // Other Things
    ImGui::ColorEdit3("clear color", (float*)&clearColor.Value); // Edit 3 floats representing a color

    if (ImGui::Button("Button"))                           // Buttons return true when clicked (most widgets return true when edited/activated)
        counter++;
    ImGui::SameLine();
    ImGui::Text("counter = %d", counter);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

    m_impl->dawMain->Begin();
    m_impl->dawMain->End();
  }
  ImGui::End();

}

void MainApp::Init() {
  ReadIniConfig();
  EngineBase::Init();
  m_impl->dawMain = std::make_unique<DawMain>();
}

void MainApp::ReadIniConfig() {
  const char * homepath = nullptr;
#ifdef _WIN32
  homepath = std::getenv("USERPROFILE");
#else
  homepath = std::getenv("HOME");
#endif

  std::cout << "Home:" << homepath << std::endl;

  const char * inifilename = "vinfony.ini";
  INIReader reader(inifilename);

  if (reader.ParseError() < 0) {
    std::cout << "Can't load:" << inifilename << std::endl;
    return;
  }

  std::set<std::string> sections = reader.Sections();
  for (std::set<std::string>::iterator it = sections.begin(); it != sections.end(); ++it)
    std::cout << "Section:" << *it << std::endl;
}