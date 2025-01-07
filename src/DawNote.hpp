#pragma once

namespace vinfony {

struct DawNote {
	long time;
	long stop;
	unsigned char note;
	int event_num;
	int used_prev;
	int used_next;
};

};