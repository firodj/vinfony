#include "MainApp.hpp"
#include <thread>
#include <mutex>
#include <iostream>
#include <vector>
#define IMGUI_DEFINE_MATH_OPERATORS

#include <SDL.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "DawMain.hpp"
#include "DawSeq.hpp"
#include "DawSoundFont.hpp"

#include <kosongg/INIReader.h>
#include <ifd/ImFileDialog.hpp>
#include <ifd/ImFileDialog_opengl.hpp>
#include <kosongg/IconsFontAwesome6.h>
#include "kosongg/Component.h"
#include <fmt/core.h>
#include <regex>
#include "circularfifo1.h"
#include "TsfDev.hpp"
#include "BassMidiDev.hpp"
#include "stb_image.h"
#include "PianoButton.hpp"

#define SAMPLE_RATE 44100.0

static std::unique_ptr<MainApp> g_mainapp;

static std::mutex g_mtxMainapp;

// Simple helper function to load an image into a OpenGL texture with common settings
bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height)
{
  // Load from file
  int image_width = 0;
  int image_height = 0;
  unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
  if (image_data == NULL) {
    fmt::println("Error LoadTextureFromFile {}", filename);
    return false;
  }

  // Create a OpenGL texture identifier
  GLuint image_texture;
  glGenTextures(1, &image_texture);
  glBindTexture(GL_TEXTURE_2D, image_texture);

  // Setup filtering parameters for display
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

  // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
  stbi_image_free(image_data);

  *out_texture = image_texture;
  *out_width = image_width;
  *out_height = image_height;

  return true;
}

struct MainApp::Impl {
  std::unique_ptr<vinfony::TinySoundFontDevice> tsfdev;
  std::unique_ptr<vinfony::BassMidiDevice> bassdev;
  // FIXME: sequencer should afer audiodevice.
  // the order of these struct member by compiler is necessary for descrutor ordering.
  vinfony::DawSeq sequencer;
  float toolbarSize{50};
  float menuBarHeight{0};
  std::string soundfontPath;
  GLuint texPiano{0};
  int pianoWidth,pianoHeight;

  SDL_AudioSpec OutputAudioSpec;
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
  m_windowTitle = "Vinfony";
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
	m_impl->menuBarHeight = ImGui::GetCurrentWindow()->MenuBarHeight;

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
    m_impl->sequencer.SetMIDITimeBeat(0);
  }
  ImGui::SameLine();
	if (ImGui::ColoredButtonV1(ICON_FA_PLAY " Play")) {
    //m_impl->sequencer.AsyncPlayMIDI();
    m_impl->sequencer.PlayMIDI();
  }
  ImGui::SameLine();
  if (ImGui::ColoredButtonV1(ICON_FA_STOP " Stop")) {
    m_impl->sequencer.StopMIDI();
  }
  ImGui::SameLine();

  std::string label = fmt::format(ICON_FA_GAUGE " {:.2f}", m_impl->sequencer.GetTempoBPM());
  ImGui::ColoredButtonV1(label.c_str());
  ImGui::SameLine();

  int curM, curB, curT;
  m_impl->sequencer.GetCurrentMBT(curM, curB, curT);
  label = fmt::format(ICON_FA_CLOCK " {:3d}:{:01d}:{:03d}", curM, curB, curT);
static ImVec2 labelMBTsize{};
  if (labelMBTsize.x == 0) {
    labelMBTsize = ImGui::CalcButtonSizeWithText(ICON_FA_CLOCK " 000:0:000", NULL, true);
  }
  ImGui::ColoredButtonV1(label.c_str(), labelMBTsize);

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
      case vinfony::IsAsyncPlayMIDITerminated: // ThreadTerminate
        //m_impl->sequencer.AsyncPlayMIDIStopped();
        fmt::println("message: ThreadTerminate");
        break;
      case vinfony::IsMIDIFileLoaded:
        {
          std::string base_filename = "Vinfony - " + msg.str.substr(msg.str.find_last_of("/\\") + 1);
          SDL_SetWindowTitle(m_sdlWindow, base_filename.c_str() );
        }
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
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 3.0f);
    vinfony::DawMain("untitled", &m_impl->sequencer);
    ImGui::PopStyleVar();
  }
  ImGui::End();

  ImGui::SetNextWindowSize({640, 480}, ImGuiCond_Once);
  if (ImGui::Begin("SoundFont")) {
    vinfony::DawSoundFont( m_impl->tsfdev.get() );
  }
  ImGui::End();

#if 0
  ImGui::SetNextWindowSize({640, 480}, ImGuiCond_Once);
  if (ImGui::Begin("Piano")) {
    vinfony::PianoButton("piano");

    if (m_impl->texPiano)
      ImGui::Image((void*)(intptr_t)m_impl->texPiano, ImVec2(m_impl->pianoWidth, m_impl->pianoHeight));

  }
  ImGui::End();
#endif


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

void AudioCallback(void* data, uint8_t *stream, int len)
{
  MainApp *app = static_cast<MainApp*>(data);
  app->StdAudioCallback(stream, len);
}

void MainApp::StdAudioCallback(uint8_t *stream, int len) {
  m_impl->sequencer.RenderMIDICallback (stream, len);
}

void MainApp::Init() {
  ReadIniConfig();
  EngineBase::Init();
  // ImFileDialog requires you to set the CreateTexture and DeleteTexture
	ifd::FileDialog::Instance().CreateTexture = ifd::openglCreateTexture;
	ifd::FileDialog::Instance().DeleteTexture = ifd::openglDeleteTexture;
  m_impl->tsfdev = std::make_unique<vinfony::TinySoundFontDevice>(m_impl->soundfontPath);
  if (!m_impl->tsfdev->Init()) {
    fmt::println("Error init tsf device");
    return;
  }
  m_impl->bassdev = std::make_unique<vinfony::BassMidiDevice>(m_impl->soundfontPath);
  if (!m_impl->bassdev->Init()) {
    fmt::println("Error init bass device");
    return;
  }
  m_impl->sequencer.SetTSFDevice( m_impl->tsfdev.get() );
  m_impl->sequencer.SetBASSDevice( m_impl->bassdev.get() );

  bool ret = LoadTextureFromFile(GetResourcePath("images", "piano.png").c_str(), &m_impl->texPiano, &m_impl->pianoWidth, &m_impl->pianoHeight);

  // Define the desired audio output format we request
  m_impl->OutputAudioSpec.freq = SAMPLE_RATE;
  m_impl->OutputAudioSpec.format = AUDIO_F32;
  m_impl->OutputAudioSpec.channels = 2;
  m_impl->OutputAudioSpec.samples = 4096 >> 2;
  m_impl->OutputAudioSpec.userdata = this;
  m_impl->OutputAudioSpec.callback = AudioCallback;

  // Request the desired audio output format
  if (SDL_OpenAudio(&m_impl->OutputAudioSpec, nullptr) < 0)
  {
    fprintf(stderr, "Could not open the audio hardware or the desired audio output format\n");
  } else {
    fprintf(stdout, "Audio Buffer = %d\n", m_impl->OutputAudioSpec.samples);
  }

  SDL_PauseAudio(0);
}

void MainApp::Clean() {
  EngineBase::Clean();

  SDL_PauseAudio(1);
  SDL_CloseAudio();
}

void MainApp::ReadIniConfig() {
  // Default Config
  m_impl->soundfontPath = "./data/Arachno SoundFont - Version 1.0.sf2";

  const char * homepath = nullptr;
#ifdef _WIN32
  homepath = std::getenv("USERPROFILE");
#else
  homepath = std::getenv("HOME");
#endif

  std::cout << "Home:" << homepath << std::endl;

  std::string iniPath = GetResourcePath("configs", "vinfony.ini");
  const char * inifilename = iniPath.c_str();
  INIReader reader(inifilename);

  if (reader.ParseError() < 0) {
    std::cout << "Can't load:" << inifilename << std::endl;
    return;
  }

  std::set<std::string> sections = reader.Sections();
  for (std::set<std::string>::iterator it = sections.begin(); it != sections.end(); ++it)
    std::cout << "Section:" << *it << std::endl;

  m_impl->soundfontPath = std::regex_replace(reader.Get("", "SOUNDFONT", m_impl->soundfontPath), std::regex("^~"), homepath);

  std::cout << "SOUNDFONT=" << m_impl->soundfontPath << std::endl;
}

std::string MainApp::GetResourcePath(const char *path, const char *file) {
  std::filesystem::path spath(path);
#ifdef BUILD_APPLE_BUNDLE
  if (spath == "kosongg/fonts") spath = "fonts";
#endif
  std::filesystem::path sfile(file);
  std::string res((spath / sfile).u8string());
#ifdef BUILD_APPLE_BUNDLE
  res = GetBundleResourcePath(res.c_str());
#endif
  return res;
}