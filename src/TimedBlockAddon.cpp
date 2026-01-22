#include "TimedBlockAddon.h"
#include "Settings.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <cmath>
#include <random>

// Windows headers for custom WAV playback
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

// Undefine Windows PlaySound macro to avoid conflict with RE::PlaySound
#ifdef PlaySound
#undef PlaySound
#endif

// Alias for Windows PlaySound
#define WindowsPlaySound ::PlaySoundW

//=============================================================================
// RNG Utility - Random number generation for chance-based mechanics
//=============================================================================
namespace RNG
{
    inline std::mt19937& GetEngine()
    {
        static std::random_device rd;
        static std::mt19937 engine(rd());
        return engine;
    }
    
    // Returns a random value between 0.0 and 100.0
    inline float GetRandom()
    {
        static std::uniform_real_distribution<float> dist(0.0f, 100.0f);
        return dist(GetEngine());
    }
}

//=============================================================================
// Skill-Based Stagger Chance
//=============================================================================

// Get the weapon skill (One-Handed or Two-Handed) based on equipped weapon
RE::ActorValue GetWeaponSkillForActor(RE::Actor* a_actor)
{
    if (!a_actor) {
        return RE::ActorValue::kOneHanded;  // Default
    }
    
    // Check right hand first (primary weapon)
    auto* equippedRight = a_actor->GetEquippedObject(false);
    if (equippedRight && equippedRight->IsWeapon()) {
        auto* weapon = equippedRight->As<RE::TESObjectWEAP>();
        if (weapon) {
            RE::WEAPON_TYPE weaponType = weapon->GetWeaponType();
            switch (weaponType) {
                case RE::WEAPON_TYPE::kTwoHandSword:
                case RE::WEAPON_TYPE::kTwoHandAxe:
                    return RE::ActorValue::kTwoHanded;
                case RE::WEAPON_TYPE::kBow:
                case RE::WEAPON_TYPE::kCrossbow:
                    return RE::ActorValue::kArchery;
                default:
                    return RE::ActorValue::kOneHanded;
            }
        }
    }
    
    // Check left hand (for dual wielding or if right hand empty)
    auto* equippedLeft = a_actor->GetEquippedObject(true);
    if (equippedLeft && equippedLeft->IsWeapon()) {
        auto* weapon = equippedLeft->As<RE::TESObjectWEAP>();
        if (weapon) {
            RE::WEAPON_TYPE weaponType = weapon->GetWeaponType();
            switch (weaponType) {
                case RE::WEAPON_TYPE::kTwoHandSword:
                case RE::WEAPON_TYPE::kTwoHandAxe:
                    return RE::ActorValue::kTwoHanded;
                default:
                    return RE::ActorValue::kOneHanded;
            }
        }
    }
    
    // Default to one-handed (unarmed uses this)
    return RE::ActorValue::kOneHanded;
}

// Calculate the stagger chance based on player skills
float CalculateStaggerChance(RE::Actor* a_player)
{
    auto* settings = Settings::GetSingleton();
    
    if (!settings->bUseStaggerChance) {
        return 100.0f;  // Guaranteed stagger if skill-based chance is disabled
    }
    
    if (!a_player) {
        return settings->fBaseStaggerChance;
    }
    
    auto* avOwner = a_player->AsActorValueOwner();
    if (!avOwner) {
        return settings->fBaseStaggerChance;
    }
    
    float totalSkill = 0.0f;
    int skillCount = 0;
    
    // Factor in Block skill if enabled
    if (settings->bStaggerUseBlockSkill) {
        float blockSkill = avOwner->GetActorValue(RE::ActorValue::kBlock);
        blockSkill = std::clamp(blockSkill, 0.0f, 100.0f);
        totalSkill += blockSkill;
        skillCount++;
    }
    
    // Factor in weapon skill if enabled
    if (settings->bStaggerUseWeaponSkill) {
        RE::ActorValue weaponAV = GetWeaponSkillForActor(a_player);
        float weaponSkill = avOwner->GetActorValue(weaponAV);
        weaponSkill = std::clamp(weaponSkill, 0.0f, 100.0f);
        totalSkill += weaponSkill;
        skillCount++;
    }
    
    // If no skills are being used, return base chance
    if (skillCount == 0) {
        return settings->fBaseStaggerChance;
    }
    
    // Average the skills
    float averageSkill = totalSkill / static_cast<float>(skillCount);
    float skillRatio = averageSkill / 100.0f;
    
    // Linear interpolation between base and max chance
    float chance = settings->fBaseStaggerChance + 
                   (settings->fMaxStaggerChance - settings->fBaseStaggerChance) * skillRatio;
    
    return std::clamp(chance, 0.0f, 100.0f);
}

// Roll for stagger success based on skill
bool RollStaggerSuccess(RE::Actor* a_player)
{
    auto* settings = Settings::GetSingleton();
    
    if (!settings->bUseStaggerChance) {
        return true;  // Always stagger if skill-based chance is disabled
    }
    
    float chance = CalculateStaggerChance(a_player);
    float roll = RNG::GetRandom();
    bool success = roll <= chance;
    
    if (settings->bDebugLogging) {
        logger::info("[STAGGER CHANCE] Skill-based roll: {:.1f}% chance, rolled {:.1f}, {}", 
            chance, roll, success ? "SUCCESS" : "FAILED");
        
        if (!success) {
            RE::DebugNotification(fmt::format("[TB] Stagger failed ({:.0f}%)", chance).c_str());
        }
    }
    
    return success;
}

//=============================================================================
// AnimSpeedManager Implementation - Per-Actor Animation Freezing
//=============================================================================

void AnimSpeedManager::SetAnimSpeed(RE::ActorHandle a_actorHandle, float a_speedMult, float a_duration)
{
    std::lock_guard<std::mutex> lock(_animSpeedsLock);
    auto it = _animSpeeds.find(a_actorHandle);
    if (it != _animSpeeds.end()) {
        it->second.speedMult = a_speedMult;
        it->second.duration = a_duration;
    } else {
        _animSpeeds.emplace(a_actorHandle, AnimSpeedData{ a_speedMult, a_duration });
    }
}

void AnimSpeedManager::RevertAnimSpeed(RE::ActorHandle a_actorHandle)
{
    std::lock_guard<std::mutex> lock(_animSpeedsLock);
    auto it = _animSpeeds.find(a_actorHandle);
    if (it != _animSpeeds.end()) {
        it->second.speedMult = 1.0f;
        it->second.duration = 0.0f;
    }
}

void AnimSpeedManager::RevertAllAnimSpeed()
{
    std::lock_guard<std::mutex> lock(_animSpeedsLock);
    _animSpeeds.clear();
}

void AnimSpeedManager::Update(RE::ActorHandle a_actorHandle, float& a_deltaTime)
{
    if (a_deltaTime <= 0.f) {
        return;
    }
    
    // Fast path: if no actors have modified speeds, skip the lock entirely
    // This is the common case during normal gameplay (no active hitstop effects)
    // Reading empty() is safe without lock - worst case we miss one frame which is imperceptible
    if (_animSpeeds.empty()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(_animSpeedsLock);
    auto it = _animSpeeds.find(a_actorHandle);
    if (it != _animSpeeds.end()) {
        float newDuration = it->second.duration - a_deltaTime;
        float speedMult = it->second.speedMult;
        
        if (newDuration <= 0.f) {
            // Calculate partial frame for smooth transition out
            float mult = (a_deltaTime + newDuration) / a_deltaTime;
            a_deltaTime *= speedMult + ((1.f - speedMult) * (1.f - mult));
            _animSpeeds.erase(it);
        } else {
            it->second.duration = newDuration;
            // Apply speed multiplier
            a_deltaTime *= speedMult;
        }
    }
}

void AnimSpeedManager::Init()
{
    PlayerAnimationHook::Install();
    NPCAnimationHook::Install();
    logger::info("AnimSpeedManager initialized");
}

void AnimSpeedManager::PlayerAnimationHook::Install()
{
    REL::Relocation<std::uintptr_t> PlayerCharacterVtbl{ RE::VTABLE_PlayerCharacter[0] };
    _originalFunc = PlayerCharacterVtbl.write_vfunc(0x7D, PlayerCharacter_UpdateAnimation);
    logger::info("Installed PlayerAnimationHook");
}

void AnimSpeedManager::PlayerAnimationHook::PlayerCharacter_UpdateAnimation(RE::PlayerCharacter* a_this, float a_deltaTime)
{
    // Update cooldown state tracking for timed block
    CooldownState::Update();
    
    // Update counter attack window timer
    CounterAttackState::Update();
    
    // Note: Lunge movement is handled by the physics hook (LungePhysicsHook)
    // for proper Havok physics integration
    
    // Update counter slow time (check for timeout)
    CounterSlowTimeState::Update();
    
    AnimSpeedManager::Update(a_this->GetHandle(), a_deltaTime);
    _originalFunc(a_this, a_deltaTime);
}

void AnimSpeedManager::NPCAnimationHook::Install()
{
    auto& trampoline = SKSE::GetTrampoline();
    REL::Relocation<uintptr_t> hook{ RELOCATION_ID(40436, 41453) };  // RunOneActorAnimationUpdateJob
    _originalFunc = trampoline.write_call<5>(hook.address() + RELOCATION_OFFSET(0x74, 0x74), UpdateAnimationInternal);
    logger::info("Installed NPCAnimationHook");
}

void AnimSpeedManager::NPCAnimationHook::UpdateAnimationInternal(RE::Actor* a_this, float a_deltaTime)
{
    AnimSpeedManager::Update(a_this->GetHandle(), a_deltaTime);
    _originalFunc(a_this, a_deltaTime);
}

//=============================================================================
// CooldownState Implementation - Tracks parry window effect transitions
//=============================================================================

bool CooldownState::PlayerHasParryWindowEffect()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return false;
    }
    
    // Get the parry window effect from the TimedBlockAddon
    auto* addon = TimedBlockAddon::GetSingleton();
    if (!addon) {
        return false;
    }
    
    return addon->ActorHasParryWindowEffect(player);
}

bool CooldownState::GetParryEffectCached()
{
    // Only check once per frame - subsequent calls return cached result
    if (!parryEffectCachedThisFrame) {
        cachedParryEffectResult = PlayerHasParryWindowEffect();
        parryEffectCachedThisFrame = true;
    }
    return cachedParryEffectResult;
}

bool CooldownState::GetNearbyEnemyCached()
{
    auto* settings = Settings::GetSingleton();
    if (!settings->bIgnoreCooldownOutsideRange || settings->fCooldownIgnoreDistance <= 0.0f) {
        return true;  // Feature disabled, assume enemies nearby
    }
    
    auto now = std::chrono::steady_clock::now();
    if (now - lastEnemyCheckTime >= ENEMY_CHECK_INTERVAL) {
        lastEnemyCheckTime = now;
        
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* addon = TimedBlockAddon::GetSingleton();
        if (player && addon) {
            cachedHasNearbyEnemy = addon->HasNearbyEnemyInCombat(player, settings->fCooldownIgnoreDistance);
        }
    }
    return cachedHasNearbyEnemy;
}

void CooldownState::Update()
{
    auto* settings = Settings::GetSingleton();
    if (!settings->bEnableCooldown) {
        // Clear cooldown state when disabled
        inCooldown = false;
        blockedByActiveCooldown = false;
        parryEffectCachedThisFrame = false;  // Reset cache
        return;
    }
    
    // Reset per-frame caches at start of each frame
    blockedByActiveCooldown = false;
    parryEffectCachedThisFrame = false;
    
    // Use cached parry effect check (only checks once per frame)
    bool hasParryEffectNow = GetParryEffectCached();
    
    // Use cached nearby enemy check (only checks every 100ms instead of every frame)
    bool shouldIgnoreCooldown = !GetNearbyEnemyCached();
    
    if (settings->bDebugLogging && hasParryEffectNow != hadParryEffectLastFrame) {
        logger::info("[COOLDOWN UPDATE] ParryEffect: {} -> {}, InCooldown: {}, IgnoreDueToDistance: {}", 
            hadParryEffectLastFrame, hasParryEffectNow, inCooldown, shouldIgnoreCooldown);
    }
    
    // Detect transition: had effect last frame, doesn't have it now = window ended
    if (hadParryEffectLastFrame && !hasParryEffectNow) {
        // Parry window just ended
        if (!timedBlockTriggeredThisWindow && !shouldIgnoreCooldown) {
            // No timed block was triggered during this window - start/restart cooldown
            // But ONLY if we're not ignoring cooldown due to distance
            StartCooldown("Parry window ended without timed block");
        } else if (timedBlockTriggeredThisWindow) {
            // Successful timed block - CLEAR the cooldown entirely to allow consecutive blocks
            inCooldown = false;
            if (settings->bDebugLogging) {
                logger::info("[COOLDOWN] Parry window ENDED with successful timed block - COOLDOWN CLEARED");
            }
        } else if (shouldIgnoreCooldown) {
            if (settings->bDebugLogging) {
                logger::info("[COOLDOWN] Parry window ENDED without timed block - but IGNORING (no enemies nearby)");
            }
        }
        
        // Reset for next window
        timedBlockTriggeredThisWindow = false;
    }
    
    // Detect new parry window starting
    if (!hadParryEffectLastFrame && hasParryEffectNow) {
        timedBlockTriggeredThisWindow = false;
        
        if (settings->bDebugLogging) {
            logger::info("[COOLDOWN] Parry window STARTED (cooldown active: {}, ignoring: {})", inCooldown, shouldIgnoreCooldown);
        }
        
        // If on cooldown AND not ignoring due to distance, immediately dispel and RESTART the cooldown timer
        if (IsOnCooldownInternal() && !shouldIgnoreCooldown) {
            auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                cooldownEndTime - std::chrono::steady_clock::now()).count();
            logger::info("[COOLDOWN] Blocked during cooldown ({}ms remaining) - RESTARTING cooldown", remainingMs);
            
            // Mark that this frame's block was blocked by cooldown
            blockedByActiveCooldown = true;
            
            // RESTART the cooldown timer (punish spam blocking)
            StartCooldown("Block during cooldown - RESTARTED");
            
            if (settings->bDebugLogging) {
                RE::DebugNotification("[TB Debug] Block during cooldown - RESTART");
            }
            
            // Dispel the parry window effect
            DispelParryWindowSpell();
            hasParryEffectNow = false;
        } else if (IsOnCooldownInternal() && shouldIgnoreCooldown) {
            // On cooldown but ignoring - allow the block to proceed
            if (settings->bDebugLogging) {
                logger::info("[COOLDOWN] On cooldown but IGNORING (no enemies within {} units)", settings->fCooldownIgnoreDistance);
                RE::DebugNotification("[TB Debug] Cooldown ignored (no enemies)");
            }
        }
    }
    
    hadParryEffectLastFrame = hasParryEffectNow;
}

void CooldownState::StartCooldown(const char* reason)
{
    auto* settings = Settings::GetSingleton();
    inCooldown = true;
    cooldownEndTime = std::chrono::steady_clock::now() + 
        std::chrono::milliseconds(static_cast<long long>(settings->fCooldownDurationMs));
    
    if (settings->bDebugLogging) {
        logger::info("[COOLDOWN] {} - Starting {}ms cooldown", reason, settings->fCooldownDurationMs);
        RE::DebugNotification("[TB Debug] Cooldown STARTED");
    }
}

void CooldownState::OnTimedBlockTriggered()
{
    timedBlockTriggeredThisWindow = true;
    
    // CLEAR the cooldown on successful timed block to allow consecutive blocks
    inCooldown = false;
    blockedByActiveCooldown = false;
    
    auto* settings = Settings::GetSingleton();
    if (settings->bDebugLogging) {
        logger::info("[COOLDOWN] TIMED BLOCK SUCCESS! Cooldown CLEARED for consecutive blocks");
        RE::DebugNotification("[TB Debug] TIMED BLOCK SUCCESS!");
    }
}

bool CooldownState::IsOnCooldownInternal()
{
    if (!inCooldown) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    if (now >= cooldownEndTime) {
        inCooldown = false;
        return false;
    }
    
    return true;
}

bool CooldownState::IsOnCooldown()
{
    auto* settings = Settings::GetSingleton();
    
    // If we already blocked a parry window this frame, we're definitely on cooldown
    if (blockedByActiveCooldown) {
        if (settings->bDebugLogging) {
            logger::info("[COOLDOWN] IsOnCooldown(): YES (blockedByActiveCooldown flag set)");
        }
        return true;
    }
    
    if (!inCooldown) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    if (now >= cooldownEndTime) {
        inCooldown = false;
        
        if (settings->bDebugLogging) {
            logger::info("[COOLDOWN] Cooldown EXPIRED by timer");
            RE::DebugNotification("[TB Debug] Cooldown EXPIRED");
        }
        return false;
    }
    
    // Calculate remaining time for logging (but don't spam)
    return true;
}


//=============================================================================
// Helper Functions
//=============================================================================

bool IsActorPowerAttacking(RE::Actor* actor, std::string* outReason = nullptr)
{
    if (!actor) {
        return false;
    }
    
    // Method 1: Check attack state (vanilla)
    auto attackState = actor->AsActorState()->GetAttackState();
    
    // Power attacks and bashes
    if (attackState == RE::ATTACK_STATE_ENUM::kBash) {
        if (outReason) *outReason = "Bash";
        return true;
    }
    
    // Method 2: Check vanilla behavior graph variables
    bool isPowerAttacking = false;
    actor->GetGraphVariableBool("bIsPowerAttacking", isPowerAttacking);
    if (isPowerAttacking) {
        if (outReason) *outReason = "bIsPowerAttacking";
        return true;
    }
    
    // Method 3: Check bIsFullBodyPowerAttack (vanilla full body power attacks)
    bool isFullBodyPower = false;
    actor->GetGraphVariableBool("bIsFullBodyPowerAttack", isFullBodyPower);
    if (isFullBodyPower) {
        if (outReason) *outReason = "bIsFullBodyPowerAttack";
        return true;
    }
    
    // Method 4: MCO - Check MCO_IsHeavyAttack behavior variable
    bool mcoHeavyAttack = false;
    if (actor->GetGraphVariableBool("MCO_IsHeavyAttack", mcoHeavyAttack) && mcoHeavyAttack) {
        if (outReason) *outReason = "MCO_IsHeavyAttack";
        return true;
    }
    
    // Method 5: MCO - Check MCO_PowerAttack behavior variable  
    bool mcoPowerAttack = false;
    if (actor->GetGraphVariableBool("MCO_PowerAttack", mcoPowerAttack) && mcoPowerAttack) {
        if (outReason) *outReason = "MCO_PowerAttack";
        return true;
    }
    
    // Method 6: MCO - Check MCO_AttackType (> 0 typically means power/heavy attack)
    int mcoAttackType = 0;
    if (actor->GetGraphVariableInt("MCO_AttackType", mcoAttackType) && mcoAttackType > 0) {
        if (outReason) *outReason = fmt::format("MCO_AttackType={}", mcoAttackType);
        return true;
    }
    
    // Method 7: SCAR - Check SCAR_PowerAttacking
    bool scarPowerAttack = false;
    if (actor->GetGraphVariableBool("SCAR_PowerAttacking", scarPowerAttack) && scarPowerAttack) {
        if (outReason) *outReason = "SCAR_PowerAttacking";
        return true;
    }
    
    // Method 8: SCAR - Check SCAR_IsHeavyAttack
    bool scarHeavyAttack = false;
    if (actor->GetGraphVariableBool("SCAR_IsHeavyAttack", scarHeavyAttack) && scarHeavyAttack) {
        if (outReason) *outReason = "SCAR_IsHeavyAttack";
        return true;
    }
    
    // Method 9: Check SkySA/ABR style - iState variable (3 = power attack)
    int iState = 0;
    if (actor->GetGraphVariableInt("iState", iState) && iState == 3) {
        if (outReason) *outReason = "iState=3 (SkySA/ABR)";
        return true;
    }
    
    // Method 10: Check vanilla attackPower variable
    float attackPowerFloat = 0.0f;
    actor->GetGraphVariableFloat("attackPower", attackPowerFloat);
    if (attackPowerFloat > 0.1f) {
        if (outReason) *outReason = fmt::format("attackPower={:.2f}", attackPowerFloat);
        return true;
    }
    
    // Method 11: Check staggerPower for heavy attacks (high stagger = power attack)
    float staggerPower = 0.0f;
    actor->GetGraphVariableFloat("staggerPower", staggerPower);
    if (staggerPower > 0.5f) {
        if (outReason) *outReason = fmt::format("staggerPower={:.2f}", staggerPower);
        return true;
    }
    
    // Method 12: Check iAttackState for certain values indicating power attacks
    int iAttackState = 0;
    actor->GetGraphVariableInt("iAttackState", iAttackState);
    // iAttackState of 2 or 3 often indicates power attacks in various frameworks
    if (iAttackState >= 2) {
        if (outReason) *outReason = fmt::format("iAttackState={}", iAttackState);
        return true;
    }
    
    // Method 13: Check for ADXP/MCO style heavy attack via IsHeavyAttack boolean
    bool isHeavyAttack = false;
    if (actor->GetGraphVariableBool("IsHeavyAttack", isHeavyAttack) && isHeavyAttack) {
        if (outReason) *outReason = "IsHeavyAttack";
        return true;
    }
    
    // Method 14: Check for bInJumpState combined with attacking (jump attacks are often power attacks)
    bool isJumpAttack = false;
    if (actor->GetGraphVariableBool("bInJumpState", isJumpAttack) && isJumpAttack) {
        bool isAttacking = false;
        actor->GetGraphVariableBool("IsAttacking", isAttacking);
        if (isAttacking) {
            if (outReason) *outReason = "JumpAttack";
            return true;
        }
    }
    
    return false;
}

//=============================================================================
// TimedBlockAddon Implementation
//=============================================================================

TimedBlockAddon* TimedBlockAddon::GetSingleton() {
    static TimedBlockAddon singleton;
    return &singleton;
}

void TimedBlockAddon::Initialize() {
    logger::info("Initializing TimedBlockAddon...");
    
    if (!LoadForms()) {
        logger::error("Failed to load required forms from SimpleTimedBlock.esp!");
        logger::error("Make sure SimpleTimedBlock.esp is loaded.");
        return;
    }
    
    // Initialize animation speed hooks (also handles cooldown state tracking)
    AnimSpeedManager::Init();
    
    // Install physics hook for lunge movement
    LungePhysicsHook::Install();
    
    logger::info("TimedBlockAddon initialized successfully");
}

void TimedBlockAddon::Register() {
    RE::ScriptEventSourceHolder* eventHolder = RE::ScriptEventSourceHolder::GetSingleton();
    if (eventHolder) {
        eventHolder->AddEventSink(GetSingleton());
        logger::info("Registered TimedBlockAddon event sink");
    }
}

bool TimedBlockAddon::LoadForms() {
    const char* mod = "SimpleTimedBlock.esp";
    const RE::FormID parryWindowEffectID = 0x801;
    const RE::FormID parryWindowSpellID = 0x802;  // The parry window spell (0x802, not 0x800)
    
    RE::TESDataHandler* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) {
        logger::error("Failed to get TESDataHandler");
        return false;
    }
    
    // Check if SimpleTimedBlock.esp is loaded
    if (!dataHandler->LookupModByName(mod)) {
        logger::error("SimpleTimedBlock.esp is not loaded!");
        return false;
    }
    
    mgef_parry_window = dataHandler->LookupForm<RE::EffectSetting>(parryWindowEffectID, mod);
    if (!mgef_parry_window) {
        logger::error("Failed to find parry window effect (0x801) in SimpleTimedBlock.esp");
        return false;
    }
    
    spell_parry_window = dataHandler->LookupForm<RE::SpellItem>(parryWindowSpellID, mod);
    if (!spell_parry_window) {
        logger::warn("Failed to find parry window spell (0x802) in SimpleTimedBlock.esp");
    } else {
        logger::info("Found parry window spell: {} (FormID: {:08X})", spell_parry_window->GetName(), spell_parry_window->GetFormID());
    }
    
    // The parry window timing is controlled by the Magic Effect's TAPER DURATION (0.33s default)
    // NOT the spell effect's duration field! Let's modify the magic effect directly.
    auto* settings = Settings::GetSingleton();
    float durationSeconds = settings->fParryWindowDurationMs / 1000.0f;
    
    // Log original magic effect data
    logger::info("Magic Effect '{}' original data:", mgef_parry_window->GetName());
    logger::info("  taperDuration: {:.3f}s ({:.0f}ms)", 
        mgef_parry_window->data.taperDuration, 
        mgef_parry_window->data.taperDuration * 1000.0f);
    logger::info("  taperCurve: {:.3f}", mgef_parry_window->data.taperCurve);
    
    // Modify the magic effect's taper duration
    float originalTaperDuration = mgef_parry_window->data.taperDuration;
    mgef_parry_window->data.taperDuration = durationSeconds;
    
    logger::info(">>> MODIFIED magic effect taper duration: {:.3f}s -> {:.3f}s ({:.0f}ms -> {:.0f}ms)", 
        originalTaperDuration, durationSeconds,
        originalTaperDuration * 1000.0f, durationSeconds * 1000.0f);
    
    // Show debug notification in-game
    RE::DebugNotification(fmt::format("[TB Addon] Parry window: {:.0f}ms -> {:.0f}ms", 
        originalTaperDuration * 1000.0f, settings->fParryWindowDurationMs).c_str());
    
    logger::debug("Found parry window effect: {}", mgef_parry_window->GetName());
    return true;
}

bool TimedBlockAddon::ActorHasParryWindowEffect(RE::Actor* actor) {
    if (!actor || !mgef_parry_window) {
        return false;
    }
    
    auto magicTarget = actor->AsMagicTarget();
    if (!magicTarget) {
        return false;
    }
    
    auto activeEffects = magicTarget->GetActiveEffectList();
    if (!activeEffects || activeEffects->empty()) {
        return false;
    }
    
    for (RE::ActiveEffect* effect : *activeEffects) {
        if (effect && !effect->flags.any(RE::ActiveEffect::Flag::kInactive)) {
            RE::EffectSetting* setting = effect->GetBaseObject();
            if (setting && setting == mgef_parry_window) {
                return true;
            }
        }
    }
    
    return false;
}

bool TimedBlockAddon::HasNearbyEnemyInCombat(RE::Actor* player, float radius) {
    if (!player) {
        return false;  // No player, no check needed
    }
    
    // OPTIMIZATION: Early exit if player is not in combat at all
    // This is the most common case during exploration - skip all actor iteration
    if (!player->IsInCombat()) {
        return false;
    }
    
    auto settings = Settings::GetSingleton();
    auto playerPos = player->GetPosition();
    
    // OPTIMIZATION: Use squared distance to avoid sqrt operations
    float radiusSq = radius * radius;
    
    // Get the process lists to iterate through high actors (nearby, loaded actors)
    auto processLists = RE::ProcessLists::GetSingleton();
    if (!processLists) {
        return true;  // Fail-safe: assume enemy nearby
    }
    
    // Check all high-process actors (actors near the player that are fully loaded)
    for (auto& actorHandle : processLists->highActorHandles) {
        auto actor = actorHandle.get().get();
        if (!actor || actor == player || actor->IsDead()) {
            continue;
        }
        
        // OPTIMIZATION: Check squared distance first (cheaper than combat checks)
        float distSq = playerPos.GetSquaredDistance(actor->GetPosition());
        if (distSq > radiusSq) {
            continue;
        }
        
        // OPTIMIZATION: Simple hostility + combat check instead of iterating combat targets
        // If actor is hostile to player AND in combat, it's likely fighting the player
        if (actor->IsHostileToActor(player) && actor->IsInCombat()) {
            if (settings->bDebugLogging) {
                logger::debug("[DISTANCE] ENEMY FOUND: '{}' at {:.0f} units (hostile + in combat)",
                    actor->GetName(), std::sqrt(distSq));
            }
            return true;
        }
    }
    
    if (settings->bDebugLogging) {
        logger::debug("[DISTANCE] No hostile enemies in combat within {} units", radius);
    }
    return false;
}

RE::BSEventNotifyControl TimedBlockAddon::ProcessEvent(
    const RE::TESHitEvent* a_event,
    RE::BSTEventSource<RE::TESHitEvent>*) 
{
    using Result = RE::BSEventNotifyControl;
    using HitFlag = RE::TESHitEvent::Flag;
    
    if (!a_event || !a_event->target || a_event->projectile) {
        return Result::kContinue;
    }
    
    RE::Actor* defender = a_event->target->As<RE::Actor>();
    if (!defender || !defender->IsPlayerRef()) {
        return Result::kContinue;  // Only process for player
    }
    
    // Check if this is a blocked hit
    if (!a_event->flags.any(HitFlag::kHitBlocked)) {
        return Result::kContinue;
    }
    
    auto* settings = Settings::GetSingleton();
    
    // Get the attacker FIRST - we need it for distance check
    RE::Actor* attacker = a_event->cause ? a_event->cause->As<RE::Actor>() : nullptr;
    if (!attacker) {
        return Result::kContinue;
    }
    
    // Check if player has the parry window effect (indicating a timed block)
    // Use cached result if available (same frame), otherwise check fresh
    if (!CooldownState::GetParryEffectCached()) {
        return Result::kContinue;
    }
    
    if (settings->bDebugLogging) {
        logger::info("[HITEVENT] Blocked hit detected with parry window effect active!");
    }
    
    // *** CHECK COOLDOWN FIRST - before any effects can be applied ***
    if (settings->bEnableCooldown) {
        // Use cached nearby enemy check (checked every 100ms, not every event)
        bool ignoreCooldownDueToDistance = !CooldownState::GetNearbyEnemyCached();
        if (ignoreCooldownDueToDistance && settings->bDebugLogging) {
            logger::debug("[COOLDOWN] No enemies in combat nearby - IGNORING cooldown");
        }
        
        if (!ignoreCooldownDueToDistance && CooldownState::IsOnCooldown()) {
            logger::info("[HITEVENT] Timed block HIT detected but on cooldown - SKIPPING ALL addon effects");
            if (settings->bDebugLogging) {
                RE::DebugNotification("[TB Debug] WOULD HAVE been timed block (cooldown)");
            }
            return Result::kContinue;  // EXIT EARLY - no effects!
        }
    }
    
    // Mark that a timed block was triggered (for cooldown tracking)
    CooldownState::OnTimedBlockTriggered();
    
    // Start counter attack window (if enabled) - pass attacker for lunge targeting
    CounterAttackState::StartWindow(attacker);
    
    logger::info("[HITEVENT] TIMED BLOCK SUCCESS! Applying addon effects...");
    
    // Apply all effects
    ApplyTimedBlockEffects(defender, attacker);
    
    return Result::kContinue;
}

void TimedBlockAddon::ApplyTimedBlockEffects(RE::Actor* defender, RE::Actor* attacker) {
    auto settings = Settings::GetSingleton();
    
    // 0. PREVENT player stagger - cancel any stagger/recoil animation on the defender
    // This ensures heavy attacks don't stagger the player on a successful timed block
    if (settings->bPreventPlayerStagger && defender) {
        // Cancel stagger graph variables (vanilla)
        defender->SetGraphVariableBool("IsStaggering", false);
        defender->SetGraphVariableBool("IsRecoiling", false);
        defender->SetGraphVariableBool("IsBlockHit", false);
        defender->SetGraphVariableFloat("staggerMagnitude", 0.0f);
        
        // MaxuBlockOverhaul compatibility
        defender->SetGraphVariableBool("Maxsu_IsBlockHit", false);
        defender->SetGraphVariableBool("bMaxsu_BlockHit", false);
        defender->SetGraphVariableFloat("Maxsu_BlockHitStrength", 0.0f);
        
        // Send animation events to interrupt stagger (vanilla)
        defender->NotifyAnimationGraph("staggerStop");
        defender->NotifyAnimationGraph("BlockHitEnd");
        
        // MaxuBlockOverhaul compatibility
        defender->NotifyAnimationGraph("Maxsu_BlockHitEnd");
        defender->NotifyAnimationGraph("Maxsu_BlockHitInterrupt");
        
        if (settings->bDebugLogging) {
            logger::info("[TIMED BLOCK] Cancelled player stagger/recoil (vanilla + MaxuBlockOverhaul)");
        }
    }
    
    // 1. Freeze attacker's animation (hitstop effect - does NOT slow down the world)
    if (settings->bEnableHitstop && attacker) {
        float hitstopSpeed = settings->fHitstopSpeed;  // 0.0 = complete freeze, 0.1 = very slow
        float hitstopDuration = settings->fHitstopDuration;
        
        logger::debug("Applying hitstop to attacker: speed={}, duration={}s", hitstopSpeed, hitstopDuration);
        AnimSpeedManager::SetAnimSpeed(attacker->GetHandle(), hitstopSpeed, hitstopDuration);
    }
    
    // 2. Force attacker into stagger animation (with optional skill-based chance)
    if (settings->bEnableStagger && attacker && defender) {
        // Detect if attacker is performing a power attack (including MCO/SCAR)
        std::string powerAttackReason;
        bool isPowerAttack = IsActorPowerAttacking(attacker, &powerAttackReason);
        float staggerMagnitude = isPowerAttack ? settings->fPowerAttackStaggerMagnitude : settings->fStaggerMagnitude;
        
        if (settings->bDebugLogging) {
            if (isPowerAttack) {
                logger::info("[TIMED BLOCK] Blocked POWER ATTACK from '{}' (detected via: {}), stagger: {}", 
                    attacker->GetName(), powerAttackReason, staggerMagnitude);
                RE::DebugNotification(fmt::format("[TB] POWER ATTACK blocked ({})", powerAttackReason).c_str());
            } else {
                logger::info("[TIMED BLOCK] Blocked NORMAL ATTACK from '{}', stagger: {}", 
                    attacker->GetName(), staggerMagnitude);
                RE::DebugNotification("[TB] Normal attack blocked");
            }
        } else {
            logger::debug("Triggering stagger on attacker with magnitude: {}", staggerMagnitude);
        }
        
        // Check skill-based stagger chance (if enabled)
        if (RollStaggerSuccess(defender)) {
            TriggerStagger(defender, attacker, staggerMagnitude);
        } else {
            logger::debug("Stagger skipped due to failed skill-based chance roll");
        }
    }
    
    // 3. Camera shake for impact feel
    if (settings->bEnableCameraShake && defender) {
        float shakeStrength = settings->fCameraShakeStrength;
        float shakeDuration = settings->fCameraShakeDuration;
        logger::debug("Applying camera shake: strength={}, duration={}s", shakeStrength, shakeDuration);
        ShakeCamera(shakeStrength, defender->GetPosition(), shakeDuration);
    }
    
    // 4. Restore stamina on successful timed block
    if (settings->bEnableStaminaRestore && defender) {
        auto* avOwner = defender->AsActorValueOwner();
        if (avOwner) {
            float maxStamina = avOwner->GetPermanentActorValue(RE::ActorValue::kStamina);
            float currentStamina = avOwner->GetActorValue(RE::ActorValue::kStamina);
            float restoreAmount = maxStamina * (settings->fStaminaRestorePercent / 100.0f);
            float actualRestore = (std::min)(restoreAmount, maxStamina - currentStamina);  // Don't over-fill
            
            if (actualRestore > 0.0f) {
                avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, actualRestore);
                
                if (settings->bDebugLogging) {
                    logger::info("[TIMED BLOCK] Restored {:.1f} stamina ({:.0f}% of max {:.1f})", 
                        actualRestore, settings->fStaminaRestorePercent, maxStamina);
                    RE::DebugNotification(fmt::format("[TB] +{:.0f} Stamina", actualRestore).c_str());
                }
            }
        }
    }
    
    // 5. Play sound effect
    if (settings->bEnableSound) {
        PlayTimedBlockSound();
    }
    
    // 6. Apply slowmo effect (slows entire world)
    if (settings->bEnableSlowmo) {
        ApplySlowmo(settings->fSlowmoSpeed, settings->fSlowmoDuration);
    }
}

void TimedBlockAddon::TriggerStagger(RE::Actor* defender, RE::Actor* attacker, float magnitude) {
    if (!defender || !attacker) {
        return;
    }
    
    // Don't stagger if swimming, in killmove, or not loaded
    if (attacker->AsActorState()->IsSwimming() || attacker->IsInKillMove() || !attacker->Is3DLoaded()) {
        return;
    }
    
    // Calculate stagger direction based on heading angle from attacker to defender
    // This makes the attacker stagger backwards (away from defender)
    RE::NiPoint3 defenderPos = defender->GetPosition();
    float headingAngle = attacker->GetHeadingAngle(defenderPos, false);
    float direction = (headingAngle >= 0.0f) ? headingAngle / 360.0f : (360.0f + headingAngle) / 360.0f;
    
    // Set stagger graph variables
    static const RE::BSFixedString staggerDirection("staggerDirection");
    static const RE::BSFixedString staggerMagnitude("StaggerMagnitude");
    static const RE::BSFixedString staggerStart("staggerStart");
    
    attacker->SetGraphVariableFloat(staggerDirection, direction);
    attacker->SetGraphVariableFloat(staggerMagnitude, magnitude);
    attacker->NotifyAnimationGraph(staggerStart);
    
    logger::debug("Triggered stagger: direction={}, magnitude={}", direction, magnitude);
}

void TimedBlockAddon::ShakeCamera(float strength, const RE::NiPoint3& position, float duration) {
    if (strength <= 0.0f || duration <= 0.0f) {
        return;
    }
    
    // Use the game's native camera shake function
    Offsets::ShakeCamera(strength, position, duration);
}

void TimedBlockAddon::PlayTimedBlockSound() {
    auto settings = Settings::GetSingleton();
    
    if (settings->bUseCustomWav) {
        // Play custom WAV file using Windows API
        PlayCustomWavSound();
    } else {
        // Play game sound descriptor
        if (settings->sSoundPath.empty()) {
            return;
        }
        
        logger::debug("Playing timed block sound: {}", settings->sSoundPath);
        
        // Play the sound on main thread
        SKSE::GetTaskInterface()->AddTask([soundPath = settings->sSoundPath]() {
            RE::PlaySound(soundPath.c_str());
            logger::debug("Played sound descriptor: {}", soundPath);
        });
    }
}

// WAV file header structures for volume adjustment
#pragma pack(push, 1)
struct WAVHeader {
    char     riff[4];        // "RIFF"
    uint32_t fileSize;       // File size - 8
    char     wave[4];        // "WAVE"
};

struct WAVChunkHeader {
    char     id[4];
    uint32_t size;
};

struct WAVFmtChunk {
    uint16_t audioFormat;    // 1 = PCM
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};
#pragma pack(pop)

// Global buffer for WAV playback (needs to stay alive during playback)
static std::vector<uint8_t> g_timedBlockAudioBuffer;
static std::vector<uint8_t> g_counterStrikeAudioBuffer;

// Load WAV file and apply software volume adjustment
bool LoadWAVWithVolume(const std::filesystem::path& filePath, float volumeScale, std::vector<uint8_t>& outBuffer) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }
    
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    outBuffer.resize(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(outBuffer.data()), fileSize)) {
        return false;
    }
    file.close();
    
    // Validate WAV header
    if (outBuffer.size() < sizeof(WAVHeader)) {
        logger::error("File too small to be a valid WAV");
        return false;
    }
    
    WAVHeader* header = reinterpret_cast<WAVHeader*>(outBuffer.data());
    if (std::memcmp(header->riff, "RIFF", 4) != 0 || std::memcmp(header->wave, "WAVE", 4) != 0) {
        logger::error("Not a valid WAV file");
        return false;
    }
    
    // Find fmt and data chunks
    size_t pos = sizeof(WAVHeader);
    WAVFmtChunk* fmt = nullptr;
    uint8_t* audioData = nullptr;
    uint32_t audioDataSize = 0;
    
    while (pos + sizeof(WAVChunkHeader) <= outBuffer.size()) {
        WAVChunkHeader* chunk = reinterpret_cast<WAVChunkHeader*>(outBuffer.data() + pos);
        
        if (std::memcmp(chunk->id, "fmt ", 4) == 0) {
            fmt = reinterpret_cast<WAVFmtChunk*>(outBuffer.data() + pos + sizeof(WAVChunkHeader));
        } else if (std::memcmp(chunk->id, "data", 4) == 0) {
            audioData = outBuffer.data() + pos + sizeof(WAVChunkHeader);
            audioDataSize = chunk->size;
            break;
        }
        
        pos += sizeof(WAVChunkHeader) + chunk->size;
        if (pos % 2 != 0) pos++;  // Ensure 2-byte alignment
    }
    
    if (!fmt || !audioData || audioDataSize == 0) {
        logger::error("Could not find fmt or data chunk in WAV");
        return false;
    }
    
    // Only process PCM audio
    if (fmt->audioFormat != 1) {
        logger::warn("Non-PCM WAV format, volume adjustment skipped");
        return true;
    }
    
    // If volume is 100%, no need to adjust
    if (volumeScale >= 0.99f) {
        return true;
    }
    
    // Adjust audio samples based on bit depth
    if (fmt->bitsPerSample == 16) {
        int16_t* samples = reinterpret_cast<int16_t*>(audioData);
        size_t numSamples = audioDataSize / sizeof(int16_t);
        
        for (size_t i = 0; i < numSamples; ++i) {
            float sample = static_cast<float>(samples[i]) * volumeScale;
            samples[i] = static_cast<int16_t>(std::clamp(sample, -32768.0f, 32767.0f));
        }
    } else if (fmt->bitsPerSample == 8) {
        // 8-bit audio is unsigned (0-255, 128 is silence)
        for (size_t i = 0; i < audioDataSize; ++i) {
            float sample = (static_cast<float>(audioData[i]) - 128.0f) * volumeScale;
            audioData[i] = static_cast<uint8_t>(std::clamp(sample + 128.0f, 0.0f, 255.0f));
        }
    } else if (fmt->bitsPerSample == 24) {
        size_t numSamples = audioDataSize / 3;
        for (size_t i = 0; i < numSamples; ++i) {
            int32_t sample = audioData[i * 3] | (audioData[i * 3 + 1] << 8) | (audioData[i * 3 + 2] << 16);
            if (sample & 0x800000) sample |= 0xFF000000;  // Sign extend
            
            float adjusted = static_cast<float>(sample) * volumeScale;
            int32_t result = static_cast<int32_t>(std::clamp(adjusted, -8388608.0f, 8388607.0f));
            
            audioData[i * 3] = result & 0xFF;
            audioData[i * 3 + 1] = (result >> 8) & 0xFF;
            audioData[i * 3 + 2] = (result >> 16) & 0xFF;
        }
    } else if (fmt->bitsPerSample == 32) {
        int32_t* samples = reinterpret_cast<int32_t*>(audioData);
        size_t numSamples = audioDataSize / sizeof(int32_t);
        
        for (size_t i = 0; i < numSamples; ++i) {
            double sample = static_cast<double>(samples[i]) * volumeScale;
            samples[i] = static_cast<int32_t>(std::clamp(sample, -2147483648.0, 2147483647.0));
        }
    } else {
        logger::warn("Unsupported WAV bit depth: {}", fmt->bitsPerSample);
    }
    
    return true;
}

void TimedBlockAddon::PlayCustomWavSound() {
    // Build the path to the WAV file
    // Path: Data/SKSE/Plugins/SimpleTimedBlockAddons/timedblock.wav
    
    auto settings = Settings::GetSingleton();
    float volume = settings->fCustomWavVolume;
    
    SKSE::GetTaskInterface()->AddTask([volume]() {
        // Get the game's data path
        std::filesystem::path wavPath = std::filesystem::current_path();
        wavPath /= "Data";
        wavPath /= "SKSE";
        wavPath /= "Plugins";
        wavPath /= "SimpleTimedBlockAddons";
        wavPath /= "timedblock.wav";
        
        if (std::filesystem::exists(wavPath)) {
            // Load WAV file and apply software volume adjustment
            // This modifies the audio samples directly, not affecting other sounds
            if (!LoadWAVWithVolume(wavPath, volume, g_timedBlockAudioBuffer)) {
                logger::error("Failed to load WAV file: {}", wavPath.string());
                // Fallback to game sound
                RE::PlaySound("UIMenuOK");
                return;
            }
            
            // Play from memory buffer with adjusted volume
            // SND_MEMORY: play from memory buffer
            // SND_ASYNC: play asynchronously (don't block)
            // SND_NODEFAULT: don't play default sound on error
            BOOL result = PlaySoundA(reinterpret_cast<LPCSTR>(g_timedBlockAudioBuffer.data()), 
                                     NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
            
            if (result) {
                logger::debug("Playing custom WAV at {}% volume: {}", volume * 100.0f, wavPath.string());
            } else {
                logger::error("Failed to play WAV. Error: {}", GetLastError());
                RE::PlaySound("UIMenuOK");
            }
        } else {
            logger::warn("Custom WAV file not found: {}", wavPath.string());
            RE::PlaySound("UIMenuOK");
        }
    });
}

void PlayCounterStrikeSound() {
    // Build the path to the WAV file
    // Path: Data/SKSE/Plugins/SimpleTimedBlockAddons/counterstrike.wav
    
    auto settings = Settings::GetSingleton();
    if (!settings->bEnableCounterStrikeSound) {
        return;
    }
    
    float volume = settings->fCounterStrikeSoundVolume;
    
    SKSE::GetTaskInterface()->AddTask([volume]() {
        // Get the game's data path
        std::filesystem::path wavPath = std::filesystem::current_path();
        wavPath /= "Data";
        wavPath /= "SKSE";
        wavPath /= "Plugins";
        wavPath /= "SimpleTimedBlockAddons";
        wavPath /= "counterstrike.wav";
        
        if (std::filesystem::exists(wavPath)) {
            // Load WAV file and apply software volume adjustment
            if (!LoadWAVWithVolume(wavPath, volume, g_counterStrikeAudioBuffer)) {
                logger::error("Failed to load counter strike WAV file: {}", wavPath.string());
                // Fallback to a game hit sound
                RE::PlaySound("NPCHumanCombatShieldBlock");
                return;
            }
            
            // Play from memory buffer with adjusted volume
            BOOL result = PlaySoundA(reinterpret_cast<LPCSTR>(g_counterStrikeAudioBuffer.data()), 
                                     NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
            
            if (result) {
                logger::debug("Playing counter strike WAV at {}% volume: {}", volume * 100.0f, wavPath.string());
            } else {
                logger::error("Failed to play counter strike WAV. Error: {}", GetLastError());
                RE::PlaySound("NPCHumanCombatShieldBlock");
            }
        } else {
            logger::warn("Counter strike WAV file not found: {} - using fallback sound", wavPath.string());
            RE::PlaySound("NPCHumanCombatShieldBlock");
        }
    });
}

void TimedBlockAddon::ApplySlowmo(float speed, float duration) {
    logger::debug("Applying slowmo: speed={}, duration={}s", speed, duration);
    
    // Set slowmo
    SKSE::GetTaskInterface()->AddTask([speed]() {
        static REL::Relocation<float*> gtm{ RELOCATION_ID(511883, 388443) };
        *gtm = speed;
        logger::debug("Slowmo activated: {:.0f}%", speed * 100.0f);
    });
    
    // Schedule restoration of normal speed
    std::thread([duration]() {
        // Wait for slowmo duration in real time
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(duration * 1000.0f)));
        
        // Restore normal speed on main thread
        SKSE::GetTaskInterface()->AddTask([]() {
            static REL::Relocation<float*> gtm{ RELOCATION_ID(511883, 388443) };
            *gtm = 1.0f;
            logger::debug("Slowmo ended, speed restored to normal");
        });
    }).detach();
}

//=============================================================================
// DispelParryWindowSpell - Safely removes parry window effect during cooldown
//=============================================================================

void DispelParryWindowSpell()
{
    auto* addon = TimedBlockAddon::GetSingleton();
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* settings = Settings::GetSingleton();
    
    if (!addon || !player) {
        return;
    }
    
    // Get the spell from the addon (we store it during LoadForms)
    // Access it through the addon's spell_parry_window member
    // Since spell_parry_window is private, we'll use dispel by effect instead
    
    auto magicTarget = player->AsMagicTarget();
    if (!magicTarget) {
        return;
    }
    
    auto activeEffects = magicTarget->GetActiveEffectList();
    if (!activeEffects || activeEffects->empty()) {
        return;
    }
    
    // Find and dispel the parry window effect
    for (RE::ActiveEffect* effect : *activeEffects) {
        if (effect && !effect->flags.any(RE::ActiveEffect::Flag::kInactive)) {
            RE::EffectSetting* setting = effect->GetBaseObject();
            // Check if this is the parry window effect by checking if actor has it
            if (setting && addon->ActorHasParryWindowEffect(player)) {
                // Dispel by setting the effect to finish
                effect->Dispel(true);
                
                if (settings->bDebugLogging) {
                    logger::info("[COOLDOWN] DISPELLED parry window effect - cooldown is ACTIVE");
                }
                return;
            }
        }
    }
}

//=============================================================================
// Counter Attack State - allows attacking to cancel block animation
//=============================================================================

void CounterAttackState::StartWindow(RE::Actor* attacker)
{
    auto* settings = Settings::GetSingleton();
    if (!settings->bEnableCounterAttack) {
        return;
    }
    
    inWindow = true;
    windowEndTime = std::chrono::steady_clock::now() + 
        std::chrono::milliseconds(static_cast<long long>(settings->fCounterAttackWindow * 1000.0f));
    
    // Store the attacker handle for lunge targeting
    if (attacker) {
        lastAttackerHandle = attacker->GetHandle();
    }
    
    // Arm the counter slow time if enabled
    if (settings->bEnableCounterSlowTime) {
        CounterSlowTimeState::Arm();
    }
    
    if (settings->bDebugLogging) {
        logger::info("[COUNTER] Counter attack window OPENED for {}ms", settings->fCounterAttackWindow * 1000.0f);
        RE::DebugNotification("[TB] Counter window open");
    }
}

RE::Actor* CounterAttackState::GetLastAttacker()
{
    auto actorPtr = lastAttackerHandle.get();
    return actorPtr.get();
}

void CounterAttackState::Update()
{
    if (!inWindow && !damageBonusActive) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto* settings = Settings::GetSingleton();
    
    // Check counter attack window timeout
    if (inWindow && now >= windowEndTime) {
        inWindow = false;
        
        if (settings->bDebugLogging) {
            logger::info("[COUNTER] Counter attack window CLOSED (timed out)");
        }
    }
    
    // Check damage bonus timeout (uses configurable timeout)
    if (damageBonusActive && now >= damageBonusEndTime) {
        logger::info("[COUNTER DAMAGE] Damage bonus timed out ({:.1f}s)", settings->fCounterDamageBonusTimeout);
        spdlog::default_logger()->flush();
        
        if (settings->bDebugLogging) {
            RE::DebugNotification("[TB] Counter damage expired");
        }
        RemoveDamageBonus();
    }
}

bool CounterAttackState::IsInWindow()
{
    return inWindow;
}

void CounterAttackState::OnAttackInput()
{
    if (!inWindow) {
        return;
    }
    
    auto* settings = Settings::GetSingleton();
    auto* player = RE::PlayerCharacter::GetSingleton();
    
    if (!player) {
        return;
    }
    
    // Cancel the block recoil/stagger animation by sending animation events
    // This allows the player to attack instead of waiting for the block animation to finish
    
    // Cancel stagger/recoil/block hit states (vanilla)
    player->SetGraphVariableBool("IsStaggering", false);
    player->SetGraphVariableBool("IsRecoiling", false);
    player->SetGraphVariableBool("IsBlockHit", false);
    player->SetGraphVariableBool("bIsBlocking", false);
    player->SetGraphVariableFloat("staggerMagnitude", 0.0f);
    
    // MaxuBlockOverhaul compatibility - reset its custom variables
    player->SetGraphVariableBool("Maxsu_IsBlockHit", false);
    player->SetGraphVariableBool("bMaxsu_BlockHit", false);
    player->SetGraphVariableFloat("Maxsu_BlockHitStrength", 0.0f);
    
    // Send animation events to interrupt stagger/block (vanilla)
    player->NotifyAnimationGraph("staggerStop");
    player->NotifyAnimationGraph("recoilStop");
    player->NotifyAnimationGraph("blockStop");
    player->NotifyAnimationGraph("BlockHitEnd");
    
    // MaxuBlockOverhaul compatibility - send its custom interrupt events
    player->NotifyAnimationGraph("Maxsu_BlockHitEnd");
    player->NotifyAnimationGraph("Maxsu_BlockHitInterrupt");
    player->NotifyAnimationGraph("Maxsu_WeaponBlockHitEnd");
    player->NotifyAnimationGraph("Maxsu_ShieldBlockHitEnd");
    
    // Start lunge toward attacker if enabled
    if (settings->bEnableCounterLunge) {
        RE::Actor* attacker = GetLastAttacker();
        if (attacker && attacker->Is3DLoaded()) {
            CounterLungeState::Start(player, attacker);
        }
    }
    
    // Apply damage bonus if enabled
    if (settings->bEnableCounterDamageBonus) {
        ApplyDamageBonus();
    }
    
    if (settings->bDebugLogging) {
        logger::info("[COUNTER] Attack input during counter window - cancelled block animation (vanilla + MaxuBlockOverhaul)");
        RE::DebugNotification("[TB] Counter attack!");
    }
    
    // Close the window
    inWindow = false;
}

void CounterAttackState::ApplyDamageBonus()
{
    auto* settings = Settings::GetSingleton();
    
    if (damageBonusActive) {
        logger::info("[COUNTER DAMAGE] Bonus already active, skipping");
        return;
    }
    
    // Just set the state - we apply the actual damage when the hit lands
    appliedDamageBonus = settings->fCounterDamageBonusPercent;
    damageBonusActive = true;
    
    // Set the timeout for damage bonus (starts now, when counter attack is initiated)
    auto timeoutMs = static_cast<int>(settings->fCounterDamageBonusTimeout * 1000.0f);
    damageBonusEndTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    
    logger::info("[COUNTER DAMAGE] Armed +{:.0f}% damage bonus (timeout: {:.1f}s)", 
        settings->fCounterDamageBonusPercent, settings->fCounterDamageBonusTimeout);
    spdlog::default_logger()->flush();
    
    if (settings->bDebugLogging) {
        RE::DebugNotification(fmt::format("[TB] +{:.0f}% damage ready!", settings->fCounterDamageBonusPercent).c_str());
    }
}

void CounterAttackState::RemoveDamageBonus()
{
    if (!damageBonusActive) {
        return;  // Not active
    }
    
    logger::info("[COUNTER DAMAGE] Damage bonus cleared (was +{:.0f}%)", appliedDamageBonus);
    spdlog::default_logger()->flush();
    
    appliedDamageBonus = 0.0f;
    damageBonusActive = false;
}

bool CounterAttackState::IsDamageBonusActive()
{
    return damageBonusActive;
}

//=============================================================================
// Counter Attack Input Handler
//=============================================================================

CounterAttackInputHandler* CounterAttackInputHandler::GetSingleton()
{
    static CounterAttackInputHandler singleton;
    return &singleton;
}

void CounterAttackInputHandler::Register()
{
    auto* inputEventSource = RE::BSInputDeviceManager::GetSingleton();
    if (inputEventSource) {
        inputEventSource->AddEventSink(GetSingleton());
        logger::info("Counter attack input handler registered");
    }
}

RE::BSEventNotifyControl CounterAttackInputHandler::ProcessEvent(
    RE::InputEvent* const* a_event,
    RE::BSTEventSource<RE::InputEvent*>*)
{
    if (!a_event) {
        return RE::BSEventNotifyControl::kContinue;
    }
    
    auto* settings = Settings::GetSingleton();
    if (!settings->bEnableCounterAttack || !CounterAttackState::IsInWindow()) {
        return RE::BSEventNotifyControl::kContinue;
    }
    
    // Check if UI is open
    auto* ui = RE::UI::GetSingleton();
    if (ui && ui->GameIsPaused()) {
        return RE::BSEventNotifyControl::kContinue;
    }
    
    for (auto* event = *a_event; event; event = event->next) {
        if (event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
            continue;
        }
        
        auto* buttonEvent = static_cast<RE::ButtonEvent*>(event);
        if (!buttonEvent->IsDown()) {
            continue;
        }
        
        // Get the control mapped to this input
        auto* controlMap = RE::ControlMap::GetSingleton();
        if (!controlMap) {
            continue;
        }
        
        // Check if this is an attack input (right or left attack)
        std::string_view userEvent = controlMap->GetUserEventName(
            buttonEvent->GetIDCode(), 
            buttonEvent->GetDevice(), 
            RE::ControlMap::InputContextID::kGameplay
        );
        
        if (userEvent == "Right Attack/Block" || userEvent == "Left Attack/Block" ||
            userEvent == "Attack" || userEvent == "Power Attack") {
            // Attack input detected during counter window
            CounterAttackState::OnAttackInput();
            break;
        }
    }
    
    return RE::BSEventNotifyControl::kContinue;
}

//=============================================================================
// Counter Lunge State - moves player toward attacker
//=============================================================================

void CounterLungeState::Start(RE::Actor* player, RE::Actor* target)
{
    if (!player || !target) {
        return;
    }
    
    auto* settings = Settings::GetSingleton();
    
    startPos = player->GetPosition();
    RE::NiPoint3 targetPos = target->GetPosition();
    
    // Calculate distance to target (horizontal only)
    RE::NiPoint3 diff = targetPos - startPos;
    diff.z = 0.0f;
    float distance = diff.Length();
    
    // Don't lunge if too close (prevents collision issues)
    if (distance < MIN_LUNGE_DISTANCE) {
        if (settings->bDebugLogging) {
            logger::info("[LUNGE] Target too close ({:.1f} < {:.1f}), skipping lunge", 
                distance, MIN_LUNGE_DISTANCE);
            RE::DebugNotification("[TB] Target too close for lunge");
        }
        return;
    }
    
    // Store target handle so we can track them during the lunge
    targetHandle = target->GetHandle();
    
    // Calculate lunge parameters
    // Don't go past 80% of distance (leave some gap to target)
    float targetDist = distance * 0.8f - MIN_LUNGE_DISTANCE * 0.5f;
    if (targetDist < 50.0f) {
        targetDist = 50.0f;  // Minimum lunge distance
    }
    maxDistance = (std::min)(targetDist, settings->fCounterLungeDistance);
    speed = settings->fCounterLungeSpeed;
    duration = maxDistance / speed;
    elapsed = 0.0f;
    active = true;
    
    if (settings->bDebugLogging) {
        float playerYaw = player->GetAngle().z;
        RE::NiPoint3 worldDir = diff / distance;
        float targetAngle = std::atan2(worldDir.x, worldDir.y);
        
        logger::info("[LUNGE] ========== LUNGE STARTED ==========");
        logger::info("[LUNGE] Target: '{}', Distance: {:.1f} units", target->GetName(), distance);
        logger::info("[LUNGE] Max lunge distance: {:.1f}, Speed: {:.1f}, Duration: {:.3f}s", 
            maxDistance, speed, duration);
        logger::info("[LUNGE] Player: ({:.1f}, {:.1f}, {:.1f}), Yaw: {:.2f} rad ({:.1f} deg)", 
            startPos.x, startPos.y, startPos.z, playerYaw, playerYaw * 57.2958f);
        logger::info("[LUNGE] Target: ({:.1f}, {:.1f}, {:.1f})", 
            targetPos.x, targetPos.y, targetPos.z);
        logger::info("[LUNGE] Direction to target: ({:.3f}, {:.3f}), Angle: {:.2f} rad ({:.1f} deg)", 
            worldDir.x, worldDir.y, targetAngle, targetAngle * 57.2958f);
        spdlog::default_logger()->flush();
        RE::DebugNotification("[TB] Lunge!");
    }
}

void CounterLungeState::ApplyVelocity(RE::bhkCharacterController* controller, float deltaTime)
{
    if (!active || !controller || deltaTime <= 0.0f) {
        return;
    }
    
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        Cancel();
        return;
    }
    
    // Verify this is the player's controller
    auto* playerController = player->GetCharController();
    if (playerController != controller) {
        return;  // Not the player's controller
    }
    
    // Check if still on ground
    if (controller->context.currentState != RE::hkpCharacterStateType::kOnGround) {
        Cancel();
        return;
    }
    
    // Get the target's CURRENT position (they may have moved)
    auto targetPtr = targetHandle.get();
    RE::Actor* target = targetPtr.get();
    if (!target || !target->Is3DLoaded()) {
        Cancel();
        return;
    }
    
    RE::NiPoint3 currentPos = player->GetPosition();
    RE::NiPoint3 targetPos = target->GetPosition();
    
    // Calculate current direction to target (horizontal only)
    RE::NiPoint3 toTarget = targetPos - currentPos;
    toTarget.z = 0.0f;
    float distanceToTarget = toTarget.Length();
    
    // Stop if we're close enough to the target
    if (distanceToTarget < MIN_LUNGE_DISTANCE) {
        auto* settings = Settings::GetSingleton();
        if (settings->bDebugLogging) {
            logger::info("[LUNGE] Reached target (distance: {:.1f}), ending lunge", distanceToTarget);
        }
        Cancel();
        return;
    }
    
    elapsed += deltaTime;
    
    // Check if lunge is complete by time
    if (elapsed >= duration) {
        Cancel();
        return;
    }
    
    // Check distance traveled from start
    RE::NiPoint3 fromStart = currentPos - startPos;
    fromStart.z = 0.0f;
    float traveled = fromStart.Length();
    
    if (traveled >= maxDistance) {
        Cancel();
        return;
    }
    
    // Calculate current speed using ease-out curve
    float t = elapsed / duration;
    float curveFactor = 1.0f - (t * t);  // Ease-out quadratic
    curveFactor = (std::max)(curveFactor, 0.2f);
    float currentSpeed = speed * curveFactor;
    
    // Normalize direction to target
    RE::NiPoint3 worldDirection = toTarget / distanceToTarget;
    
    // Get player's yaw angle
    float playerYaw = player->GetAngle().z;
    
    // Calculate the world angle of the direction to target
    // atan2(x, y) gives angle from +Y axis (north), positive = counterclockwise
    float targetAngle = std::atan2(worldDirection.x, worldDirection.y);
    
    // Calculate relative angle (how much to the left/right of player's facing)
    float relativeAngle = targetAngle - playerYaw;
    
    // Normalize to -PI to PI range
    while (relativeAngle > 3.14159f) relativeAngle -= 6.28318f;
    while (relativeAngle < -3.14159f) relativeAngle += 6.28318f;
    
    // Convert to local velocity
    // sin(0) = 0, cos(0) = 1 means straight ahead
    // sin(PI/2) = 1, cos(PI/2) = 0 means right
    float localX = std::sin(relativeAngle);  // Strafe (positive = right)
    float localY = std::cos(relativeAngle);  // Forward (positive = forward)
    
    // Apply to velocityMod
    auto* vel = reinterpret_cast<float*>(&(controller->velocityMod));
    vel[0] = currentSpeed * localX;   // Strafe
    vel[1] = currentSpeed * localY;   // Forward/back
    
    auto* settings = Settings::GetSingleton();
    if (settings->bDebugLogging) {
        // Log every few frames
        static int frameCounter = 0;
        if (++frameCounter % 10 == 0) {
            logger::info("[LUNGE] Frame {}: pos=({:.0f},{:.0f}), target=({:.0f},{:.0f}), dist={:.1f}", 
                frameCounter, currentPos.x, currentPos.y, targetPos.x, targetPos.y, distanceToTarget);
            logger::info("[LUNGE]   playerYaw={:.2f}deg, targetAngle={:.2f}deg, relAngle={:.2f}deg", 
                playerYaw * 57.2958f, targetAngle * 57.2958f, relativeAngle * 57.2958f);
            logger::info("[LUNGE]   localDir=({:.2f},{:.2f}), vel=({:.1f},{:.1f}), traveled={:.1f}/{:.1f}", 
                localX, localY, vel[0], vel[1], traveled, maxDistance);
            spdlog::default_logger()->flush();
        }
    }
}

bool CounterLungeState::IsActive()
{
    return active;
}

void CounterLungeState::Cancel()
{
    bool wasActive = active;
    
    if (active) {
        auto* settings = Settings::GetSingleton();
        if (settings->bDebugLogging) {
            logger::info("[LUNGE] Ended after {:.3f}s, traveled some distance", elapsed);
        }
    }
    
    active = false;
    elapsed = 0.0f;
    
    // Clear velocityMod to stop movement
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player) {
        if (auto* controller = player->GetCharController()) {
            auto* vel = reinterpret_cast<float*>(&(controller->velocityMod));
            vel[0] = 0.0f;
            vel[1] = 0.0f;
        }
    }
    
    // If lunge was active and slow time should start after lunge, trigger it now
    if (wasActive) {
        auto* settings = Settings::GetSingleton();
        if (settings->bEnableCounterSlowTime && settings->bCounterSlowStartAfterLunge) {
            // Check if slow time is armed (waiting for trigger)
            if (CounterSlowTimeState::waitingForStartEvent) {
                if (settings->bDebugLogging) {
                    logger::info("[LUNGE] Triggering slow time after lunge end");
                }
                CounterSlowTimeState::Start();
            }
        }
    }
}

//=============================================================================
// Lunge Physics Hook - hooks into Havok physics simulation
//=============================================================================

namespace LungePhysicsHook
{
    // Signature of bhkCharacterStateOnGround::SimulateStatePhysics
    using SimulateStatePhysics_t = void(RE::bhkCharacterStateOnGround*, RE::bhkCharacterController*);
    
    // Pointer to the original vfunc
    static REL::Relocation<SimulateStatePhysics_t> g_originalSimulate;
    
    // Our hook - runs BEFORE the original physics simulation
    void SimulateStatePhysics_Hook(RE::bhkCharacterStateOnGround* a_this, RE::bhkCharacterController* a_controller)
    {
        // Apply lunge velocity if active
        if (a_controller && CounterLungeState::IsActive()) {
            float deltaTime = a_controller->stepInfo.deltaTime;
            CounterLungeState::ApplyVelocity(a_controller, deltaTime);
        }
        
        // Run the original ground physics
        g_originalSimulate(a_this, a_controller);
    }
    
    void Install()
    {
        // Hook vfunc index 8 on bhkCharacterStateOnGround's vtable
        // Same slot used by CustomDodge for SimulateStatePhysics
        REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_bhkCharacterStateOnGround[0] };
        g_originalSimulate = vtbl.write_vfunc(8, SimulateStatePhysics_Hook);
        
        logger::info("Lunge physics hook installed");
    }
}

//=============================================================================
// Counter Slow Time State - slows time during counter attack animation
//=============================================================================

void CounterSlowTimeState::Arm()
{
    auto* settings = Settings::GetSingleton();
    if (!settings->bEnableCounterSlowTime) {
        return;
    }
    
    waitingForStartEvent = true;
    waitingForEndEvent = false;
    active = false;
    
    // Set max timeout
    maxEndTime = std::chrono::steady_clock::now() + 
        std::chrono::milliseconds(static_cast<long long>(settings->fCounterSlowTimeMaxDuration * 1000.0f + 
                                                         settings->fCounterAttackWindow * 1000.0f));
    
    if (settings->bDebugLogging) {
        if (settings->bCounterSlowStartAfterLunge) {
            logger::info("[COUNTER SLOWTIME] Armed - will start after lunge ends");
        } else {
            logger::info("[COUNTER SLOWTIME] Armed - waiting for '{}' event", settings->sCounterSlowStartEvent);
        }
    }
}

void CounterSlowTimeState::Start()
{
    auto* settings = Settings::GetSingleton();
    
    waitingForStartEvent = false;
    waitingForEndEvent = true;
    active = true;
    
    // Update max end time from when slow time starts
    maxEndTime = std::chrono::steady_clock::now() + 
        std::chrono::milliseconds(static_cast<long long>(settings->fCounterSlowTimeMaxDuration * 1000.0f));
    
    // Apply slow time
    SKSE::GetTaskInterface()->AddTask([scale = settings->fCounterSlowTimeScale]() {
        static REL::Relocation<float*> gtm{ RELOCATION_ID(511883, 388443) };
        *gtm = scale;
    });
    
    if (settings->bDebugLogging) {
        logger::info("[COUNTER SLOWTIME] Started at {}% - waiting for '{}' event", 
            settings->fCounterSlowTimeScale * 100.0f, settings->sCounterSlowEndEvent);
        RE::DebugNotification("[TB] Slow time!");
    }
}

void CounterSlowTimeState::End()
{
    if (!active && !waitingForStartEvent && !waitingForEndEvent) {
        return;
    }
    
    auto* settings = Settings::GetSingleton();
    
    waitingForStartEvent = false;
    waitingForEndEvent = false;
    active = false;
    
    // Restore normal time
    SKSE::GetTaskInterface()->AddTask([]() {
        static REL::Relocation<float*> gtm{ RELOCATION_ID(511883, 388443) };
        *gtm = 1.0f;
    });
    
    if (settings->bDebugLogging) {
        logger::info("[COUNTER SLOWTIME] Ended");
    }
}

void CounterSlowTimeState::Update()
{
    if (!waitingForStartEvent && !waitingForEndEvent && !active) {
        return;
    }
    
    // Check timeout
    auto now = std::chrono::steady_clock::now();
    if (now >= maxEndTime) {
        auto* settings = Settings::GetSingleton();
        if (settings->bDebugLogging) {
            logger::info("[COUNTER SLOWTIME] Timeout reached");
        }
        End();
    }
}

void CounterSlowTimeState::OnAnimEvent(const std::string_view& eventName)
{
    auto* settings = Settings::GetSingleton();
    
    // Only process START events if we're NOT using lunge trigger
    if (waitingForStartEvent && !settings->bCounterSlowStartAfterLunge) {
        // Check if this is the start event (case-insensitive contains)
        std::string eventLower(eventName);
        std::transform(eventLower.begin(), eventLower.end(), eventLower.begin(), ::tolower);
        std::string startLower = settings->sCounterSlowStartEvent;
        std::transform(startLower.begin(), startLower.end(), startLower.begin(), ::tolower);
        
        if (eventLower.find(startLower) != std::string::npos) {
            if (settings->bDebugLogging) {
                logger::info("[COUNTER SLOWTIME] Start event '{}' matched", eventName);
            }
            Start();
        }
    }
    // END events are always processed (regardless of how we started)
    else if (waitingForEndEvent && active) {
        // Check if this is the end event
        std::string eventLower(eventName);
        std::transform(eventLower.begin(), eventLower.end(), eventLower.begin(), ::tolower);
        std::string endLower = settings->sCounterSlowEndEvent;
        std::transform(endLower.begin(), endLower.end(), endLower.begin(), ::tolower);
        
        if (eventLower.find(endLower) != std::string::npos) {
            if (settings->bDebugLogging) {
                logger::info("[COUNTER SLOWTIME] End event '{}' matched", eventName);
            }
            End();
        }
    }
}

bool CounterSlowTimeState::IsActive()
{
    return active;
}

//=============================================================================
// Counter Animation Event Handler - listens for attack animation events
//=============================================================================

CounterAnimEventHandler* CounterAnimEventHandler::GetSingleton()
{
    static CounterAnimEventHandler singleton;
    return &singleton;
}

void CounterAnimEventHandler::Register()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player) {
        player->AddAnimationGraphEventSink(GetSingleton());
        logger::info("Counter attack animation event handler registered");
    }
}

RE::BSEventNotifyControl CounterAnimEventHandler::ProcessEvent(
    const RE::BSAnimationGraphEvent* a_event,
    RE::BSTEventSource<RE::BSAnimationGraphEvent>*)
{
    if (!a_event || !a_event->tag.data() || a_event->tag.empty()) {
        return RE::BSEventNotifyControl::kContinue;
    }
    
    // Forward animation events to the slow time state
    CounterSlowTimeState::OnAnimEvent(a_event->tag.c_str());
    
    return RE::BSEventNotifyControl::kContinue;
}

//=============================================================================
// Counter Damage Hit Handler - removes damage bonus after first hit
//=============================================================================

CounterDamageHitHandler* CounterDamageHitHandler::GetSingleton()
{
    static CounterDamageHitHandler singleton;
    return &singleton;
}

void CounterDamageHitHandler::Register()
{
    RE::ScriptEventSourceHolder* eventHolder = RE::ScriptEventSourceHolder::GetSingleton();
    if (eventHolder) {
        eventHolder->AddEventSink(GetSingleton());
        logger::info("Counter damage hit handler registered");
    }
}

RE::BSEventNotifyControl CounterDamageHitHandler::ProcessEvent(
    const RE::TESHitEvent* a_event,
    RE::BSTEventSource<RE::TESHitEvent>*)
{
    // Only process if damage bonus is active
    if (!CounterAttackState::IsDamageBonusActive()) {
        return RE::BSEventNotifyControl::kContinue;
    }
    
    if (!a_event) {
        return RE::BSEventNotifyControl::kContinue;
    }
    
    // Check if the CAUSE (attacker) is the player
    if (!a_event->cause) {
        return RE::BSEventNotifyControl::kContinue;
    }
    
    RE::Actor* attacker = a_event->cause->As<RE::Actor>();
    if (!attacker || !attacker->IsPlayerRef()) {
        return RE::BSEventNotifyControl::kContinue;
    }
    
    // Skip projectile hits (we only care about melee)
    if (a_event->projectile) {
        return RE::BSEventNotifyControl::kContinue;
    }
    
    // Player landed a melee hit - apply bonus damage directly
    RE::Actor* target = a_event->target ? a_event->target->As<RE::Actor>() : nullptr;
    
    if (!target) {
        return RE::BSEventNotifyControl::kContinue;
    }
    
    auto* settings = Settings::GetSingleton();
    auto* player = RE::PlayerCharacter::GetSingleton();
    
    // Calculate the bonus damage based on the weapon's base damage
    float baseDamage = 0.0f;
    auto* weapon = player ? player->GetEquippedObject(false) : nullptr;  // Right hand
    if (weapon && weapon->IsWeapon()) {
        auto* weap = weapon->As<RE::TESObjectWEAP>();
        if (weap) {
            baseDamage = static_cast<float>(weap->GetAttackDamage());
        }
    }
    
    // If no weapon or unarmed, use a base value
    if (baseDamage <= 0.0f) {
        baseDamage = 10.0f;  // Unarmed base
    }
    
    // Apply the player's damage bonuses (strength, perks, etc.)
    if (player) {
        float damageMult = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kAttackDamageMult);
        baseDamage *= damageMult;
    }
    
    // Calculate bonus damage
    float bonusPercent = settings->fCounterDamageBonusPercent / 100.0f;
    float bonusDamage = baseDamage * bonusPercent;
    
    // Apply the bonus damage directly to the target
    if (bonusDamage > 0.0f) {
        // Get target's current health for logging
        float targetHealthBefore = target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);
        
        // Deal the bonus damage
        target->AsActorValueOwner()->RestoreActorValue(
            RE::ACTOR_VALUE_MODIFIER::kDamage,
            RE::ActorValue::kHealth,
            -bonusDamage  // Negative to deal damage
        );
        
        float targetHealthAfter = target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);
        
        logger::info("[COUNTER DAMAGE] Hit '{}': Base damage ~{:.1f}, Bonus +{:.0f}% = +{:.1f} extra damage",
            target->GetName(), baseDamage, settings->fCounterDamageBonusPercent, bonusDamage);
        logger::info("[COUNTER DAMAGE] Target health: {:.1f} -> {:.1f} (actual damage applied: {:.1f})",
            targetHealthBefore, targetHealthAfter, targetHealthBefore - targetHealthAfter);
        spdlog::default_logger()->flush();
        
        if (settings->bDebugLogging) {
            RE::DebugNotification(fmt::format("[TB] Counter! +{:.0f} damage", bonusDamage).c_str());
        }
        
        // Play counter strike sound
        PlayCounterStrikeSound();
    }
    
    // Remove the damage bonus after applying it
    CounterAttackState::RemoveDamageBonus();
    
    return RE::BSEventNotifyControl::kContinue;
}
