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
    float fStaggerMagnitude{ 0.7f };            // Stagger for normal attacks (0=small, 0.3=medium, 0.7=large, 1.0+=largest)
    float fPowerAttackStaggerMagnitude{ 1.0f }; // Stagger for power attacks (usually higher)
    
    // Skill-based stagger chance
    bool  bUseStaggerChance{ false };       // Disabled by default (guaranteed stagger)
    float fBaseStaggerChance{ 50.0f };      // Base chance at 0 skill (50%)
    float fMaxStaggerChance{ 100.0f };      // Max chance at 100 skill (100%)
    bool  bStaggerUseBlockSkill{ true };    // Factor in Block skill
    bool  bStaggerUseWeaponSkill{ false };  // Factor in weapon skill (One-Handed/Two-Handed)
    
    //==========================================================================
    // Camera Shake Settings - Impact feel
    //==========================================================================
    bool  bEnableCameraShake{ true };
    float fCameraShakeStrength{ 0.5f };    // Shake intensity
    float fCameraShakeDuration{ 0.2f };    // Shake duration in seconds
    
    //==========================================================================
    // Stamina Restoration - Restore stamina on successful timed block
    //==========================================================================
    bool  bEnableStaminaRestore{ true };   // Enabled by default
    float fStaminaRestorePercent{ 100.0f }; // Percentage of max stamina to restore (100 = full refill)
    
    //==========================================================================
    // Sound Settings
    //==========================================================================
    bool  bEnableSound{ true };
    std::string sSoundPath{ "UIMenuOK" };  // Sound descriptor EditorID
    bool  bUseCustomWav{ false };          // Use custom WAV file instead of sound descriptor
    float fCustomWavVolume{ 1.0f };        // Volume for custom WAV (0.0 - 1.0)
    
    //==========================================================================
    // Slowmo Settings - World slowdown on timed block
    //==========================================================================
    bool  bEnableSlowmo{ false };          // Disabled by default
    float fSlowmoSpeed{ 0.25f };           // 25% of normal game speed
    float fSlowmoDuration{ 0.75f };        // Duration in seconds (real time)
    
    //==========================================================================
    // Counter Attack Settings - Cancel block animation to attack
    //==========================================================================
    bool  bEnableCounterAttack{ false };   // Disabled by default
    float fCounterAttackWindow{ 0.5f };    // Window in seconds to perform counter attack
    bool  bPreventPlayerStagger{ true };   // Prevent player stagger on successful timed block
    
    //==========================================================================
    // Counter Attack Damage Bonus - Increase damage of counter hit
    //==========================================================================
    bool  bEnableCounterDamageBonus{ false };    // Disabled by default
    float fCounterDamageBonusPercent{ 50.0f };   // Damage bonus percentage (50 = +50% damage)
    float fCounterDamageBonusTimeout{ 1.0f };    // Timeout in seconds for damage bonus (if no hit landed)
    
    //==========================================================================
    // Counter Strike Sound - Sound when counter hit connects
    //==========================================================================
    bool  bEnableCounterStrikeSound{ true };     // Enabled by default
    float fCounterStrikeSoundVolume{ 1.0f };     // Volume 0.0 - 1.0
    
    //==========================================================================
    // Counter Attack Lunge Settings - Move toward attacker on counter
    //==========================================================================
    bool  bEnableCounterLunge{ false };     // Disabled by default
    float fCounterLungeDistance{ 150.0f };  // Max distance to lunge (game units)
    float fCounterLungeSpeed{ 800.0f };     // Speed of lunge (units/second)
    
    //==========================================================================
    // Counter Attack Slow Time Settings - Slow time during counter attack
    //==========================================================================
    bool  bEnableCounterSlowTime{ false };       // Disabled by default
    float fCounterSlowTimeScale{ 0.25f };        // Time scale (0.25 = 25% speed)
    float fCounterSlowTimeMaxDuration{ 2.0f };   // Max duration if end event not received
    bool  bCounterSlowStartAfterLunge{ false };  // Start slow time after lunge ends (instead of anim event)
    std::string sCounterSlowStartEvent{ "attackStart" };  // Event to start slow time (if not using lunge trigger)
    std::string sCounterSlowEndEvent{ "weaponSwing" };    // Event to end slow time
    
    //==========================================================================
    // Parry Window Settings - Controls the timed block window duration
    //==========================================================================
    float fParryWindowDurationMs{ 330.0f };     // Duration in milliseconds for the parry/timed block window (original mod default: 330ms)
    
    //==========================================================================
    // Timed Block Cooldown Settings
    //==========================================================================
    bool  bEnableCooldown{ true };              // Enable cooldown after failed timed block
    float fCooldownDurationMs{ 250.0f };        // Cooldown duration in milliseconds
    bool  bIgnoreCooldownOutsideRange{ false }; // Ignore cooldown if no enemy within range
    float fCooldownIgnoreDistance{ 512.0f };    // Distance threshold for cooldown ignore
    
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
