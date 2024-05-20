#include "MainApp.hpp"
#include <thread>
#include <mutex>
#include <iostream>
#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui.h"
#include "imgui_internal.h"
#include "DawMain.hpp"
#include "DawSeq.hpp"

#include <kosongg/INIReader.h>
#include <ifd/ImFileDialog.hpp>
#include <ifd/ImFileDialog_opengl.hpp>
#include <kosongg/IconsFontAwesome6.h>
#include "kosongg/Component.h"
#include <fmt/core.h>
#include "circularfifo1.h"

static std::unique_ptr<MainApp> g_mainapp;

static std::mutex g_mtxMainapp;
struct MainApp::Impl {
  vinfony::DawSeq sequencer;
  float toolbarSize{50};
  float menuBarHeight{0};
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
  m_showDemoWindow = false;
}

MainApp::~MainApp() {
}

// https://github.com/tpecholt/imrad/blob/main/src/imrad.cpp
void MainApp::DockSpaceUI()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos + ImVec2(0, m_impl->toolbarSize));
	ImGui::SetNextWindowSize(viewport->Size - ImVec2(0, m_impl->toolbarSize));
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGuiWindowFlags window_flags = 0
		| ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("Master DockSpace", NULL, window_flags);
	ImGuiID dockspace_id = ImGui::GetID("MyDockspace");

	// Save off menu bar height for later.
	m_impl->menuBarHeight = ImGui::GetCurrentWindow()->MenuBarHeight();

	ImGui::DockSpace(dockspace_id);

  //ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_DockSpace);
	//ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);


  //ImGui::DockBuilderFinish(dockspace_id);

	ImGui::End();

	ImGui::PopStyleVar(3);
}

void MainApp::ToolbarUI()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + m_impl->menuBarHeight));
	ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, m_impl->toolbarSize));
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags window_flags = 0
		| ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoSavedSettings
		;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::Begin("TOOLBAR", NULL, window_flags);
	ImGui::PopStyleVar();

  const bool disabled = !m_impl->sequencer.IsFileLoaded();

  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));
  ImGui::PushItemFlag(ImGuiItemFlags_Disabled, disabled);

  if (disabled) {
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0xFF, 0xFF, 0xFF, 0x80));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0x00, 0x00, 0x00, 0x40));
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0x00, 0x7A, 0xFF, 0xFF));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
  }

  if (ImGui::ColoredButtonV1(ICON_FA_BACKWARD_FAST " Rewind", ImVec2{})) {
    fmt::println("Rewind");
  }
  ImGui::SameLine();
	if (ImGui::ColoredButtonV1(ICON_FA_PLAY " Play")) {
    m_impl->sequencer.AsyncPlayMIDI();
  }
  ImGui::SameLine();
  if (ImGui::ColoredButtonV1(ICON_FA_STOP " Stop")) {
    m_impl->sequencer.StopMIDI();
  }

  ImGui::PopItemFlag();
  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar(2);

	ImGui::End();
}

void MainApp::RunImGui() {
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  ImColor clearColor(m_clearColor);
  m_impl->sequencer.ProcessMessage([&](vinfony::SeqMsg &msg) -> bool {
    switch ( msg.type ) {
      case 1: // ThreadTerminate
        m_impl->sequencer.StopMIDI();
        fmt::println("message: ThreadTerminate");
        break;
      default:
        fmt::println("unknown message {}", msg.type);
    }
    return true;
  });

  DockSpaceUI();
	ToolbarUI();

  // 1. Show the big demo window and another window
  EngineBase::RunImGui();

  // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
  if (ImGui::BeginMainMenuBar())
  {
    if (ImGui::BeginMenu("File"))
    {
      if (ImGui::MenuItem("New", "CTRL+N")) {}
      if (ImGui::MenuItem("Open", "CTRL+O")) {
        ifd::FileDialog::Instance().Open("MidiFileOpenDialog",
          "Open a MIDI file", "Midi file (*.mid;*.rmi){.mid,.rmi},.*"
        );
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Exit", "CTRL+W")) {}
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window"))
    {
        ImGui::MenuItem("Demo Window",    nullptr, &m_showDemoWindow);
        //ImGui::MenuItem("Another Window", nullptr, &m_showAnotherWindow);
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

#if 0
  static int counter = 0;
  if (ImGui::Begin("Welcome to vinfony!")) {      // Create a window called "Hello, world!" and append into it.
    // Other Things
    ImGui::ColorEdit3("clear color", (float*)&clearColor.Value); // Edit 3 floats representing a color

    if (ImGui::Button("Button"))                           // Buttons return true when clicked (most widgets return true when edited/activated)
        counter++;
    ImGui::SameLine();
    ImGui::Text("counter = %d", counter);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
  }
  ImGui::End();
#endif

  ImGui::SetNextWindowSize({640, 480}, ImGuiCond_Once);
  if (ImGui::Begin("Vinfony Project")) {
    vinfony::DawMain("untitled", &m_impl->sequencer.displayState);
  }
  ImGui::End();

  if (ifd::FileDialog::Instance().IsDone("MidiFileOpenDialog")) {
    if (ifd::FileDialog::Instance().HasResult()) {
      const std::vector<std::filesystem::path>& res = ifd::FileDialog::Instance().GetResults();
      for (const auto& r : res) { // MidiFileOpenDialog supports multiselection
        //printf("OPEN[%s]\n", r.u8string().c_str());
        m_impl->sequencer.AsyncReadMIDIFile(r.u8string());
      }
    }
    ifd::FileDialog::Instance().Close();
  }
}

void MainApp::Init() {
  ReadIniConfig();
  EngineBase::Init();
  // ImFileDialog requires you to set the CreateTexture and DeleteTexture
	ifd::FileDialog::Instance().CreateTexture = ifd::openglCreateTexture;
	ifd::FileDialog::Instance().DeleteTexture = ifd::openglDeleteTexture;
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