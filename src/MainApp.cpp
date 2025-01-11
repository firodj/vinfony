#include "MainApp.hpp"
#include <thread>
#include <mutex>
#include <iostream>
#include <vector>

#include <SDL.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include "DawSeq.hpp"
#include "DawSoundFont.hpp"

#include <kosongg/INIReader.h>
#include <ifd/ImFileDialog.hpp>
#include <ifd/ImFileDialog_opengl.hpp>
#include <kosongg/IconsFontAwesome6.h>
#include "kosongg/Component.h"
#include "kosongg/GLUtil.h"
#include <kosongg/GetExeDirectory.h>
#include <fmt/core.h>
#include <regex>
#include "circularfifo1.h"
#include "TsfDev.hpp"
#include "BassMidiDev.hpp"

#include "PianoButton.hpp"
#include "Misc.hpp"

#include "hscpp/Filesystem.h"
#include "hscpp/Hotswapper.h"
#include "hscpp/Util.h"
#include "hscpp/mem/Ref.h"
#include "hscpp/mem/MemoryManager.h"

#include "UI/MainWidget.hpp"

#include "Globals.hpp"

#define SAMPLE_RATE 44100.0

static std::unique_ptr<MainApp> g_mainapp;
static std::mutex g_mtxMainapp;

struct MainApp::Impl {
	std::unique_ptr<hscpp::Hotswapper> swapper;
	std::unique_ptr<vinfony::Globals> globals;
	hscpp::mem::UniqueRef<hscpp::mem::MemoryManager> memoryManager;

	std::unique_ptr<vinfony::TinySoundFontDevice> tsfdev;
	std::unique_ptr<vinfony::BassMidiDevice> bassdev;
	// FIXME: sequencer should afer audiodevice.
	// the order of these struct member by compiler is necessary for descrutor ordering.
	vinfony::DawSeq sequencer;
	float toolbarSize{50};
	float menuBarHeight{0};
	std::string soundfontPath;
	unsigned int texPiano{0};
	int pianoWidth,pianoHeight;

	bool showSoundFont;
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
	m_impl->showSoundFont = false;

	std::string path = GetExeDirectory();
}

MainApp::~MainApp() {
}

void MainApp::RunImGui() {
	vinfony::Globals *globals = vinfony::Globals::Resolve();
	m_impl->swapper->Update();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImColor clearColor(m_clearColor);
	m_impl->sequencer.ProcessMessage([&](vinfony::SeqMsg &msg) -> bool {
		switch ( msg.type ) {
			case vinfony::IsAsyncPlayMIDITerminated: // ThreadTerminate
				m_impl->sequencer.AsyncPlayMIDIStopped();
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

	globals->pMainWidget->Update();

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
#if 0
	ImGui::SetNextWindowSize({640, 480}, ImGuiCond_Once);
	if (ImGui::Begin("Vinfony Project")) {
		ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 3.0f);
		vinfony::DawMain("untitled", &m_impl->sequencer);
		ImGui::PopStyleVar();
	}
	ImGui::End();
#endif
	if (m_impl->showSoundFont) {
		ImGui::SetNextWindowSize({640, 480}, ImGuiCond_Once);
		if (ImGui::Begin("SoundFont", &m_impl->showSoundFont)) {
			vinfony::DawSoundFont( m_impl->tsfdev.get() );
		}
		ImGui::End();
	}

	ImGui::SetNextWindowSize({640, 480}, ImGuiCond_Once);
	if (ImGui::Begin("Piano")) {
		ImGui::BeginChild("piano", ImVec2{0.0, 200.0}, ImGuiChildFlags_ResizeY | ImGuiChildFlags_Border, ImGuiWindowFlags_AlwaysHorizontalScrollbar);
		vinfony::PianoButtonV("piano");
		ImGui::EndChild();

		if (m_impl->texPiano)
			ImGui::Image((ImTextureID)m_impl->texPiano, ImVec2(m_impl->pianoWidth, m_impl->pianoHeight));
	}
	ImGui::End();

	if (ifd::FileDialog::Instance().IsDone("MidiFileOpenDialog")) {
		if (ifd::FileDialog::Instance().HasResult()) {
			const std::vector<std::filesystem::path>& res = ifd::FileDialog::Instance().GetResults();
			for (const auto& r : res) { // MidiFileOpenDialog supports multiselection
				m_impl->sequencer.AsyncReadMIDIFile(r.u8string());
			}
		}
		ifd::FileDialog::Instance().Close();
	}
}

void MainApp::Init(std::vector<std::string> &args) {
	ReadIniConfig();

	// InitSDL & InitImGui
	EngineBase::Init(args);

	auto projPath = hscpp::fs::canonical( hscpp::fs::path(_PROJECT_SRC_PATH_) );

	m_impl->globals = std::make_unique<vinfony::Globals>();

	auto swapperConfig = std::make_unique<hscpp::Config>();
	swapperConfig->compiler.projPath = projPath;

	m_impl->swapper = std::make_unique<hscpp::Hotswapper>(std::move(swapperConfig));
	m_impl->swapper->EnableFeature(hscpp::Feature::Preprocessor);
	m_impl->swapper->EnableFeature(hscpp::Feature::DependentCompilation);
#ifdef _WIN32
    m_impl->swapper->SetVar("os", "Windows");
#else
    m_impl->swapper->SetVar("os", "Posix");
#endif


	m_impl->swapper->SetVar("projPath", projPath);

	m_impl->swapper->AddSourceDirectory(projPath / "src" / "UI");
	//m_impl->swapper->AddIncludeDirectory(projPath / "src" );

	auto buildPath = hscpp::util::GetHscppBuildPath();
	m_impl->swapper->SetVar("buildPath", buildPath);

	hscpp::mem::MemoryManager::Config config;
	config.pAllocationResolver = m_impl->swapper->GetAllocationResolver();
	m_impl->memoryManager = hscpp::mem::MemoryManager::Create(config);
	m_impl->swapper->SetAllocator(&m_impl->memoryManager);

	m_impl->swapper->SetGlobalUserData(m_impl->globals.get());

	m_impl->globals->pMemoryManager = &m_impl->memoryManager;
	m_impl->globals->pImGuiContext = ImGui::GetCurrentContext();
	m_impl->globals->pMainWidget = m_impl->memoryManager->Allocate<vinfony::MainWidget>();

	m_impl->globals->sequencer = &m_impl->sequencer;

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

	bool ret = kosongg::LoadTextureFromFile(GetResourcePath("images", "piano.png").c_str(), &m_impl->texPiano, &m_impl->pianoWidth, &m_impl->pianoHeight);
	fmt::println("Loading images piano.png, status: {}", ret);
}

void MainApp::Clean() {
	m_impl->globals.reset();
	m_impl->swapper.reset();

	EngineBase::Clean();
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