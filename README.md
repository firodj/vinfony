# vinfony

MIDI Music Creator

## Dev Notes

**IMGUI**: How to store state (globally) when not using class/instances.

```c++
#define EMPTY_REF -1

static ImGuiID g_lastPianoButtonID{0};

const char * label = "piano";
ImGuiID pianoID = ImGui::GetID(label);
int *pianoStorageID = ImGui::GetStateStorage()->GetIntRef(pianoID, EMPTY_REF);

if (*pianoStorageID == -1) {
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
