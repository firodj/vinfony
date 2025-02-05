#pragma once

#include <memory>
#include <string>
#include <functional>

#include "hscpp/module/PreprocessorMacros.h" // Added so macros are available when using a tracked class.
#include "hscpp/module/Tracker.h"

#include "DawDisplay.hpp"
#include "../IDawSeq.hpp"
#include "../IDawTrack.hpp"

namespace vinfony {

struct DawProp;
class DawTrack;
struct DawMainStorage;

struct DawPropDrawParam {
	DawProp * self;
	IDawTrack * track;
	int r,c;
};

using DawPropDrawFunc = std::function<void(DawPropDrawParam * param, IDawSeq * seq)>;

struct DawProp {
	int         id;
	std::string name;
	int         w;
	DawPropDrawFunc DrawProp{};
};


// DawMainProject
class DawMainProject {

	HSCPP_TRACK(DawMainProject, "DawMainProject");

	struct DrawingState {
		float h0; // colum headaer height
		float tot_h; // total heigth of all rows
		IDawSeq *seq;
	};

public:
	hscpp_virtual ~DawMainProject();
    DawMainProject();
	hscpp_virtual void Update();

	void Draw(IDawSeq *seq);

	void Creating();
    void Destroying();

	std::unique_ptr<DawMainStorage> m_storage;
	bool m_showDebug;
	bool m_needRedraw;

	DrawingState m_drawingState;

	void DrawChild1();
	void DrawChild2();
};

};
