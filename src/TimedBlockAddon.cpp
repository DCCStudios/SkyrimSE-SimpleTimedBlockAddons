#include "TimedBlockAddon.h"
#include "Settings.h"

// Undefine Windows PlaySound macro
#ifdef PlaySound
#undef PlaySound
#endif

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

void CooldownState::Update()
{
    auto* settings = Settings::GetSingleton();
    if (!settings->bEnableCooldown) {
        return;
    }
    
    bool hasParryEffectNow = PlayerHasParryWindowEffect();
    
    // Detect transition: had effect last frame, doesn't have it now = window ended
    if (hadParryEffectLastFrame && !hasParryEffectNow) {
        // Parry window just ended
        if (!timedBlockTriggeredThisWindow) {
            // No timed block was triggered during this window - start cooldown
            inCooldown = true;
            cooldownEndTime = std::chrono::steady_clock::now() + 
                std::chrono::milliseconds(static_cast<long long>(settings->fCooldownDurationMs));
            
            if (settings->bDebugLogging) {
                logger::info("[COOLDOWN] Parry window ENDED without timed block - STARTING {}ms cooldown", 
                    settings->fCooldownDurationMs);
            }
        } else {
            if (settings->bDebugLogging) {
                logger::info("[COOLDOWN] Parry window ENDED - timed block was triggered, NO NEW cooldown started");
            }
        }
        
        // Reset for next window
        timedBlockTriggeredThisWindow = false;
    }
    
    // Detect new parry window starting (only if spell wasn't blocked)
    if (!hadParryEffectLastFrame && hasParryEffectNow) {
        timedBlockTriggeredThisWindow = false;
        
        if (settings->bDebugLogging) {
            logger::info("[COOLDOWN] Parry window STARTED (current cooldown active: {})", inCooldown);
        }
    }
    
    hadParryEffectLastFrame = hasParryEffectNow;
}

void CooldownState::OnTimedBlockTriggered()
{
    timedBlockTriggeredThisWindow = true;
    // NOTE: We do NOT reset inCooldown here - cooldown only expires based on timer
    
    auto* settings = Settings::GetSingleton();
    if (settings->bDebugLogging) {
        logger::info("[COOLDOWN] Timed block triggered! timedBlockTriggeredThisWindow=true");
    }
}

bool CooldownState::IsOnCooldown()
{
    auto* settings = Settings::GetSingleton();
    
    if (!inCooldown) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    if (now >= cooldownEndTime) {
        inCooldown = false;
        
        if (settings->bDebugLogging) {
            logger::info("[COOLDOWN] Cooldown EXPIRED by timer");
        }
        return false;
    }
    
    // Calculate remaining time for logging
    if (settings->bDebugLogging) {
        auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(cooldownEndTime - now).count();
        logger::info("[COOLDOWN] IsOnCooldown() check: YES, {}ms remaining", remainingMs);
    }
    
    return true;
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
    
    // Install spell cast hook to block parry window spell during cooldown
    SpellCastHook::Install();
    
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
    const RE::FormID parryWindowSpellID = 0x800;  // The spell that applies the effect
    
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
        logger::warn("Failed to find parry window spell (0x800) in SimpleTimedBlock.esp - cooldown spell blocking disabled");
    } else {
        logger::debug("Found parry window spell: {}", spell_parry_window->GetName());
        SpellCastHook::SetParryWindowSpell(spell_parry_window);
    }
    
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
    
    // Check if player has the parry window effect (indicating a timed block)
    if (!ActorHasParryWindowEffect(defender)) {
        return Result::kContinue;
    }
    
    // Check if on cooldown
    auto* settings = Settings::GetSingleton();
    
    if (settings->bDebugLogging) {
        logger::info("[HITEVENT] Blocked hit detected with parry window effect active!");
    }
    
    if (settings->bEnableCooldown && CooldownState::IsOnCooldown()) {
        logger::info("[HITEVENT] Timed block HIT detected but on cooldown - SKIPPING addon effects");
        return Result::kContinue;
    }
    
    // Get the attacker
    RE::Actor* attacker = a_event->cause ? a_event->cause->As<RE::Actor>() : nullptr;
    if (!attacker) {
        return Result::kContinue;
    }
    
    // Mark that a timed block was triggered (for cooldown tracking)
    CooldownState::OnTimedBlockTriggered();
    
    logger::info("[HITEVENT] TIMED BLOCK SUCCESS! Applying addon effects...");
    
    // Apply all effects
    ApplyTimedBlockEffects(defender, attacker);
    
    return Result::kContinue;
}

void TimedBlockAddon::ApplyTimedBlockEffects(RE::Actor* defender, RE::Actor* attacker) {
    auto settings = Settings::GetSingleton();
    
    // 1. Freeze attacker's animation (hitstop effect - does NOT slow down the world)
    if (settings->bEnableHitstop && attacker) {
        float hitstopSpeed = settings->fHitstopSpeed;  // 0.0 = complete freeze, 0.1 = very slow
        float hitstopDuration = settings->fHitstopDuration;
        
        logger::debug("Applying hitstop to attacker: speed={}, duration={}s", hitstopSpeed, hitstopDuration);
        AnimSpeedManager::SetAnimSpeed(attacker->GetHandle(), hitstopSpeed, hitstopDuration);
    }
    
    // 2. Force attacker into stagger animation
    if (settings->bEnableStagger && attacker && defender) {
        float staggerMagnitude = settings->fStaggerMagnitude;
        logger::debug("Triggering stagger on attacker with magnitude: {}", staggerMagnitude);
        TriggerStagger(defender, attacker, staggerMagnitude);
    }
    
    // 3. Push attacker away (optional, disabled by default)
    if (settings->bEnablePushback && attacker && defender) {
        float pushMagnitude = settings->fPushbackMagnitude;
        logger::debug("Pushing attacker away with magnitude: {}", pushMagnitude);
        PushActorAway(defender, attacker, pushMagnitude);
    }
    
    // 4. Camera shake for impact feel
    if (settings->bEnableCameraShake && defender) {
        float shakeStrength = settings->fCameraShakeStrength;
        float shakeDuration = settings->fCameraShakeDuration;
        logger::debug("Applying camera shake: strength={}, duration={}s", shakeStrength, shakeDuration);
        ShakeCamera(shakeStrength, defender->GetPosition(), shakeDuration);
    }
    
    // 5. Play sound effect
    if (settings->bEnableSound) {
        PlayTimedBlockSound();
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

void TimedBlockAddon::PushActorAway(RE::Actor* defender, RE::Actor* attacker, float magnitude) {
    if (!defender || !attacker || magnitude <= 0.0f) {
        return;
    }
    
    // Use the DEFENDER's process (they are the one doing the pushing/blocking)
    auto process = defender->GetActorRuntimeData().currentProcess;
    if (!process) {
        return;
    }
    
    // Push origin is the defender's position - attacker will be pushed AWAY from this point
    RE::NiPoint3 pushOrigin = defender->GetPosition();
    
    // Use the game's native push function
    // This pushes the attacker away from the pushOrigin
    Offsets::PushActorAway(process, attacker, pushOrigin, magnitude);
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
    
    if (settings->sSoundPath.empty()) {
        return;
    }
    
    logger::debug("Playing timed block sound: {}", settings->sSoundPath);
    
    // Play the sound on main thread
    SKSE::GetTaskInterface()->AddTask([soundPath = settings->sSoundPath]() {
        // If it looks like an EditorID (no path separators or extension)
        if (soundPath.find("\\") == std::string::npos && 
            soundPath.find("/") == std::string::npos &&
            soundPath.find(".") == std::string::npos) {
            // Play as sound descriptor EditorID
            RE::PlaySound(soundPath.c_str());
            logger::debug("Played sound descriptor: {}", soundPath);
        } else {
            // For file paths, custom WAV files require a sound descriptor form in an ESP
            // As fallback, play the default UI sound
            RE::PlaySound("UIMenuOK");
            logger::debug("Playing fallback UI sound (custom WAV requires sound descriptor in ESP)");
        }
    });
}

//=============================================================================
// SpellCastHook Implementation - Blocks parry window spell during cooldown
//=============================================================================

void SpellCastHook::Install()
{
    // Hook MagicCaster::CastSpellImmediate
    // This is called when instant spells are cast (like the parry window spell)
    REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_ActorMagicCaster[0] };
    _originalCastSpell = vtbl.write_vfunc(0x1D, CastSpellImmediate);  // CastSpellImmediate is vtable index 0x1D
    
    logger::info("Installed SpellCastHook for parry window cooldown blocking (vtable index 0x1D)");
    if (parryWindowSpell) {
        logger::info("  Parry window spell FormID: {:08X}", parryWindowSpell->GetFormID());
    } else {
        logger::warn("  WARNING: Parry window spell is null - cooldown blocking will not work!");
    }
}

void SpellCastHook::CastSpellImmediate(RE::MagicCaster* a_caster, RE::MagicItem* a_spell, 
    bool a_noHitEffectArt, RE::TESObjectREFR* a_target, float a_effectiveness, 
    bool a_hostileEffectivenessOnly, float a_magnitudeOverride, RE::TESObjectREFR* a_cause)
{
    auto* settings = Settings::GetSingleton();
    
    // Log ALL spell casts when debug logging is enabled (to verify hook is working)
    if (settings->bDebugLogging && a_spell) {
        logger::info("[SPELLHOOK] CastSpellImmediate called - Spell: {} (FormID: {:08X})", 
            a_spell->GetName(), a_spell->GetFormID());
    }
    
    // Check if this is the parry window spell being cast on the player during cooldown
    if (parryWindowSpell && a_spell == parryWindowSpell) {
        if (settings->bDebugLogging) {
            logger::info("[SPELLHOOK] PARRY WINDOW SPELL detected!");
        }
        
        // Check if cooldown is enabled and active
        if (settings->bEnableCooldown && CooldownState::IsOnCooldown()) {
            // Check if target is the player
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (a_target && a_target == player) {
                logger::info("[COOLDOWN] *** BLOCKED parry window spell - cooldown is ACTIVE ***");
                // Don't cast the spell - just return without calling original
                return;
            }
        } else {
            if (settings->bDebugLogging) {
                logger::info("[SPELLHOOK] Parry window spell ALLOWED (cooldown not active or disabled)");
            }
        }
    }
    
    // Call original function for all other spells or if not on cooldown
    _originalCastSpell(a_caster, a_spell, a_noHitEffectArt, a_target, a_effectiveness, 
        a_hostileEffectivenessOnly, a_magnitudeOverride, a_cause);
}
