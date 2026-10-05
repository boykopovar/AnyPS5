#include "InputEditor.hpp"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <stdexcept>
#include <set>

namespace {
const char* buttonName(SDL_GameControllerButton button) {
    const auto* name = SDL_GameControllerGetStringForButton(button);
    return name != nullptr ? name : "None";
}
const char* axisName(SDL_GameControllerAxis axis) {
    const auto* name = SDL_GameControllerGetStringForAxis(axis);
    return name != nullptr ? name : "None";
}
std::string cleanName(const char* name) {
    std::string result = name != nullptr ? name : "Controller";
    for (auto& c : result) if (c == ',' || c == '\n' || c == '\r') c = ' ';
    if (result.empty()) result = "Controller";
    return result;
}
}

struct InputConfig::Editor::State {
    std::filesystem::path path;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    ImGuiContext* context = nullptr;
    bool platformReady = false;
    bool rendererReady = false;
    bool loaded = false;
    std::array<bool, 3> dirty{};
    int activeTab = -1;
    SDL_Rect cancelBounds{};
    bool closing = false;
    std::string error;
    std::string status;
    std::vector<Pad::InputBinding> keyboard;
    ControllerProfiles profiles;
    std::vector<std::string> database;
    SDL_Joystick* joystick = nullptr;
    SDL_GameController* controller = nullptr;
    std::string guid;
    int captureAction = -1;
    int captureButton = -1;
    int captureAxis = -1;
    int wizardStep = 0;
    bool rawCapture = false;
    std::string candidate;
    std::vector<Sint16> neutral;
    std::map<std::string, std::string> rawBindings;
    std::map<std::string, std::map<std::string, std::string>> rawDrafts;
    std::map<std::string, int> rawSteps;
    std::set<std::string> unsavedRaw;
    SavedConfiguration saved = SavedConfiguration::None;

    explicit State(std::filesystem::path value) : path(std::move(value)) {}
    ~State() { destroy(); }

    void selectDevice(int index) {
        if (!guid.empty()) { rawDrafts[guid] = rawBindings; rawSteps[guid] = wizardStep; }
        cancelCapture();
        rawBindings.clear();
        wizardStep = 0;
        if (controller) SDL_GameControllerClose(controller);
        if (joystick) SDL_JoystickClose(joystick);
        controller = nullptr;
        joystick = nullptr;
        guid.clear();
        if (index < 0) return;
        joystick = SDL_JoystickOpen(index);
        if (!joystick) throw std::runtime_error(SDL_GetError());
        guid = Guid(joystick);
        if (rawDrafts.contains(guid)) rawBindings = rawDrafts.at(guid);
        if (rawSteps.contains(guid)) wizardStep = rawSteps.at(guid);
        if (SDL_IsGameController(index)) {
            controller = SDL_GameControllerOpen(index);
            if (!controller) throw std::runtime_error(SDL_GetError());
        }
    }
    void cancelCapture() {
        captureAction = captureButton = captureAxis = -1;
        rawCapture = false;
        candidate.clear();
        neutral.clear();
    }
    void load() {
        try {
            const auto newKeyboard = LoadKeyboard(path);
            const auto newProfiles = LoadControllers(path.parent_path() / "anyps5-controller.ini");
            const auto newDatabase = LoadDatabase(path.parent_path() / "anyps5-gamecontrollerdb.txt");
            keyboard = newKeyboard;
            profiles = newProfiles;
            database = newDatabase;
            loaded = true;
            dirty.fill(false);
            rawDrafts.clear(); rawSteps.clear(); unsavedRaw.clear();
            rawBindings.clear(); wizardStep = 0;
            error.clear();
            status.clear();
            cancelCapture();
        } catch (const std::exception& failure) {
            loaded = false;
            error = failure.what();
        }
    }
    void destroy() {
        if (context) ImGui::SetCurrentContext(context);
        if (rendererReady) ImGui_ImplSDLRenderer2_Shutdown();
        if (platformReady) ImGui_ImplSDL2_Shutdown();
        rendererReady = platformReady = false;
        if (context) ImGui::DestroyContext(context);
        context = nullptr;
        if (renderer) SDL_DestroyRenderer(renderer);
        renderer = nullptr;
        if (window) SDL_DestroyWindow(window);
        window = nullptr;
        selectDevice(-1);
    }
    void buttonSource(std::size_t index) {
        auto& profile = profiles[guid];
        const auto current = profile.buttons[index];
        if (ImGui::BeginCombo("Source", buttonName(current))) {
            for (int value = -1; value < SDL_CONTROLLER_BUTTON_MAX; ++value) {
                const auto source = static_cast<SDL_GameControllerButton>(value);
                if (ImGui::Selectable(buttonName(source), source == current)) { profile.buttons[index] = source; dirty[1] = true; }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Capture")) { cancelCapture(); captureButton = static_cast<int>(index); }
        if (controller && current != SDL_CONTROLLER_BUTTON_INVALID) {
            ImGui::SameLine(); ImGui::TextUnformatted(SDL_GameControllerGetButton(controller, current) ? "Pressed" : "Released");
        }
    }
    void axisSource(std::size_t index) {
        auto& source = profiles[guid].axes[index];
        if (ImGui::BeginCombo("Source", axisName(source.source))) {
            for (int value = -1; value < SDL_CONTROLLER_AXIS_MAX; ++value) {
                const auto axis = static_cast<SDL_GameControllerAxis>(value);
                if (ImGui::Selectable(axisName(axis), axis == source.source)) { source.source = axis; dirty[1] = true; }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Invert", &source.inverted)) dirty[1] = true;
        ImGui::SameLine();
        if (ImGui::Button("Capture")) {
            cancelCapture(); captureAxis = static_cast<int>(index);
            for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; ++axis) neutral.push_back(controller ? SDL_GameControllerGetAxis(controller, static_cast<SDL_GameControllerAxis>(axis)) : 0);
        }
        if (controller && source.source != SDL_CONTROLLER_AXIS_INVALID) ImGui::Text("Raw value: %d", SDL_GameControllerGetAxis(controller, source.source));
    }
    std::string wizardTarget() const {
        if (wizardStep < SDL_CONTROLLER_BUTTON_MAX) return buttonName(static_cast<SDL_GameControllerButton>(wizardStep));
        return axisName(static_cast<SDL_GameControllerAxis>(wizardStep - SDL_CONTROLLER_BUTTON_MAX));
    }
    void startRawCapture() {
        cancelCapture();
        SDL_JoystickUpdate();
        for (int axis = 0; axis < SDL_JoystickNumAxes(joystick); ++axis) neutral.push_back(SDL_JoystickGetAxis(joystick, axis));
        rawCapture = true;
    }
    void keyboardTab() {
        ImGui::TextWrapped("Select Capture, then press a key, mouse button or wheel direction. F10 opens this editor during gameplay.");
        for (std::size_t index = 0; index < Actions().size(); ++index) {
            const auto& action = Actions()[index];
            ImGui::PushID(static_cast<int>(index));
            ImGui::SeparatorText(action.name.data());
            for (std::size_t binding = 0; binding < keyboard.size();) {
                if (!Matches(keyboard[binding], action)) { ++binding; continue; }
                ImGui::PushID(static_cast<int>(binding));
                ImGui::TextUnformatted(BindingName(keyboard[binding]).c_str()); ImGui::SameLine();
                const bool remove = ImGui::SmallButton("Remove");
                ImGui::PopID();
                if (remove) { keyboard.erase(keyboard.begin() + static_cast<std::ptrdiff_t>(binding)); dirty[0] = true; }
                else ++binding;
            }
            if (ImGui::Button("Capture alternate")) { cancelCapture(); captureAction = static_cast<int>(index); }
            ImGui::SameLine();
            if (ImGui::Button("Default")) {
                std::erase_if(keyboard, [&action](const auto& binding) { return Matches(binding, action); });
                for (const auto& binding : Pad::InputMapping) if (Matches(binding, action)) keyboard.push_back(binding);
                dirty[0] = true;
            }
            ImGui::PopID();
        }
    }
    void devices() {
        const auto* name = joystick ? SDL_JoystickName(joystick) : nullptr;
        if (ImGui::BeginCombo("Controller", name ? name : "Select a controller")) {
            for (int index = 0; index < SDL_NumJoysticks(); ++index) {
                ImGui::PushID(index);
                const char* deviceName = SDL_JoystickNameForIndex(index);
                if (ImGui::Selectable(deviceName ? deviceName : "Unknown controller")) selectDevice(index);
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        if (joystick) ImGui::Text("GUID: %s", guid.c_str());
        else ImGui::TextWrapped("Connect a controller to configure its buttons, sticks and triggers.");
    }
    void controllerTab() {
        devices();
        if (!joystick) return;
        if (!controller) { ImGui::TextWrapped("SDL does not recognize this controller. Create its mapping in SDL Mapping first."); return; }
        ImGui::TextWrapped("Profiles apply to controllers with the same SDL GUID. Changes take effect when saved.");
        const auto preview = SampleController(controller, profiles[guid]);
        ImGui::Text("PS5 preview: buttons 0x%08x | sticks %u,%u / %u,%u | L2 %u R2 %u", preview.buttons,
            static_cast<unsigned>(preview.sticks[0]), static_cast<unsigned>(preview.sticks[1]), static_cast<unsigned>(preview.sticks[2]), static_cast<unsigned>(preview.sticks[3]),
            static_cast<unsigned>(preview.analogButtonsL2), static_cast<unsigned>(preview.analogButtonsR2));
        if (ImGui::Button("Restore controller defaults")) { profiles[guid] = ControllerProfile{}; dirty[1] = true; }
        for (std::size_t index = 0; index < ControllerButtons.size(); ++index) {
            ImGui::PushID(static_cast<int>(index));
            ImGui::SeparatorText(ControllerButtons[index].name.data());
            buttonSource(index);
            ImGui::PopID();
        }
        for (std::size_t index = 0; index < ControllerAxisNames.size(); ++index) {
            ImGui::PushID(static_cast<int>(index + ControllerButtons.size()));
            ImGui::SeparatorText(ControllerAxisNames[index].data());
            axisSource(index);
            ImGui::PopID();
        }
    }
    void databaseTab() {
        devices();
        if (!joystick) return;
        ImGui::TextWrapped("Release all controls before Capture. For stick axes, move right or down; for triggers, press fully. Each detected input requires confirmation. Skip controls that the device does not have.");
        const int count = static_cast<int>(SDL_CONTROLLER_BUTTON_MAX) + static_cast<int>(SDL_CONTROLLER_AXIS_MAX);
        if (wizardStep >= count) { ImGui::TextUnformatted("Mapping complete. Save SDL mapping below."); }
        else {
            ImGui::SeparatorText(wizardTarget().c_str());
            if (ImGui::Button("Capture physical input")) startRawCapture();
            ImGui::SameLine();
            if (ImGui::Button("Skip")) { cancelCapture(); ++wizardStep; }
            if (rawCapture) ImGui::TextUnformatted("Waiting for a physical input...");
            if (!candidate.empty()) {
                ImGui::Text("Detected: %s", candidate.c_str());
                if (ImGui::Button("Confirm")) { rawBindings[wizardTarget()] = candidate; ++wizardStep; cancelCapture(); unsavedRaw.insert(guid); dirty[2] = true; }
                ImGui::SameLine();
                if (ImGui::Button("Retry")) startRawCapture();
            }
        }
        if (ImGui::Button("Restart assistant")) { rawBindings.clear(); wizardStep = 0; cancelCapture(); unsavedRaw.erase(guid); dirty[2] = !unsavedRaw.empty(); }
        if (wizardStep > 0) {
            ImGui::SameLine();
            if (ImGui::Button("Previous")) { --wizardStep; cancelCapture(); }
        }
        ImGui::SeparatorText("Confirmed bindings");
        for (const auto& [target, source] : rawBindings) ImGui::Text("%s = %s", target.c_str(), source.c_str());
    }
    void save(int tab) {
        try {
            if (tab == 0) {
                WriteAtomic(path, SerializeKeyboard(keyboard));
                saved = SavedConfiguration::Keyboard;
            } else if (tab == 1) {
                WriteAtomic(path.parent_path() / "anyps5-controller.ini", SerializeControllers(profiles));
                saved = SavedConfiguration::Controller;
            } else {
                if (!joystick || rawBindings.empty()) throw std::runtime_error("Capture and confirm at least one physical input before saving");
                std::string mapping = guid + "," + cleanName(SDL_JoystickName(joystick)) + ",";
                for (const auto& [target, source] : rawBindings) mapping += target + ":" + source + ",";
                mapping += std::string("platform:") + SDL_GetPlatform() + ",";
                ValidateMapping(mapping);
                auto updated = database;
                std::erase_if(updated, [this](const auto& existing) { return existing.starts_with(guid + ",") && existing.find(std::string("platform:") + SDL_GetPlatform() + ",") != existing.npos; });
                updated.push_back(mapping);
                WriteAtomic(path.parent_path() / "anyps5-gamecontrollerdb.txt", SerializeDatabase(updated));
                database = std::move(updated);
                unsavedRaw.erase(guid);
                saved = SavedConfiguration::Database;
            }
            status = "Saved this tab. Unsaved edits in other tabs are retained.";
            dirty[tab] = tab == 2 && !unsavedRaw.empty();
            error.clear();
        } catch (const std::exception& failure) { error = failure.what(); }
    }
};

InputConfig::Editor::Editor(std::filesystem::path path) : state(std::make_unique<State>(std::move(path))) {}
InputConfig::Editor::~Editor() = default;

void InputConfig::Editor::Open() {
    if (IsOpen()) { SDL_RaiseWindow(state->window); return; }
    state->closing = false;
    state->window = SDL_CreateWindow("AnyPS5 Input Configuration", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 900, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    try {
        if (!state->window) throw std::runtime_error(SDL_GetError());
        SDL_SetWindowMinimumSize(state->window, 620, 420);
        state->renderer = SDL_CreateRenderer(state->window, -1, SDL_RENDERER_SOFTWARE);
        if (!state->renderer) throw std::runtime_error(SDL_GetError());
        IMGUI_CHECKVERSION();
        state->context = ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImFontConfig font;
        font.SizePixels = 17.0f;
        ImGui::GetIO().Fonts->AddFontDefault(&font);
        ImGui::StyleColorsDark();
        ImGui::GetStyle().WindowPadding = ImVec2(14, 12);
        ImGui::GetStyle().FramePadding = ImVec2(7, 4);
        ImGui::GetStyle().ItemSpacing = ImVec2(8, 6);
        state->platformReady = ImGui_ImplSDL2_InitForSDLRenderer(state->window, state->renderer);
        if (!state->platformReady) throw std::runtime_error("Input: ImGui SDL initialization failed");
        state->rendererReady = ImGui_ImplSDLRenderer2_Init(state->renderer);
        if (!state->rendererReady) throw std::runtime_error("Input: ImGui renderer initialization failed");
        state->load();
        if (SDL_NumJoysticks() > 0) {
            int selected = 0;
            for (int index = 0; index < SDL_NumJoysticks(); ++index) if (SDL_IsGameController(index)) { selected = index; break; }
            state->selectDevice(selected);
        }
    } catch (...) { state->destroy(); throw; }
}

void InputConfig::Editor::RefreshController() {
    const auto instance = state->joystick ? SDL_JoystickInstanceID(state->joystick) : -1;
    for (int index = 0; index < SDL_NumJoysticks(); ++index) {
        if (SDL_JoystickGetDeviceInstanceID(index) == instance) { state->selectDevice(index); return; }
    }
    state->selectDevice(-1);
}

void InputConfig::Editor::Close() { state->destroy(); }
bool InputConfig::Editor::IsOpen() const { return state->window != nullptr; }
unsigned InputConfig::Editor::WindowId() const { return state->window ? SDL_GetWindowID(state->window) : 0; }

bool InputConfig::Editor::HandleEvent(const SDL_Event& event) {
    if (!IsOpen()) return false;
    ImGui::SetCurrentContext(state->context);
    const auto instance = state->joystick ? SDL_JoystickInstanceID(state->joystick) : -1;
    if (event.type == SDL_JOYDEVICEREMOVED && event.jdevice.which == instance) {
        state->selectDevice(-1); state->status = "Controller disconnected. Select a connected device to continue.";
    }
    if (event.type == SDL_WINDOWEVENT && event.window.windowID == WindowId() && event.window.event == SDL_WINDOWEVENT_CLOSE) { state->closing = true; return true; }
    const bool capturing = state->captureAction >= 0 || state->captureButton >= 0 || state->captureAxis >= 0;
    const SDL_Point point{event.button.x, event.button.y};
    if (capturing && event.type == SDL_MOUSEBUTTONDOWN && event.button.windowID == WindowId() && SDL_PointInRect(&point, &state->cancelBounds)) {
        state->cancelCapture();
        return true;
    }
    const bool captureInput = state->captureAction >= 0 && (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEWHEEL);
    if (!captureInput) ImGui_ImplSDL2_ProcessEvent(&event);
    if (state->captureAction >= 0) {
        std::string source;
        if (event.type == SDL_KEYDOWN && event.key.windowID == WindowId() && !event.key.repeat) {
            try { source = BindingName({event.key.keysym.scancode, Pad::MouseButton::None, Pad::InputControl::Button}); }
            catch (const std::exception& failure) { state->error = failure.what(); }
        }
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.windowID == WindowId()) {
            Pad::InputBinding binding{SDL_SCANCODE_UNKNOWN, static_cast<Pad::MouseButton>(event.button.button), Pad::InputControl::Button};
            try { source = BindingName(binding); } catch (const std::exception& failure) { state->error = failure.what(); }
        }
        if (event.type == SDL_MOUSEWHEEL && event.wheel.windowID == WindowId() && event.wheel.y) {
            const auto direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
            source = direction > 0 ? "WHEEL:Up" : "WHEEL:Down";
        }
        if (!source.empty()) {
            try {
                if (source == "KEY:F10") throw std::runtime_error("F10 is reserved for opening the configuration editor");
                const auto binding = ParseBinding(Actions()[state->captureAction], source);
                const auto& action = Actions()[state->captureAction];
                const bool duplicate = std::any_of(state->keyboard.begin(), state->keyboard.end(), [&](const auto& existing) { return Matches(existing, action) && BindingName(existing) == source; });
                if (!duplicate) state->keyboard.push_back(binding);
                state->dirty[0] = true; state->error.clear(); state->cancelCapture();
            } catch (const std::exception& failure) { state->error = failure.what(); }
        }
    }
    if (state->captureButton >= 0 && event.type == SDL_CONTROLLERBUTTONDOWN && event.cbutton.which == instance) {
        state->profiles[state->guid].buttons[state->captureButton] = static_cast<SDL_GameControllerButton>(event.cbutton.button);
        state->dirty[1] = true; state->cancelCapture();
    }
    if (state->captureAxis >= 0 && event.type == SDL_CONTROLLERAXISMOTION && event.caxis.which == instance && event.caxis.axis < state->neutral.size() && std::abs(static_cast<int>(event.caxis.value) - state->neutral[event.caxis.axis]) > 16000) {
        state->profiles[state->guid].axes[state->captureAxis].source = static_cast<SDL_GameControllerAxis>(event.caxis.axis);
        state->dirty[1] = true; state->cancelCapture();
    }
    if (state->rawCapture && state->joystick) {
        std::string source;
        const bool axisTarget = state->wizardStep >= SDL_CONTROLLER_BUTTON_MAX;
        const bool digitalTarget = !axisTarget || state->wizardStep >= static_cast<int>(SDL_CONTROLLER_BUTTON_MAX) + static_cast<int>(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        if (digitalTarget && event.type == SDL_JOYBUTTONDOWN && event.jbutton.which == instance) source = "b" + std::to_string(event.jbutton.button);
        if (digitalTarget && event.type == SDL_JOYHATMOTION && event.jhat.which == instance && (event.jhat.value == 1 || event.jhat.value == 2 || event.jhat.value == 4 || event.jhat.value == 8)) source = "h" + std::to_string(event.jhat.hat) + "." + std::to_string(event.jhat.value);
        if (event.type == SDL_JOYAXISMOTION && event.jaxis.which == instance && event.jaxis.axis < state->neutral.size()) {
            const int resting = state->neutral[event.jaxis.axis];
            const int value = event.jaxis.value;
            if (std::abs(value - resting) > 16000) {
                const bool trigger = state->wizardStep >= static_cast<int>(SDL_CONTROLLER_BUTTON_MAX) + static_cast<int>(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
                source = "a" + std::to_string(event.jaxis.axis);
                if (!axisTarget || (trigger && std::abs(resting) < 8000)) source = std::string(value > resting ? "+" : "-") + source;
                else if (value < resting) source += "~";
            }
        }
        if (!source.empty()) { state->candidate = source; state->rawCapture = false; }
    }
    return true;
}

InputConfig::SavedConfiguration InputConfig::Editor::Render() {
    if (!IsOpen()) return SavedConfiguration::None;
    ImGui::SetCurrentContext(state->context);
    ImGui_ImplSDLRenderer2_NewFrame(); ImGui_ImplSDL2_NewFrame(); ImGui::NewFrame();
    const auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Input Configuration", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextWrapped("%s", state->path.string().c_str());
    if (!state->error.empty()) ImGui::TextWrapped("Error: %s", state->error.c_str());
    if (!state->status.empty()) ImGui::TextWrapped("%s", state->status.c_str());
    if (state->captureAction >= 0 || state->captureButton >= 0 || state->captureAxis >= 0) {
        ImGui::TextUnformatted("Waiting for input..."); ImGui::SameLine();
        if (ImGui::Button("Cancel capture")) state->cancelCapture();
        const auto minimum = ImGui::GetItemRectMin();
        const auto maximum = ImGui::GetItemRectMax();
        state->cancelBounds = {static_cast<int>(minimum.x), static_cast<int>(minimum.y), static_cast<int>(maximum.x - minimum.x), static_cast<int>(maximum.y - minimum.y)};
    }
    if (!state->loaded) { if (ImGui::Button("Reload configuration")) state->load(); }
    else if (ImGui::BeginTabBar("Sources")) {
        const char* tabs[]{"Keyboard / Mouse", "PS5 Commands", "SDL Mapping"};
        for (int tab = 0; tab < 3; ++tab) {
            if (!ImGui::BeginTabItem(tabs[tab])) continue;
            if (state->activeTab != tab) { state->cancelCapture(); state->activeTab = tab; }
            if (ImGui::Button("Save this tab")) state->save(tab);
            ImGui::SameLine();
            if (ImGui::Button("Discard all edits / Reload")) { state->selectDevice(-1); state->load(); }
            ImGui::BeginChild("Bindings", ImVec2(0, -40));
            if (tab == 0) state->keyboardTab();
            else if (tab == 1) state->controllerTab();
            else state->databaseTab();
            ImGui::EndChild(); ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    if (ImGui::Button("Close")) state->closing = true;
    if (state->closing) {
        if (std::any_of(state->dirty.begin(), state->dirty.end(), [](bool value) { return value; })) ImGui::OpenPopup("Unsaved edits");
    }
    if (ImGui::BeginPopupModal("Unsaved edits", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Close and discard unsaved edits?");
        if (ImGui::Button("Discard and close")) { state->dirty.fill(false); }
        ImGui::SameLine();
        if (ImGui::Button("Keep editing")) { state->closing = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    ImGui::End(); ImGui::Render();
    SDL_SetRenderDrawColor(state->renderer, 20, 22, 28, 255); SDL_RenderClear(state->renderer);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), state->renderer); SDL_RenderPresent(state->renderer);
    const auto saved = std::exchange(state->saved, SavedConfiguration::None);
    if (state->closing && std::none_of(state->dirty.begin(), state->dirty.end(), [](bool value) { return value; })) Close();
    return saved;
}
