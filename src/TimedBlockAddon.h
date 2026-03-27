#pragma once

// RELOCATION_OFFSET macro for SE/AE compatibility
#ifndef RELOCATION_OFFSET
#   ifdef SKYRIM_SUPPORT_AE
#       define RELOCATION_OFFSET(se, ae) ae
#   else
#       define RELOCATION_OFFSET(se, ae) se
#   endif
#endif

// Custom hash for RE::ActorHandle to use in unordered_map
struct ActorHandleHash {
    std::size_t operator()(RE::ActorHandle handle) const noexcept {
        return std::hash<std::uint32_t>{}(handle.native_handle());
    }
};

// Animation speed manager - freezes specific actors instead of the whole world
class AnimSpeedManager
{
public:
    static void SetAnimSpeed(RE::ActorHandle a_actorHandle, float a_speedMult, float a_duration);
    static void RevertAnimSpeed(RE::ActorHandle a_actorHandle);
    static void RevertAllAnimSpeed();
    static void Init();

private:
    struct AnimSpeedData
    {
        float speedMult;
        float duration;
    };
    
    // Use custom hash for ActorHandle
    static inline std::unordered_map<RE::ActorHandle, AnimSpeedData, ActorHandleHash> _animSpeeds;
    static inline std::mutex _animSpeedsLock;
    
    static void Update(RE::ActorHandle a_actorHandle, float& a_deltaTime);
    
    // Hook for player animations
    class PlayerAnimationHook
    {
    public:
        static void Install();
    private:
        static void PlayerCharacter_UpdateAnimation(RE::PlayerCharacter* a_this, float a_deltaTime);
        static inline REL::Relocation<decltype(PlayerCharacter_UpdateAnimation)> _originalFunc;
    };
    
    // Hook for NPC animations
    class NPCAnimationHook
    {
    public:
        static void Install();
    private:
        static void UpdateAnimationInternal(RE::Actor* a_this, float a_deltaTime);
        static inline REL::Relocation<decltype(UpdateAnimationInternal)> _originalFunc;
    };
};

// Cooldown state tracking - monitors parry window effect state
namespace CooldownState
{
    inline std::chrono::steady_clock::time_point cooldownEndTime;
    inline bool hadParryEffectLastFrame{ false };
    inline bool timedBlockTriggeredThisWindow{ false };
    inline bool inCooldown{ false };
    inline bool blockedByActiveCooldown{ false };  // Set when a block was prevented this frame
    
    // Optimization: Cache parry effect check (reset each frame)
    inline bool parryEffectCachedThisFrame{ false };
    inline bool cachedParryEffectResult{ false };
    
    // Optimization: Cache nearby enemy check (checked every 100ms instead of every frame)
    inline std::chrono::steady_clock::time_point lastEnemyCheckTime;
    inline bool cachedHasNearbyEnemy{ true };  // Fail-safe: assume enemy nearby
    constexpr auto ENEMY_CHECK_INTERVAL = std::chrono::milliseconds(100);  // 10 checks/sec
    
    // Get cached parry effect state (only checks once per frame)
    bool GetParryEffectCached();
    
    // Get cached nearby enemy state (only checks every ENEMY_CHECK_INTERVAL)
    bool GetNearbyEnemyCached();
    
    // Called each frame to track parry window effect transitions
    void Update();
    
    // Called when a timed block is successfully triggered - CLEARS cooldown
    void OnTimedBlockTriggered();
    
    // Start or restart the cooldown timer
    void StartCooldown(const char* reason);
    
    // Check if currently on cooldown (includes per-frame block flag)
    bool IsOnCooldown();
    
    // Internal check without the per-frame flag
    bool IsOnCooldownInternal();
    
    // Check if player has the parry window effect
    bool PlayerHasParryWindowEffect();
}

class TimedBlockAddon : public RE::BSTEventSink<RE::TESHitEvent> {
public:
    static TimedBlockAddon* GetSingleton();
    
    void Initialize();
    static void Register();
    
    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESHitEvent* a_event,
        RE::BSTEventSource<RE::TESHitEvent>* a_eventSource) override;

    // Effect methods
    void ApplyTimedBlockEffects(RE::Actor* defender, RE::Actor* attacker, bool skipSlowmo = false, bool fromTimedDodge = false);
    void PlayTimedBlockSound();
    void PlayCustomWavSound();
    void ApplySlowmo(float speed, float duration);
    
    // Form lookups
    bool LoadForms();
    
    // Check if actor has parry window effect (public for cooldown tracking)
    bool ActorHasParryWindowEffect(RE::Actor* actor);
    
    // Check if there's any enemy within range in combat with the player
    bool HasNearbyEnemyInCombat(RE::Actor* player, float radius);
    
private:
    TimedBlockAddon() = default;
    TimedBlockAddon(const TimedBlockAddon&) = delete;
    TimedBlockAddon(TimedBlockAddon&&) = delete;
    TimedBlockAddon& operator=(const TimedBlockAddon&) = delete;
    TimedBlockAddon& operator=(TimedBlockAddon&&) = delete;
    
    // Force attacker into stagger animation
    void TriggerStagger(RE::Actor* defender, RE::Actor* attacker, float magnitude);
    
    // Camera shake effect
    void ShakeCamera(float strength, const RE::NiPoint3& position, float duration);
    
    // Forms from SimpleTimedBlock.esp
    RE::EffectSetting* mgef_parry_window{ nullptr };
    RE::SpellItem* spell_parry_window{ nullptr };
};


// Offsets and function pointers
namespace Offsets
{
    // Camera shake function
    typedef void(__fastcall* tShakeCamera)(float strength, RE::NiPoint3 source, float duration);
    inline static REL::Relocation<tShakeCamera> ShakeCamera{ RELOCATION_ID(32275, 33012) };
}

// Dispels the parry window spell from the player (used when on cooldown)
void DispelParryWindowSpell();

// Counter Attack state tracking - allows attacking to cancel block animation
namespace CounterAttackState
{
    inline std::chrono::steady_clock::time_point windowEndTime;
    inline std::chrono::steady_clock::time_point damageBonusEndTime;
    inline bool inWindow{ false };
    inline RE::ActorHandle lastAttackerHandle;
    inline bool damageBonusActive{ false };
    inline float appliedDamageBonus{ 0.0f };
    inline bool fromTimedDodge{ false };
    
    void StartWindow(RE::Actor* attacker = nullptr);
    void Update();
    bool IsInWindow();
    void OnAttackInput();
    RE::Actor* GetLastAttacker();
    void ApplyDamageBonus();
    void RemoveDamageBonus();
    bool IsDamageBonusActive();
}

// Hit event handler to remove damage bonus after first hit
class CounterDamageHitHandler : public RE::BSTEventSink<RE::TESHitEvent> {
public:
    static CounterDamageHitHandler* GetSingleton();
    static void Register();
    
    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESHitEvent* a_event,
        RE::BSTEventSource<RE::TESHitEvent>* a_eventSource) override;
        
private:
    CounterDamageHitHandler() = default;
};

// Counter Lunge state tracking - moves player toward attacker
namespace CounterLungeState
{
    inline bool active{ false };
    inline bool loggedFirstFrame{ false };
    inline int  curveType{ 0 };
    inline float meleeStopDistance{ 128.0f };
    inline float elapsed{ 0.0f };
    inline float duration{ 0.0f };
    inline float totalDistance{ 0.0f };
    inline RE::NiPoint3 startPos{};
    inline RE::ActorHandle targetHandle{};
    
    // Start the lunge toward the target position
    void Start(RE::Actor* player, RE::Actor* target);
    
    // Move toward the target via velocityMod (called from physics hook)
    void ApplyVelocity(RE::bhkCharacterController* controller, float deltaTime);

    // Check if lunge is currently active
    bool IsActive();
    
    // Cancel the lunge
    void Cancel();
}

// Physics hook for lunge movement - hooks into Havok physics simulation
namespace LungePhysicsHook
{
    void Install();
}



// Counter Slow Time state tracking - slows time during counter attack
namespace CounterSlowTimeState
{
    inline bool active{ false };
    inline bool waitingForStartEvent{ false };
    inline bool waitingForEndEvent{ false };
    inline std::chrono::steady_clock::time_point maxEndTime;
    
    // Arm the slow time (wait for start event)
    void Arm();
    
    // Start slow time (called when start event is received)
    void Start();
    
    // End slow time (called when end event is received or timeout)
    void End();
    
    // Update each frame
    void Update();
    
    // Called when an animation event is received
    void OnAnimEvent(const std::string_view& eventName);
    
    // Check if slow time is active
    bool IsActive();
}

// Animation event handler for counter slow time
class CounterAnimEventHandler : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
public:
    static CounterAnimEventHandler* GetSingleton();
    static void Register();
    
    RE::BSEventNotifyControl ProcessEvent(
        const RE::BSAnimationGraphEvent* a_event,
        RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource) override;
        
private:
    CounterAnimEventHandler() = default;
};

// Timed Dodge State - tracks perfect dodge slow-motion, i-frames, radial blur
namespace TimedDodgeState
{
    // Core state
    inline bool active{ false };               // Overall timed dodge is in effect (slomo or blur fading)
    inline bool slomoActive{ false };          // Slow-motion is currently running
    inline bool iframesActive{ false };        // Player has i-frames (cannot be damaged)
    inline bool dodgeIframesEnded{ false };    // Dodge animation's own i-frames have ended, we own the graph vars
    inline bool counterWindowOpened{ false };  // Counter window has been opened for this timed dodge
    inline float trackedHealth{ 0.0f };        // Fallback health for non-MaxsuIFrame setups
    
    // Timing
    inline std::chrono::steady_clock::time_point effectStartTime;
    inline std::chrono::steady_clock::time_point effectEndTime;
    
    // Cooldown
    inline bool onCooldown{ false };
    inline std::chrono::steady_clock::time_point cooldownEndTime;

    // Early dodge forgiveness buffer
    inline bool pendingDodge{ false };
    inline std::chrono::steady_clock::time_point pendingDodgeExpiry;
    
    // Radial blur IMOD state (uses the source IMOD directly, saves/restores original values)
    inline RE::TESImageSpaceModifier* dodgeImod{ nullptr };
    inline RE::ImageSpaceModifierInstanceForm* dodgeImodInstance{ nullptr };
    inline bool blurEffectActive{ false };
    inline float currentBlurStrength{ 0.0f };
    inline float targetBlurStrength{ 0.0f };
    inline std::chrono::steady_clock::time_point lastBlurUpdateTime;
    
    // Original IMOD values to restore after blur effect ends
    inline float originalBlurStrength{ 0.0f };
    inline float originalBlurRampUp{ 0.0f };
    inline float originalBlurRampDown{ 0.0f };
    inline float originalBlurStart{ 0.0f };
    
    // Attacker tracking
    inline RE::ActorHandle attackerHandle;
    
    // Check if an animation event name is a dodge event
    bool IsDodgeEvent(const char* eventName);
    
    // Called when a dodge animation event fires on the player
    void OnAnimEvent(const char* eventName);
    
    // Attempt to trigger a timed dodge (checks for attacking enemies, cooldown, etc.)
    void OnDodgeEvent();
    
    // Start all timed dodge effects (slomo, i-frames, blur, counter window)
    void Start(RE::Actor* attacker);
    
    // End all timed dodge effects (restore game speed, remove i-frames, fade blur)
    void End();
    
    // Per-frame update: radial blur blending, i-frame health tracking, timer checks
    void Update();
    
    // Is the timed dodge currently active (slomo or blur still fading)?
    bool IsActive();
    
    // Is the slow-motion phase running?
    bool IsSlomoActive();
    
    // Cooldown management
    bool IsOnCooldown();
    void StartCooldown();
    
    // Called when the player is hit during i-frames (restores health)
    void OnPlayerHit(RE::Actor* player);
    
    // Create the radial blur IMOD (called once during initialization)
    void InitializeBlurIMOD();
    
    // Find the closest hostile enemy in melee attack swing phase within range
    RE::Actor* FindAttackingEnemyInRange(RE::Actor* player, float range);

    // Play the timed dodge WAV sound
    void PlayDodgeSound();
}

// Input event sink for detecting attack input during counter attack window
class CounterAttackInputHandler : public RE::BSTEventSink<RE::InputEvent*> {
public:
    static CounterAttackInputHandler* GetSingleton();
    static void Register();
    
    RE::BSEventNotifyControl ProcessEvent(
        RE::InputEvent* const* a_event,
        RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;
        
private:
    CounterAttackInputHandler() = default;
};
