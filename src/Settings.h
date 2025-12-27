#pragma once

class Settings {
public:
    static Settings* GetSingleton();

    void LoadSettings();
    void SaveSettings();
    
    //==========================================================================
    // Hitstop Settings - Freezes attacker's animation (NOT the whole world!)
    //==========================================================================
    bool  bEnableHitstop{ true };
    float fHitstopSpeed{ 0.05f };          // Animation speed during hitstop (0.0 = freeze, 0.1 = very slow)
    float fHitstopDuration{ 0.15f };       // Duration in seconds
    
    //==========================================================================
    // Stagger Settings - Force attacker into stagger animation
    //==========================================================================
    bool  bEnableStagger{ true };
    float fStaggerMagnitude{ 0.7f };       // Stagger strength (0=small, 0.3=medium, 0.7=large, 1.0+=largest)
    
    //==========================================================================
    // Pushback Settings - Push attacker away on timed block
    //==========================================================================
    bool  bEnablePushback{ false };        // Disabled by default
    float fPushbackMagnitude{ 1.5f };      // How far to push the attacker (units)
    
    //==========================================================================
    // Camera Shake Settings - Impact feel
    //==========================================================================
    bool  bEnableCameraShake{ true };
    float fCameraShakeStrength{ 0.5f };    // Shake intensity
    float fCameraShakeDuration{ 0.2f };    // Shake duration in seconds
    
    //==========================================================================
    // Sound Settings
    //==========================================================================
    bool  bEnableSound{ true };
    std::string sSoundPath{ "UIMenuOK" };  // Sound descriptor EditorID or file path
    
    //==========================================================================
    // Timed Block Cooldown Settings
    //==========================================================================
    bool  bEnableCooldown{ true };         // Enable cooldown after failed timed block
    float fCooldownDurationMs{ 250.0f };   // Cooldown duration in milliseconds
    
    //==========================================================================
    // Original mod settings (read-only, for compatibility)
    //==========================================================================
    std::string sBlockKey{ "V" };
    float fStaggerDistance{ 128.0f };
    bool  bPerkLockedBlock{ false };
    bool  bOnlyWithShield{ false };
    bool  bPerkLockedStagger{ false };
    
    // Debug
    bool bDebugLogging{ false };

private:
    Settings() = default;
    Settings(const Settings&) = delete;
    Settings(Settings&&) = delete;
    Settings& operator=(const Settings&) = delete;
    Settings& operator=(Settings&&) = delete;

    static constexpr const char* INI_PATH = R"(.\Data\SKSE\Plugins\simple-timed-block.ini)";
};
