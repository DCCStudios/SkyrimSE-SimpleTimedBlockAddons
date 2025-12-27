#include "Menu.h"
#include "Settings.h"
#include "SKSEMenuFramework.h"

namespace Menu
{
    namespace State
    {
        // Session values - these are what we edit in the UI
        // They start as copies of the saved settings and can be modified freely
        // Only when user clicks "Save" are they written to INI
        
        inline bool initialized{ false };
        inline bool hasUnsavedChanges{ false };
        
        // Hitstop
        inline bool  bEnableHitstop{ true };
        inline float fHitstopSpeed{ 0.05f };
        inline float fHitstopDuration{ 0.15f };
        
        // Stagger
        inline bool  bEnableStagger{ true };
        inline float fStaggerMagnitude{ 0.7f };
        
        // Pushback
        inline bool  bEnablePushback{ false };
        inline float fPushbackMagnitude{ 1.5f };
        
        // Camera Shake
        inline bool  bEnableCameraShake{ true };
        inline float fCameraShakeStrength{ 0.5f };
        inline float fCameraShakeDuration{ 0.2f };
        
        // Sound
        inline bool bEnableSound{ true };
        
        // Cooldown
        inline bool  bEnableCooldown{ true };
        inline float fCooldownDurationMs{ 250.0f };
        
        // Debug
        inline bool bDebugLogging{ false };
        
        void LoadFromSettings()
        {
            auto* settings = Settings::GetSingleton();
            
            bEnableHitstop = settings->bEnableHitstop;
            fHitstopSpeed = settings->fHitstopSpeed;
            fHitstopDuration = settings->fHitstopDuration;
            
            bEnableStagger = settings->bEnableStagger;
            fStaggerMagnitude = settings->fStaggerMagnitude;
            
            bEnablePushback = settings->bEnablePushback;
            fPushbackMagnitude = settings->fPushbackMagnitude;
            
            bEnableCameraShake = settings->bEnableCameraShake;
            fCameraShakeStrength = settings->fCameraShakeStrength;
            fCameraShakeDuration = settings->fCameraShakeDuration;
            
            bEnableSound = settings->bEnableSound;
            
            bEnableCooldown = settings->bEnableCooldown;
            fCooldownDurationMs = settings->fCooldownDurationMs;
            
            bDebugLogging = settings->bDebugLogging;
            
            hasUnsavedChanges = false;
            initialized = true;
        }
        
        void ApplyToSettings()
        {
            auto* settings = Settings::GetSingleton();
            
            settings->bEnableHitstop = bEnableHitstop;
            settings->fHitstopSpeed = fHitstopSpeed;
            settings->fHitstopDuration = fHitstopDuration;
            
            settings->bEnableStagger = bEnableStagger;
            settings->fStaggerMagnitude = fStaggerMagnitude;
            
            settings->bEnablePushback = bEnablePushback;
            settings->fPushbackMagnitude = fPushbackMagnitude;
            
            settings->bEnableCameraShake = bEnableCameraShake;
            settings->fCameraShakeStrength = fCameraShakeStrength;
            settings->fCameraShakeDuration = fCameraShakeDuration;
            
            settings->bEnableSound = bEnableSound;
            
            settings->bEnableCooldown = bEnableCooldown;
            settings->fCooldownDurationMs = fCooldownDurationMs;
            
            settings->bDebugLogging = bDebugLogging;
        }
        
        void RevertToSaved()
        {
            LoadFromSettings();
        }
    }

    void Register()
    {
        if (!SKSEMenuFramework::IsInstalled()) {
            logger::warn("SKSE Menu Framework not installed, in-game menu disabled");
            return;
        }
        
        SKSEMenuFramework::SetSection("Simple Timed Block Addons");
        SKSEMenuFramework::AddSectionItem("Settings", RenderSettings);
        
        logger::info("SKSE Menu Framework menu registered");
    }

    void __stdcall RenderSettings()
    {
        // Initialize state on first render
        if (!State::initialized) {
            State::LoadFromSettings();
        }
        
        // Header with unsaved indicator
        ImGui::Text("Simple Timed Block Addons - Settings");
        if (State::hasUnsavedChanges) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "(Unsaved changes)");
        }
        ImGui::Separator();
        
        // ===== HITSTOP SECTION =====
        if (ImGui::CollapsingHeader("Hitstop (Attacker Animation Freeze)", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Checkbox("Enable Hitstop", &State::bEnableHitstop)) {
                State::hasUnsavedChanges = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Freezes the attacker's animation on timed block.\nDoes NOT freeze the entire game world!");
            }
            
            if (State::bEnableHitstop) {
                ImGui::Indent();
                
                if (ImGui::SliderFloat("Animation Speed", &State::fHitstopSpeed, 0.0f, 0.3f, "%.2f")) {
                    State::hasUnsavedChanges = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("How slow the attacker's animation plays during hitstop.\n0.0 = Complete freeze\n0.1 = Very slow motion");
                }
                
                if (ImGui::SliderFloat("Duration (seconds)", &State::fHitstopDuration, 0.05f, 0.5f, "%.2f")) {
                    State::hasUnsavedChanges = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("How long the hitstop effect lasts.");
                }
                
                ImGui::Unindent();
            }
        }
        
        // ===== STAGGER SECTION =====
        if (ImGui::CollapsingHeader("Stagger", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Checkbox("Enable Stagger", &State::bEnableStagger)) {
                State::hasUnsavedChanges = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Forces the attacker into a stagger animation on timed block.");
            }
            
            if (State::bEnableStagger) {
                ImGui::Indent();
                
                if (ImGui::SliderFloat("Stagger Magnitude", &State::fStaggerMagnitude, 0.0f, 1.5f, "%.2f")) {
                    State::hasUnsavedChanges = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Stagger strength:\n0.0 = Small stagger\n0.3 = Medium stagger\n0.7 = Large stagger\n1.0+ = Largest stagger");
                }
                
                ImGui::Unindent();
            }
        }
        
        // ===== PUSHBACK SECTION =====
        if (ImGui::CollapsingHeader("Pushback")) {
            if (ImGui::Checkbox("Enable Pushback", &State::bEnablePushback)) {
                State::hasUnsavedChanges = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Pushes the attacker away on timed block.");
            }
            
            if (State::bEnablePushback) {
                ImGui::Indent();
                
                if (ImGui::SliderFloat("Pushback Magnitude", &State::fPushbackMagnitude, 0.5f, 5.0f, "%.1f")) {
                    State::hasUnsavedChanges = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("How far to push the attacker (in game units).");
                }
                
                ImGui::Unindent();
            }
        }
        
        // ===== CAMERA SHAKE SECTION =====
        if (ImGui::CollapsingHeader("Camera Shake", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Checkbox("Enable Camera Shake", &State::bEnableCameraShake)) {
                State::hasUnsavedChanges = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Adds camera shake for impact feel on timed block.");
            }
            
            if (State::bEnableCameraShake) {
                ImGui::Indent();
                
                if (ImGui::SliderFloat("Shake Strength", &State::fCameraShakeStrength, 0.1f, 2.0f, "%.2f")) {
                    State::hasUnsavedChanges = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Intensity of the camera shake.");
                }
                
                if (ImGui::SliderFloat("Shake Duration (seconds)", &State::fCameraShakeDuration, 0.05f, 0.5f, "%.2f")) {
                    State::hasUnsavedChanges = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("How long the camera shake lasts.");
                }
                
                ImGui::Unindent();
            }
        }
        
        // ===== SOUND SECTION =====
        if (ImGui::CollapsingHeader("Sound")) {
            if (ImGui::Checkbox("Enable Sound", &State::bEnableSound)) {
                State::hasUnsavedChanges = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Plays a sound effect on timed block.\nSound path can be configured in the INI file.");
            }
        }
        
        // ===== COOLDOWN SECTION =====
        if (ImGui::CollapsingHeader("Timed Block Cooldown", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Checkbox("Enable Cooldown", &State::bEnableCooldown)) {
                State::hasUnsavedChanges = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Prevents spamming the block button for timed blocks.\nIf you block but fail to trigger a timed block within the window,\nyou must wait for the cooldown before attempting another timed block.");
            }
            
            if (State::bEnableCooldown) {
                ImGui::Indent();
                
                if (ImGui::SliderFloat("Cooldown Duration (ms)", &State::fCooldownDurationMs, 0.0f, 1000.0f, "%.0f ms")) {
                    State::hasUnsavedChanges = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("How long after a failed timed block attempt before you can try again.\n250ms is recommended for balanced gameplay.");
                }
                
                ImGui::Unindent();
            }
        }
        
        // ===== DEBUG SECTION =====
        if (ImGui::CollapsingHeader("Debug")) {
            if (ImGui::Checkbox("Debug Logging", &State::bDebugLogging)) {
                State::hasUnsavedChanges = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enable detailed logging to the plugin log file.");
            }
        }
        
        ImGui::Separator();
        
        // ===== ACTION BUTTONS =====
        if (ImGui::Button("Save to INI")) {
            State::ApplyToSettings();
            Settings::GetSingleton()->SaveSettings();
            State::hasUnsavedChanges = false;
            RE::DebugNotification("Simple Timed Block Addons: Settings saved!");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Save current settings to the INI file.\nChanges will persist across game sessions.");
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Revert to Saved")) {
            State::RevertToSaved();
            RE::DebugNotification("Simple Timed Block Addons: Settings reverted!");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Discard unsaved changes and reload from INI.");
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Reload INI")) {
            Settings::GetSingleton()->LoadSettings();
            State::LoadFromSettings();
            RE::DebugNotification("Simple Timed Block Addons: INI reloaded!");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reload settings from the INI file on disk.");
        }
        
        // Show note about session changes
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
            "Note: Changes apply immediately for this session.");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
            "Use 'Save to INI' to persist changes.");
    }
}


