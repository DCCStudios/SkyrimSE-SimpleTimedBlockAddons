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
    
    // Called each frame to track parry window effect transitions
    void Update();
    
    // Called when a timed block is successfully triggered
    void OnTimedBlockTriggered();
    
    // Check if currently on cooldown
    bool IsOnCooldown();
    
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
    void ApplyTimedBlockEffects(RE::Actor* defender, RE::Actor* attacker);
    void PlayTimedBlockSound();
    
    // Form lookups
    bool LoadForms();
    
    // Check if actor has parry window effect (public for cooldown tracking)
    bool ActorHasParryWindowEffect(RE::Actor* actor);
    
private:
    TimedBlockAddon() = default;
    TimedBlockAddon(const TimedBlockAddon&) = delete;
    TimedBlockAddon(TimedBlockAddon&&) = delete;
    TimedBlockAddon& operator=(const TimedBlockAddon&) = delete;
    TimedBlockAddon& operator=(TimedBlockAddon&&) = delete;
    
    // Force attacker into stagger animation
    void TriggerStagger(RE::Actor* defender, RE::Actor* attacker, float magnitude);
    
    // Push attacker away from defender
    void PushActorAway(RE::Actor* defender, RE::Actor* attacker, float magnitude);
    
    // Camera shake effect
    void ShakeCamera(float strength, const RE::NiPoint3& position, float duration);
    
    // Forms from SimpleTimedBlock.esp
    RE::EffectSetting* mgef_parry_window{ nullptr };
    RE::SpellItem* spell_parry_window{ nullptr };
};


// Offsets and function pointers
namespace Offsets
{
    // Push actor away function
    typedef void(__fastcall* tPushActorAway)(RE::AIProcess* a_causer, RE::Actor* a_target, RE::NiPoint3& a_origin, float a_magnitude);
    inline static REL::Relocation<tPushActorAway> PushActorAway{ RELOCATION_ID(38858, 39895) };
    
    // Camera shake function
    typedef void(__fastcall* tShakeCamera)(float strength, RE::NiPoint3 source, float duration);
    inline static REL::Relocation<tShakeCamera> ShakeCamera{ RELOCATION_ID(32275, 33012) };
}

// Hook to block parry window spell during cooldown
class SpellCastHook
{
public:
    static void Install();
    static void SetParryWindowSpell(RE::SpellItem* a_spell) { parryWindowSpell = a_spell; }
    
private:
    static inline RE::SpellItem* parryWindowSpell{ nullptr };
    
    // Hook for CastSpellImmediate
    static void CastSpellImmediate(RE::MagicCaster* a_caster, RE::MagicItem* a_spell, 
        bool a_noHitEffectArt, RE::TESObjectREFR* a_target, float a_effectiveness, 
        bool a_hostileEffectivenessOnly, float a_magnitudeOverride, RE::TESObjectREFR* a_cause);
    
    static inline REL::Relocation<decltype(CastSpellImmediate)> _originalCastSpell;
};
