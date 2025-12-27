#include "Settings.h"

Settings* Settings::GetSingleton() {
    static Settings singleton;
    return &singleton;
}

void Settings::LoadSettings() {
    logger::info("Loading settings...");
    
    CSimpleIniA ini;
    ini.SetUnicode();
    
    SI_Error rc = ini.LoadFile(INI_PATH);
    if (rc < 0) {
        logger::warn("Failed to load INI file, using defaults");
        SaveSettings(); // Create default INI
        return;
    }
    
    // Load original settings (for compatibility)
    sBlockKey = ini.GetValue("", "sBlockKey", "V");
    fStaggerDistance = static_cast<float>(ini.GetDoubleValue("", "fStaggerDistance", 128.0));
    bPerkLockedBlock = ini.GetBoolValue("", "bPerkLockedBlock", false);
    bOnlyWithShield = ini.GetBoolValue("", "bOnlyWithShield", false);
    bPerkLockedStagger = ini.GetBoolValue("", "bPerkLockedStagger", false);
    
    // Load Hitstop settings (freezes attacker only, not the world!)
    bEnableHitstop = ini.GetBoolValue("Hitstop", "bEnableHitstop", true);
    fHitstopSpeed = static_cast<float>(ini.GetDoubleValue("Hitstop", "fHitstopSpeed", 0.05));
    fHitstopDuration = static_cast<float>(ini.GetDoubleValue("Hitstop", "fHitstopDuration", 0.15));
    
    // Load Stagger settings
    bEnableStagger = ini.GetBoolValue("Stagger", "bEnableStagger", true);
    fStaggerMagnitude = static_cast<float>(ini.GetDoubleValue("Stagger", "fStaggerMagnitude", 0.7));
    
    // Load Pushback settings
    bEnablePushback = ini.GetBoolValue("Pushback", "bEnablePushback", false);
    fPushbackMagnitude = static_cast<float>(ini.GetDoubleValue("Pushback", "fPushbackMagnitude", 1.5));
    
    // Load Camera Shake settings
    bEnableCameraShake = ini.GetBoolValue("CameraShake", "bEnableCameraShake", true);
    fCameraShakeStrength = static_cast<float>(ini.GetDoubleValue("CameraShake", "fCameraShakeStrength", 0.5));
    fCameraShakeDuration = static_cast<float>(ini.GetDoubleValue("CameraShake", "fCameraShakeDuration", 0.2));
    
    // Load Sound settings
    bEnableSound = ini.GetBoolValue("Sound", "bEnableSound", true);
    sSoundPath = ini.GetValue("Sound", "sSoundPath", "UIMenuOK");
    
    // Load Cooldown settings
    bEnableCooldown = ini.GetBoolValue("Cooldown", "bEnableCooldown", true);
    fCooldownDurationMs = static_cast<float>(ini.GetDoubleValue("Cooldown", "fCooldownDurationMs", 250.0));
    
    // Debug logging
    bDebugLogging = ini.GetBoolValue("Log", "Debug", false);
    
    if (bDebugLogging) {
        spdlog::set_level(spdlog::level::debug);
        logger::debug("Debug logging enabled");
    }
    
    // Clamp values
    fCooldownDurationMs = std::clamp(fCooldownDurationMs, 0.0f, 1000.0f);
    
    logger::info("Settings loaded successfully");
    logger::info("  Hitstop: {} (speed={}, duration={}s)", bEnableHitstop, fHitstopSpeed, fHitstopDuration);
    logger::info("  Stagger: {} (magnitude={})", bEnableStagger, fStaggerMagnitude);
    logger::info("  Pushback: {} (magnitude={})", bEnablePushback, fPushbackMagnitude);
    logger::info("  CameraShake: {} (strength={}, duration={}s)", bEnableCameraShake, fCameraShakeStrength, fCameraShakeDuration);
    logger::info("  Sound: {} ({})", bEnableSound, sSoundPath);
    logger::info("  Cooldown: {} (duration={}ms)", bEnableCooldown, fCooldownDurationMs);
}

void Settings::SaveSettings() {
    logger::info("Saving settings...");
    
    CSimpleIniA ini;
    ini.SetUnicode();
    
    // Load existing settings first to preserve them
    ini.LoadFile(INI_PATH);
    
    // Save original settings
    ini.SetValue("", "sBlockKey", sBlockKey.c_str());
    ini.SetDoubleValue("", "fStaggerDistance", fStaggerDistance);
    ini.SetBoolValue("", "bPerkLockedBlock", bPerkLockedBlock);
    ini.SetBoolValue("", "bOnlyWithShield", bOnlyWithShield);
    ini.SetBoolValue("", "bPerkLockedStagger", bPerkLockedStagger);
    
    // Save Hitstop settings
    ini.SetBoolValue("Hitstop", "bEnableHitstop", bEnableHitstop);
    ini.SetDoubleValue("Hitstop", "fHitstopSpeed", fHitstopSpeed);
    ini.SetDoubleValue("Hitstop", "fHitstopDuration", fHitstopDuration);
    
    // Save Stagger settings
    ini.SetBoolValue("Stagger", "bEnableStagger", bEnableStagger);
    ini.SetDoubleValue("Stagger", "fStaggerMagnitude", fStaggerMagnitude);
    
    // Save Pushback settings
    ini.SetBoolValue("Pushback", "bEnablePushback", bEnablePushback);
    ini.SetDoubleValue("Pushback", "fPushbackMagnitude", fPushbackMagnitude);
    
    // Save Camera Shake settings
    ini.SetBoolValue("CameraShake", "bEnableCameraShake", bEnableCameraShake);
    ini.SetDoubleValue("CameraShake", "fCameraShakeStrength", fCameraShakeStrength);
    ini.SetDoubleValue("CameraShake", "fCameraShakeDuration", fCameraShakeDuration);
    
    // Save Sound settings
    ini.SetBoolValue("Sound", "bEnableSound", bEnableSound);
    ini.SetValue("Sound", "sSoundPath", sSoundPath.c_str());
    
    // Save Cooldown settings
    ini.SetBoolValue("Cooldown", "bEnableCooldown", bEnableCooldown);
    ini.SetDoubleValue("Cooldown", "fCooldownDurationMs", fCooldownDurationMs);
    
    // Save debug
    ini.SetBoolValue("Log", "Debug", bDebugLogging);
    
    // Preserve Forms section if it exists
    const char* perkModName = ini.GetValue("Forms", "PerkModName", "");
    const char* timedBlockPerk = ini.GetValue("Forms", "TimedBlockPerk", "");
    ini.SetValue("Forms", "PerkModName", perkModName);
    ini.SetValue("Forms", "TimedBlockPerk", timedBlockPerk);
    
    SI_Error rc = ini.SaveFile(INI_PATH);
    if (rc < 0) {
        logger::error("Failed to save INI file!");
        return;
    }
    
    logger::info("Settings saved successfully");
}
