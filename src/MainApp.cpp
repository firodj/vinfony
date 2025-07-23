#include "mainapp.hpp"
#include <thread>
#include <mutex>
#include <iostream>
#include <vector>
#include <chrono>

#include <SDL.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include "DawSeq.hpp"
//#include "DawSoundFont.hpp"

#define MINI_CASE_SENSITIVE
#include <kosongg/m_ini.hpp>
#include <ImFileDialog.hpp>
#include <ImFileDialog_opengl.hpp>
#include <kosongg/iconsfontawesome6.h>
#include "kosongg/glutil.hpp"
#include <kosongg/getexedirectory.hpp>
#include <fmt/core.h>
#include <regex>
#include "circularfifo1.h"
#include "TsfDev.hpp"
#include "BassMidiDev.hpp"

//#include "PianoButton.hpp"
#include "Misc.hpp"


#ifdef _USE_HSCPP_
#include "hscpp/Filesystem.h"
#include "hscpp/Hotswapper.h"
#include "hscpp/Util.h"
#include "hscpp/mem/Ref.h"
#include "hscpp/mem/MemoryManager.h"
#include "kosongg/hscpp_progress.hpp"
#endif

#include "UI/MainWidget.hpp"

#include "globals.hpp"
#include "watched/imcontrol.hpp"


#define SAMPLE_RATE 44100.0

static std::unique_ptr<MainApp> g_mainapp;
static std::mutex g_mtxMainapp;

struct MainApp::Impl {
	std::unique_ptr<vinfony::Globals> globals;
#ifdef _USE_HSCPP_
	std::unique_ptr<hscpp::Hotswapper> swapper;
	hscpp::mem::UniqueRef<hscpp::mem::MemoryManager> memoryManager;
#endif
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

#ifndef _USE_HSCPP_
vinfony::Globals * vinfony::Globals::m_g;
#endif

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
	//m_impl->m_showDemoWindow = false;
	m_impl->showSoundFont = false;

	std::string path = GetExeDirectory().u8string();
}

MainApp::~MainApp() {
}

void MainApp::UpdateHsCpp() {
	vinfony::Globals *globals = vinfony::Globals::Resolve();

#ifdef _USE_HSCPP_
	auto updateResult = m_impl->swapper->Update();

	if (!globals->pHsCppProgress) {
		globals->pHsCppProgress = std::make_unique<HsCppProgress>();
	}

	switch (updateResult) {
		case hscpp::Hotswapper::UpdateResult::Compiling:
			globals->pHsCppProgress->lastCompilingText = "Compiling";
			globals->pHsCppProgress->lastCompilingColor = {222, 222, 0};
			globals->pHsCppProgress->lastElapsedCompileTime = std::chrono::high_resolution_clock::now() - globals->pHsCppProgress->startCompileTime;
			break;
		case hscpp::Hotswapper::UpdateResult::StartedCompiling:
			globals->pHsCppProgress->startCompileTime = std::chrono::high_resolution_clock::now();
			break;
		case hscpp::Hotswapper::UpdateResult::PerformedSwap:
			globals->pHsCppProgress->lastCompilingText = "PerformedSwap";
			break;
		case hscpp::Hotswapper::UpdateResult::FailedSwap:
			globals->pHsCppProgress->lastCompilingColor = {172, 0, 0};
			globals->pHsCppProgress->lastCompilingText = "FailedSwap";
			break;
		default:
			switch (globals->pHsCppProgress->lastUpdateResult) {
				case (int)hscpp::Hotswapper::UpdateResult::Compiling:
					globals->pHsCppProgress->lastCompilingColor = {172, 0, 0};
					globals->pHsCppProgress->lastCompilingText = "Error";
					break;
				case (int)hscpp::Hotswapper::UpdateResult::PerformedSwap:
					globals->pHsCppProgress->lastCompilingColor = {0, 172, 0};
					globals->pHsCppProgress->lastCompilingText = "Success";
					break;
				default:
					break;
			};
	};
	globals->pHsCppProgress->lastUpdateResult = (int)updateResult;
#endif
}

void MainApp::RunImGui() {
	vinfony::Globals *globals = vinfony::Globals::Resolve();

	UpdateHsCpp();
	
	// Process custom messages
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
	ImGui::SetNextWindowSize({640, 480}, ImGuiCond_Once);
	if (ImGui::Begin("Piano")) {
		ImGui::BeginChild("piano", ImVec2{0.0, 200.0}, ImGuiChildFlags_ResizeY | ImGuiChildFlags_Border, ImGuiWindowFlags_AlwaysHorizontalScrollbar);
		vinfony::PianoButtonV("piano");
		ImGui::EndChild();

		if (m_impl->texPiano)
			ImGui::Image((ImTextureID)m_impl->texPiano, ImVec2(m_impl->pianoWidth, m_impl->pianoHeight));
	}
	ImGui::End();
#endif


#ifdef _USE_IFD_
	if (ifd::FileDialog::Instance().IsDone("MidiFileOpenDialog")) {
		if (ifd::FileDialog::Instance().HasResult()) {
			const std::vector<std::filesystem::path>& res = ifd::FileDialog::Instance().GetResults();
			for (const auto& r : res) { // MidiFileOpenDialog supports multiselection
				m_impl->sequencer.AsyncReadMIDIFile(r.u8string());
			}
		}
		ifd::FileDialog::Instance().Close();
	}
#endif
}

void MainApp::Init(std::vector<std::string> &args) {
	m_impl->globals = std::make_unique<vinfony::Globals>();
	m_impl->globals->getResourcePath = std::bind(&MainApp::GetResourcePath, this, std::placeholders::_1, std::placeholders::_2);

	ReadIniConfig();
	// InitSDL & InitImGui
	EngineBase::Init(args);

	m_impl->globals->pImGuiContext = ImGui::GetCurrentContext();

#ifdef _USE_HSCPP_
	auto projPath = hscpp::fs::canonical( hscpp::fs::path(_PROJECT_SRC_PATH_) );
	auto extPath = hscpp::fs::canonical(hscpp::fs::path(_PROJECT_EXT_PATH_));

	auto swapperConfig = std::make_unique<hscpp::Config>();
	swapperConfig->compiler.projPath = projPath.u8string();
	swapperConfig->compiler.ninja = true;
	std::cerr << "ninja path = " << swapperConfig->compiler.ninjaExecutable << std::endl;

	m_impl->swapper = std::make_unique<hscpp::Hotswapper>(std::move(swapperConfig));
	m_impl->swapper->EnableFeature(hscpp::Feature::Preprocessor);
	m_impl->swapper->EnableFeature(hscpp::Feature::DependentCompilation);
#ifdef _WIN32
    m_impl->swapper->SetVar("os", "Windows");
#elif defined(__APPLE__)
	m_impl->swapper->SetVar("os", "Darwin");
#else
	m_impl->swapper->SetVar("os", "Linux");
#endif
	m_impl->swapper->SetVar("projPath", projPath.u8string());
	m_impl->swapper->SetVar("extPath", extPath.u8string());

#ifdef _USE_IFD_
	m_impl->swapper->SetVar("use_ifd", true);
	m_impl->swapper->AddPreprocessorDefinition("_USE_IFD_");
#else
 	m_impl->swapper->SetVar("use_ifd", false);
#endif

	// Source directory to watchs
	m_impl->swapper->AddSourceDirectory(projPath / "src" / "UI");
	m_impl->swapper->AddSourceDirectory(projPath / "kosongg/cpp/watched");
	//m_impl->swapper->AddIncludeDirectory(projPath / "src" );

	auto buildPath = hscpp::util::GetHscppBuildPath();
	m_impl->swapper->SetVar("buildPath", buildPath.u8string());
	m_impl->swapper->AddPreprocessorDefinition("_USE_HSCPP_");

#ifdef IMGUI_USER_CONFIG
	m_impl->swapper->AddPreprocessorDefinition(fmt::format("IMGUI_USER_CONFIG=\\\"{}\\\"", hscpp::fs::path(IMGUI_USER_CONFIG).u8string()));
#endif
#ifdef imgui_IMPORTS
	m_impl->swapper->AddPreprocessorDefinition("imgui_IMPORTS"); // if BUILD_SHARED
#endif

	hscpp::mem::MemoryManager::Config config;
	config.pAllocationResolver = m_impl->swapper->GetAllocationResolver();
	m_impl->memoryManager = hscpp::mem::MemoryManager::Create(config);
	m_impl->swapper->SetAllocator(&m_impl->memoryManager);

	m_impl->swapper->SetGlobalUserData(m_impl->globals.get());

	m_impl->globals->pMemoryManager = &m_impl->memoryManager;
	m_impl->globals->pImGuiContext = ImGui::GetCurrentContext();
	m_impl->globals->pMainWidget = m_impl->memoryManager->Allocate<vinfony::MainWidget>();
	m_impl->globals->pImControl = m_impl->memoryManager->Allocate<kosongg::ImControl>();
	m_impl->globals->sequencer = &m_impl->sequencer;
#else
	Globals::SetGlobalUserData(m_impl->globals.get());

	m_impl->globals->pMainWidget = std::make_unique<MainWidget>();
	m_impl->globals->pImControl = std::make_unique<kosongg::ImControl>();
#endif

#ifdef _USE_IFD_
	// ImFileDialog requires you to set the CreateTexture and DeleteTexture
	ifd::FileDialog::Instance().CreateTexture = ifd::openglCreateTexture;
	ifd::FileDialog::Instance().DeleteTexture = ifd::openglDeleteTexture;
#endif

	m_impl->tsfdev = std::make_unique<vinfony::TinySoundFontDevice>(m_impl->soundfontPath);
	if (!m_impl->tsfdev->Init()) {
		fmt::println("Error init tsf device");
		return;
	}
	m_impl->globals->pTinySoundFont = m_impl->tsfdev->GetTSF();

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
#ifdef _USE_HSCPP_
	m_impl->globals.reset();
	m_impl->swapper.reset();
#endif

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

	mINI::INIFile inifile(inifilename);
	mINI::INIStructure ini;
	if (!inifile.read(ini)) {
		std::cerr << "Can't load:" << inifilename << std::endl;
		return;
	}
	for (auto const & [sectionName, section]: ini) {
		std::cout << "[" << sectionName << "]" << std::endl;
		for (auto const & value: section) {
			std::cout << "\t" << value.first << " = " << value.second << std::endl;
		}
	}
	auto soundfontPath = ini["SoundFont"]["Path"];
	if (!soundfontPath.empty()) m_impl->soundfontPath = soundfontPath;
	m_impl->soundfontPath = std::regex_replace(m_impl->soundfontPath, std::regex("^~"), homepath);

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