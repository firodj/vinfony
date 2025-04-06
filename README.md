# vinfony

MIDI Music Creator

Video:
[![Screenshot](./docs/images/screenshot1.png)](https://youtu.be/HHwiREK-5zc?si=sXR79DU0YB6-c6T9)

## Dev Notes

**IMGUI**: How to store state (globally) when not using class/instances.

```c++
#define EMPTY_REF -1

static ImGuiID g_lastPianoButtonID{0};

const char * label = "piano";
ImGuiID pianoID = ImGui::GetID(label);
int *pianoStorageID = ImGui::GetStateStorage()->GetIntRef(pianoID, EMPTY_REF);

if (*pianoStorageID == EMPTY_REF) {
    // SetState
    *pianoStorageID = 1;
}

g_lastPianoButtonID = *pianoStorageID;
```

**IMGUI**: Get current window and scoped.

```c++
ImGuiContext& g = *GImGui;
ImGuiWindow* window = g.CurrentWindow;
const ImGuiID id = window->GetID(label);
int * storageIdx = window->StateStorage.GetIntRef(id, EMPTY_REF);
```
