#include "MainApp.hpp"
#include <thread>
#include <mutex>
#include <iostream>
#include "imgui.h"
#include <kosongg/INIReader.h>

static std::unique_ptr<MainApp> g_mainapp;

static std::mutex g_mtxMainapp;

struct MainApp::Impl {
  /* private implementations */
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
  {
    static float f = 0.0f;
    static int counter = 0;

    ImGui::Begin("Welcome to vinfony!");        // Create a window called "Hello, world!" and append into it.

    ImGui::Text("This is some useful text.");              // Display some text (you can use a format strings too)
    ImGui::Checkbox("Demo Window", &m_showDemoWindow);      // Edit bools storing our window open/close state
    ImGui::Checkbox("Another Window", &m_showAnotherWindow);

    ImGui::SliderFloat("float", &f, 0.0f, 1.0f);           // Edit 1 float using a slider from 0.0f to 1.0f
    ImGui::ColorEdit3("clear color", (float*)&clearColor.Value); // Edit 3 floats representing a color

    if (ImGui::Button("Button"))                           // Buttons return true when clicked (most widgets return true when edited/activated)
        counter++;
    ImGui::SameLine();
    ImGui::Text("counter = %d", counter);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::End();
  }
}

void MainApp::Init() {
  ReadIniConfig();
  EngineBase::Init();
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