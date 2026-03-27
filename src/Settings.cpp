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
    fPowerAttackStaggerMagnitude = static_cast<float>(ini.GetDoubleValue("Stagger", "fPowerAttackStaggerMagnitude", 1.0));
    
    // Load skill-based stagger chance settings
    bUseStaggerChance = ini.GetBoolValue("Stagger", "bUseStaggerChance", false);
    fBaseStaggerChance = static_cast<float>(ini.GetDoubleValue("Stagger", "fBaseStaggerChance", 50.0));
    fMaxStaggerChance = static_cast<float>(ini.GetDoubleValue("Stagger", "fMaxStaggerChance", 100.0));
    bStaggerUseBlockSkill = ini.GetBoolValue("Stagger", "bStaggerUseBlockSkill", true);
    bStaggerUseWeaponSkill = ini.GetBoolValue("Stagger", "bStaggerUseWeaponSkill", false);
    
    // Load Camera Shake settings
    bEnableCameraShake = ini.GetBoolValue("CameraShake", "bEnableCameraShake", true);
    fCameraShakeStrength = static_cast<float>(ini.GetDoubleValue("CameraShake", "fCameraShakeStrength", 0.5));
    fCameraShakeDuration = static_cast<float>(ini.GetDoubleValue("CameraShake", "fCameraShakeDuration", 0.2));
    
    // Load Stamina Restoration settings
    bEnableStaminaRestore = ini.GetBoolValue("StaminaRestore", "bEnableStaminaRestore", true);
    fStaminaRestorePercent = static_cast<float>(ini.GetDoubleValue("StaminaRestore", "fStaminaRestorePercent", 100.0));
    
    // Load Sound settings
    bEnableSound = ini.GetBoolValue("Sound", "bEnableSound", true);
    sSoundPath = ini.GetValue("Sound", "sSoundPath", "UIMenuOK");
    bUseCustomWav = ini.GetBoolValue("Sound", "bUseCustomWav", false);
    fCustomWavVolume = static_cast<float>(ini.GetDoubleValue("Sound", "fCustomWavVolume", 1.0));
    
    // Load Slowmo settings
    bEnableSlowmo = ini.GetBoolValue("Slowmo", "bEnableSlowmo", false);
    fSlowmoSpeed = static_cast<float>(ini.GetDoubleValue("Slowmo", "fSlowmoSpeed", 0.25));
    fSlowmoDuration = static_cast<float>(ini.GetDoubleValue("Slowmo", "fSlowmoDuration", 0.75));
    
    // Load Counter Attack settings
    bEnableCounterAttack = ini.GetBoolValue("CounterAttack", "bEnableCounterAttack", false);
    fCounterAttackWindow = static_cast<float>(ini.GetDoubleValue("CounterAttack", "fCounterAttackWindow", 0.5));
    bPreventPlayerStagger = ini.GetBoolValue("CounterAttack", "bPreventPlayerStagger", true);
    
    // Load Counter Damage Bonus settings
    bEnableCounterDamageBonus = ini.GetBoolValue("CounterDamage", "bEnableCounterDamageBonus", false);
    fCounterDamageBonusPercent = static_cast<float>(ini.GetDoubleValue("CounterDamage", "fCounterDamageBonusPercent", 50.0));
    fCounterDamageBonusTimeout = static_cast<float>(ini.GetDoubleValue("CounterDamage", "fCounterDamageBonusTimeout", 1.0));
    
    // Load Counter Strike Sound settings
    bEnableCounterStrikeSound = ini.GetBoolValue("CounterDamage", "bEnableCounterStrikeSound", true);
    fCounterStrikeSoundVolume = static_cast<float>(ini.GetDoubleValue("CounterDamage", "fCounterStrikeSoundVolume", 1.0));
    
    // Load Counter Lunge settings
    bEnableCounterLunge = ini.GetBoolValue("CounterLunge", "bEnableCounterLunge", false);
    fCounterLungeDistance = static_cast<float>(ini.GetDoubleValue("CounterLunge", "fCounterLungeDistance", 150.0));
    fCounterLungeSpeed = static_cast<float>(ini.GetDoubleValue("CounterLunge", "fCounterLungeSpeed", 800.0));
    iCounterLungeCurve = static_cast<int>(ini.GetLongValue("CounterLunge", "iCurve", 0));
    fCounterLungeMeleeStopDistance = static_cast<float>(ini.GetDoubleValue("CounterLunge", "fMeleeStopDistance", 128.0));
    
    // Load Counter Slow Time settings
    bEnableCounterSlowTime = ini.GetBoolValue("CounterSlowTime", "bEnableCounterSlowTime", false);
    fCounterSlowTimeScale = static_cast<float>(ini.GetDoubleValue("CounterSlowTime", "fCounterSlowTimeScale", 0.25));
    fCounterSlowTimeMaxDuration = static_cast<float>(ini.GetDoubleValue("CounterSlowTime", "fCounterSlowTimeMaxDuration", 2.0));
    bCounterSlowStartAfterLunge = ini.GetBoolValue("CounterSlowTime", "bCounterSlowStartAfterLunge", false);
    sCounterSlowStartEvent = ini.GetValue("CounterSlowTime", "sCounterSlowStartEvent", "attackStart");
    sCounterSlowEndEvent = ini.GetValue("CounterSlowTime", "sCounterSlowEndEvent", "weaponSwing");
    
    // Load Parry Window settings (try ParryWindow section first, then root for compatibility)
    // Original mod default taper duration is 0.33s = 330ms
    fParryWindowDurationMs = static_cast<float>(ini.GetDoubleValue("ParryWindow", "fParryWindowDurationMs", 
        ini.GetDoubleValue("", "fParryWindowDurationMs", 330.0)));
    
    // Load Cooldown settings
    bEnableCooldown = ini.GetBoolValue("Cooldown", "bEnableCooldown", true);
    fCooldownDurationMs = static_cast<float>(ini.GetDoubleValue("Cooldown", "fCooldownDurationMs", 250.0));
    bIgnoreCooldownOutsideRange = ini.GetBoolValue("Cooldown", "bIgnoreCooldownOutsideRange", false);
    fCooldownIgnoreDistance = static_cast<float>(ini.GetDoubleValue("Cooldown", "fCooldownIgnoreDistance", 512.0));
    
    // Load Timed Dodge settings
    bEnableTimedDodge = ini.GetBoolValue("TimedDodge", "bEnableTimedDodge", true);
    fTimedDodgeSlomoDuration = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fSlomoDuration", 4.0));
    fTimedDodgeSlomoSpeed = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fSlomoSpeed", 0.05));
    fTimedDodgeCooldown = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fCooldown", 3.0));
    fTimedDodgeDetectionRange = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fDetectionRange", 300.0));
    fTimedDodgeForgivenessMs = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fForgivenessMs", 200.0));
    bTimedDodgeIframes = ini.GetBoolValue("TimedDodge", "bIframes", true);
    bTimedDodgeCounterAttack = ini.GetBoolValue("TimedDodge", "bCounterAttack", true);
    fTimedDodgeCounterWindowMs = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fCounterWindowMs", 2000.0));
    fTimedDodgeCounterDamagePercent = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fCounterDamagePercent", 50.0));
    fTimedDodgeCounterDamageTimeout = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fCounterDamageTimeout", 3.0));
    bTimedDodgeCounterLunge = ini.GetBoolValue("TimedDodge", "bCounterLunge", true);
    fTimedDodgeCounterLungeSpeed = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fCounterLungeSpeed", 800.0));
    iTimedDodgeCounterLungeCurve = static_cast<int>(ini.GetLongValue("TimedDodge", "iCounterLungeCurve", 0));
    fTimedDodgeCounterLungeMeleeStopDistance = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fCounterLungeMeleeStop", 128.0));
    bTimedDodgeRadialBlur = ini.GetBoolValue("TimedDodge", "bRadialBlur", true);
    fTimedDodgeBlurStrength = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fBlurStrength", 0.3));
    fTimedDodgeBlurBlendSpeed = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fBlurBlendSpeed", 5.0));
    fTimedDodgeBlurRampUp = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fBlurRampUp", 0.1));
    fTimedDodgeBlurRampDown = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fBlurRampDown", 0.2));
    fTimedDodgeBlurRadius = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fBlurRadius", 0.4));
    bTimedDodgeApplyBlockEffects = ini.GetBoolValue("TimedDodge", "bApplyBlockEffects", true);
    bTimedDodgeStagger = ini.GetBoolValue("TimedDodge", "bStagger", false);
    bTimedDodgeAttackerSlow = ini.GetBoolValue("TimedDodge", "bAttackerSlow", true);
    fTimedDodgeAttackerSlowSpeed = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fAttackerSlowSpeed", 0.05));
    fTimedDodgeAttackerSlowDuration = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fAttackerSlowDuration", 1.5));
    bTimedDodgeSound = ini.GetBoolValue("TimedDodge", "bSound", true);
    fTimedDodgeSoundVolume = static_cast<float>(ini.GetDoubleValue("TimedDodge", "fSoundVolume", 1.0));

    // Debug logging
    bDebugLogging = ini.GetBoolValue("Log", "Debug", false);
    
    if (bDebugLogging) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::debug);  // Flush on debug too
        logger::info("=== DEBUG LOGGING ENABLED ===");
    } else {
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);
    }
    
    // Clamp values
    fParryWindowDurationMs = std::clamp(fParryWindowDurationMs, 50.0f, 1000.0f);
    fCooldownDurationMs = std::clamp(fCooldownDurationMs, 0.0f, 1000.0f);
    fTimedDodgeCounterWindowMs = std::clamp(fTimedDodgeCounterWindowMs, 50.0f, 30000.0f);
    fCounterLungeMeleeStopDistance = std::clamp(fCounterLungeMeleeStopDistance, 32.0f, 512.0f);
    fTimedDodgeCounterLungeSpeed = std::clamp(fTimedDodgeCounterLungeSpeed, 200.0f, 2000.0f);
    fTimedDodgeCounterLungeMeleeStopDistance = std::clamp(fTimedDodgeCounterLungeMeleeStopDistance, 32.0f, 512.0f);
    iCounterLungeCurve = std::clamp(iCounterLungeCurve, 0, 5);
    iTimedDodgeCounterLungeCurve = std::clamp(iTimedDodgeCounterLungeCurve, 0, 5);
    
    logger::info("Settings loaded successfully");
    logger::info("  ParryWindow: {}ms", fParryWindowDurationMs);
    logger::info("  Hitstop: {} (speed={}, duration={}s)", bEnableHitstop, fHitstopSpeed, fHitstopDuration);
    logger::info("  Stagger: {} (normal={}, power={})", bEnableStagger, fStaggerMagnitude, fPowerAttackStaggerMagnitude);
    logger::info("  CameraShake: {} (strength={}, duration={}s)", bEnableCameraShake, fCameraShakeStrength, fCameraShakeDuration);
    logger::info("  Sound: {} (customWav={}, volume={}%, path={})", bEnableSound, bUseCustomWav, fCustomWavVolume * 100.0f, sSoundPath);
    logger::info("  Slowmo: {} (speed={}, duration={}s)", bEnableSlowmo, fSlowmoSpeed, fSlowmoDuration);
    logger::info("  CounterAttack: {} (window={}s, preventStagger={})", bEnableCounterAttack, fCounterAttackWindow, bPreventPlayerStagger);
    logger::info("  CounterDamage: {} (bonus={}%, timeout={}s)", bEnableCounterDamageBonus, fCounterDamageBonusPercent, fCounterDamageBonusTimeout);
    logger::info("  CounterLunge: {} (maxDist={}, speed={}, meleeStop={})",
        bEnableCounterLunge, fCounterLungeDistance, fCounterLungeSpeed, fCounterLungeMeleeStopDistance);
    logger::info("  CounterSlowTime: {} (scale={}, maxDuration={}s, start='{}', end='{}')", 
        bEnableCounterSlowTime, fCounterSlowTimeScale, fCounterSlowTimeMaxDuration, sCounterSlowStartEvent, sCounterSlowEndEvent);
    logger::info("  Cooldown: {} (duration={}ms, ignoreOutsideRange={}, distance={})", 
        bEnableCooldown, fCooldownDurationMs, bIgnoreCooldownOutsideRange, fCooldownIgnoreDistance);
    logger::info("  TimedDodge: {} (slomo={}s@{}%, cooldown={}s, range={}, iframes={}, counter={}, blur={})", 
        bEnableTimedDodge, fTimedDodgeSlomoDuration, fTimedDodgeSlomoSpeed * 100.0f, 
        fTimedDodgeCooldown, fTimedDodgeDetectionRange, bTimedDodgeIframes, 
        bTimedDodgeCounterAttack, bTimedDodgeRadialBlur);
    logger::info("    Forgiveness: {}ms, Stagger: {}, AttackerSlow: {} (speed={}, dur={}s)",
        fTimedDodgeForgivenessMs, bTimedDodgeStagger, bTimedDodgeAttackerSlow,
        fTimedDodgeAttackerSlowSpeed, fTimedDodgeAttackerSlowDuration);
    logger::info("    CounterWindow: {:.0f}ms, CounterDmg: +{}% (timeout={}s), CounterLunge: {} (speed={} u/s), Sound: {} (vol={}%)",
        fTimedDodgeCounterWindowMs, fTimedDodgeCounterDamagePercent, fTimedDodgeCounterDamageTimeout,
        bTimedDodgeCounterLunge, fTimedDodgeCounterLungeSpeed, bTimedDodgeSound, fTimedDodgeSoundVolume * 100.0f);
    logger::info("  Debug: {}", bDebugLogging);
}

void Settings::SaveSettings() {
    logger::info("Saving settings to: {}", INI_PATH);
    
    // Log current values being saved
    logger::info("  Saving - Sound: enabled={}, customWav={}, path={}", bEnableSound, bUseCustomWav, sSoundPath);
    logger::info("  Saving - Slowmo: enabled={}, speed={}, duration={}", bEnableSlowmo, fSlowmoSpeed, fSlowmoDuration);
    logger::info("  Saving - Cooldown: enabled={}, duration={}ms, ignoreRange={}, distance={}", 
        bEnableCooldown, fCooldownDurationMs, bIgnoreCooldownOutsideRange, fCooldownIgnoreDistance);
    
    CSimpleIniA ini;
    ini.SetUnicode();
    
    // Load existing settings first to preserve them
    SI_Error loadRc = ini.LoadFile(INI_PATH);
    if (loadRc < 0) {
        logger::info("INI file doesn't exist yet, will create new one");
    }
    
    // Save original settings (root section - for SimpleTimedBlock.esp compatibility)
    ini.SetValue("", "sBlockKey", sBlockKey.c_str());
    ini.SetDoubleValue("", "fStaggerDistance", fStaggerDistance);
    ini.SetBoolValue("", "bPerkLockedBlock", bPerkLockedBlock);
    ini.SetBoolValue("", "bOnlyWithShield", bOnlyWithShield);
    ini.SetBoolValue("", "bPerkLockedStagger", bPerkLockedStagger);
    ini.SetDoubleValue("", "fParryWindowDurationMs", fParryWindowDurationMs);  // Also save to root for original mod
    
    // Save Hitstop settings
    ini.SetBoolValue("Hitstop", "bEnableHitstop", bEnableHitstop);
    ini.SetDoubleValue("Hitstop", "fHitstopSpeed", fHitstopSpeed);
    ini.SetDoubleValue("Hitstop", "fHitstopDuration", fHitstopDuration);
    
    // Save Stagger settings
    ini.SetBoolValue("Stagger", "bEnableStagger", bEnableStagger);
    ini.SetDoubleValue("Stagger", "fStaggerMagnitude", fStaggerMagnitude);
    ini.SetDoubleValue("Stagger", "fPowerAttackStaggerMagnitude", fPowerAttackStaggerMagnitude);
    
    // Save skill-based stagger chance settings
    ini.SetBoolValue("Stagger", "bUseStaggerChance", bUseStaggerChance);
    ini.SetDoubleValue("Stagger", "fBaseStaggerChance", fBaseStaggerChance);
    ini.SetDoubleValue("Stagger", "fMaxStaggerChance", fMaxStaggerChance);
    ini.SetBoolValue("Stagger", "bStaggerUseBlockSkill", bStaggerUseBlockSkill);
    ini.SetBoolValue("Stagger", "bStaggerUseWeaponSkill", bStaggerUseWeaponSkill);
    
    // Save Camera Shake settings
    ini.SetBoolValue("CameraShake", "bEnableCameraShake", bEnableCameraShake);
    ini.SetDoubleValue("CameraShake", "fCameraShakeStrength", fCameraShakeStrength);
    ini.SetDoubleValue("CameraShake", "fCameraShakeDuration", fCameraShakeDuration);
    
    // Save Stamina Restoration settings
    ini.SetBoolValue("StaminaRestore", "bEnableStaminaRestore", bEnableStaminaRestore);
    ini.SetDoubleValue("StaminaRestore", "fStaminaRestorePercent", fStaminaRestorePercent);
    
    // Save Sound settings
    ini.SetBoolValue("Sound", "bEnableSound", bEnableSound);
    ini.SetValue("Sound", "sSoundPath", sSoundPath.c_str());
    ini.SetBoolValue("Sound", "bUseCustomWav", bUseCustomWav);
    ini.SetDoubleValue("Sound", "fCustomWavVolume", fCustomWavVolume);
    
    // Save Slowmo settings
    ini.SetBoolValue("Slowmo", "bEnableSlowmo", bEnableSlowmo);
    ini.SetDoubleValue("Slowmo", "fSlowmoSpeed", fSlowmoSpeed);
    ini.SetDoubleValue("Slowmo", "fSlowmoDuration", fSlowmoDuration);
    
    // Save Counter Attack settings
    ini.SetBoolValue("CounterAttack", "bEnableCounterAttack", bEnableCounterAttack);
    ini.SetDoubleValue("CounterAttack", "fCounterAttackWindow", fCounterAttackWindow);
    ini.SetBoolValue("CounterAttack", "bPreventPlayerStagger", bPreventPlayerStagger);
    
    // Save Counter Damage Bonus settings
    ini.SetBoolValue("CounterDamage", "bEnableCounterDamageBonus", bEnableCounterDamageBonus);
    ini.SetDoubleValue("CounterDamage", "fCounterDamageBonusPercent", fCounterDamageBonusPercent);
    ini.SetDoubleValue("CounterDamage", "fCounterDamageBonusTimeout", fCounterDamageBonusTimeout);
    
    // Save Counter Strike Sound settings
    ini.SetBoolValue("CounterDamage", "bEnableCounterStrikeSound", bEnableCounterStrikeSound);
    ini.SetDoubleValue("CounterDamage", "fCounterStrikeSoundVolume", fCounterStrikeSoundVolume);
    
    // Save Counter Lunge settings
    ini.SetBoolValue("CounterLunge", "bEnableCounterLunge", bEnableCounterLunge);
    ini.SetDoubleValue("CounterLunge", "fCounterLungeDistance", fCounterLungeDistance);
    ini.SetDoubleValue("CounterLunge", "fCounterLungeSpeed", fCounterLungeSpeed);
    ini.SetLongValue("CounterLunge", "iCurve", iCounterLungeCurve);
    ini.SetDoubleValue("CounterLunge", "fMeleeStopDistance", fCounterLungeMeleeStopDistance);
    
    // Save Counter Slow Time settings
    ini.SetBoolValue("CounterSlowTime", "bEnableCounterSlowTime", bEnableCounterSlowTime);
    ini.SetDoubleValue("CounterSlowTime", "fCounterSlowTimeScale", fCounterSlowTimeScale);
    ini.SetDoubleValue("CounterSlowTime", "fCounterSlowTimeMaxDuration", fCounterSlowTimeMaxDuration);
    ini.SetBoolValue("CounterSlowTime", "bCounterSlowStartAfterLunge", bCounterSlowStartAfterLunge);
    ini.SetValue("CounterSlowTime", "sCounterSlowStartEvent", sCounterSlowStartEvent.c_str());
    ini.SetValue("CounterSlowTime", "sCounterSlowEndEvent", sCounterSlowEndEvent.c_str());
    
    // Save Parry Window settings
    ini.SetDoubleValue("ParryWindow", "fParryWindowDurationMs", fParryWindowDurationMs);
    
    // Save Cooldown settings
    ini.SetBoolValue("Cooldown", "bEnableCooldown", bEnableCooldown);
    ini.SetDoubleValue("Cooldown", "fCooldownDurationMs", fCooldownDurationMs);
    ini.SetBoolValue("Cooldown", "bIgnoreCooldownOutsideRange", bIgnoreCooldownOutsideRange);
    ini.SetDoubleValue("Cooldown", "fCooldownIgnoreDistance", fCooldownIgnoreDistance);
    
    // Save Timed Dodge settings
    ini.SetBoolValue("TimedDodge", "bEnableTimedDodge", bEnableTimedDodge);
    ini.SetDoubleValue("TimedDodge", "fSlomoDuration", fTimedDodgeSlomoDuration);
    ini.SetDoubleValue("TimedDodge", "fSlomoSpeed", fTimedDodgeSlomoSpeed);
    ini.SetDoubleValue("TimedDodge", "fCooldown", fTimedDodgeCooldown);
    ini.SetDoubleValue("TimedDodge", "fDetectionRange", fTimedDodgeDetectionRange);
    ini.SetDoubleValue("TimedDodge", "fForgivenessMs", fTimedDodgeForgivenessMs);
    ini.SetBoolValue("TimedDodge", "bIframes", bTimedDodgeIframes);
    ini.SetBoolValue("TimedDodge", "bCounterAttack", bTimedDodgeCounterAttack);
    ini.SetDoubleValue("TimedDodge", "fCounterWindowMs", fTimedDodgeCounterWindowMs);
    ini.SetDoubleValue("TimedDodge", "fCounterDamagePercent", fTimedDodgeCounterDamagePercent);
    ini.SetDoubleValue("TimedDodge", "fCounterDamageTimeout", fTimedDodgeCounterDamageTimeout);
    ini.SetBoolValue("TimedDodge", "bCounterLunge", bTimedDodgeCounterLunge);
    ini.SetDoubleValue("TimedDodge", "fCounterLungeSpeed", fTimedDodgeCounterLungeSpeed);
    ini.SetLongValue("TimedDodge", "iCounterLungeCurve", iTimedDodgeCounterLungeCurve);
    ini.SetDoubleValue("TimedDodge", "fCounterLungeMeleeStop", fTimedDodgeCounterLungeMeleeStopDistance);
    ini.SetBoolValue("TimedDodge", "bRadialBlur", bTimedDodgeRadialBlur);
    ini.SetDoubleValue("TimedDodge", "fBlurStrength", fTimedDodgeBlurStrength);
    ini.SetDoubleValue("TimedDodge", "fBlurBlendSpeed", fTimedDodgeBlurBlendSpeed);
    ini.SetDoubleValue("TimedDodge", "fBlurRampUp", fTimedDodgeBlurRampUp);
    ini.SetDoubleValue("TimedDodge", "fBlurRampDown", fTimedDodgeBlurRampDown);
    ini.SetDoubleValue("TimedDodge", "fBlurRadius", fTimedDodgeBlurRadius);
    ini.SetBoolValue("TimedDodge", "bApplyBlockEffects", bTimedDodgeApplyBlockEffects);
    ini.SetBoolValue("TimedDodge", "bStagger", bTimedDodgeStagger);
    ini.SetBoolValue("TimedDodge", "bAttackerSlow", bTimedDodgeAttackerSlow);
    ini.SetDoubleValue("TimedDodge", "fAttackerSlowSpeed", fTimedDodgeAttackerSlowSpeed);
    ini.SetDoubleValue("TimedDodge", "fAttackerSlowDuration", fTimedDodgeAttackerSlowDuration);
    ini.SetBoolValue("TimedDodge", "bSound", bTimedDodgeSound);
    ini.SetDoubleValue("TimedDodge", "fSoundVolume", fTimedDodgeSoundVolume);

    // Save debug
    ini.SetBoolValue("Log", "Debug", bDebugLogging);
    
    // Preserve Forms section if it exists
    const char* perkModName = ini.GetValue("Forms", "PerkModName", "");
    const char* timedBlockPerk = ini.GetValue("Forms", "TimedBlockPerk", "");
    if (perkModName && strlen(perkModName) > 0) {
        ini.SetValue("Forms", "PerkModName", perkModName);
    }
    if (timedBlockPerk && strlen(timedBlockPerk) > 0) {
        ini.SetValue("Forms", "TimedBlockPerk", timedBlockPerk);
    }
    
    SI_Error rc = ini.SaveFile(INI_PATH);
    if (rc < 0) {
        logger::error("Failed to save INI file! Error code: {}", static_cast<int>(rc));
        return;
    }
    
    logger::info("Settings saved successfully to {}", INI_PATH);
}
