#include "Settings.h"
#include "TimedBlockAddon.h"
#include "Papyrus.h"
#include "Menu.h"

void InitializeLog() {
    auto path = logger::log_directory();
    if (!path) {
        SKSE::stl::report_and_fail("Failed to find standard logging directory"sv);
    }

    *path /= "SimpleTimedBlockAddons.log"sv;
    
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

    auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

    // Start at debug level to capture early init messages; Settings::LoadSettings()
    // will tighten to info if bDebugLogging is false.
    log->set_level(spdlog::level::debug);
    log->flush_on(spdlog::level::debug);

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v"s);

    // Periodic flush so crash logs aren't lost
    spdlog::flush_every(std::chrono::seconds(3));
    
    logger::info("=== Simple Timed Block Addons Log ===");
    logger::info("Log file path: {}", path->string());
    spdlog::default_logger()->flush();
}

void MessageHandler(SKSE::MessagingInterface::Message* message) noexcept {
    switch (message->type) {
        case SKSE::MessagingInterface::kDataLoaded: {
            logger::info("Data loaded, initializing addon...");
            
            // Load settings
            Settings::GetSingleton()->LoadSettings();
            
            // Initialize the addon (loads forms from SimpleTimedBlock.esp and installs hooks)
            TimedBlockAddon::GetSingleton()->Initialize();
            
            // Register event sink for timed block detection
            TimedBlockAddon::Register();

            WardEffectHandler::Register();

            // Register with Precision for hitbox-level ward parry (preferred over TESHitEvent fallback).
            // Safe to call even if Precision is absent — logs a warning and disables the Precision path.
            WardTimedBlockState::RegisterPrecision();
            
            // Register counter damage hit handler (removes damage bonus after first hit)
            CounterDamageHitHandler::Register();
            
            // Initialize timed dodge radial blur IMOD (needs data handler to be ready)
            TimedDodgeState::InitializeBlurIMOD();
            
            // Register SKSE Menu Framework menu
            Menu::Register();
            
            logger::info("Simple Timed Block Addons initialized!");
            break;
        }
        case SKSE::MessagingInterface::kInputLoaded: {
            // Register counter attack input handler
            CounterAttackInputHandler::Register();
            logger::info("Counter attack input handler registered");
            break;
        }
        case SKSE::MessagingInterface::kPostLoadGame: {
            logger::info("Post load game - reloading settings");
            Settings::GetSingleton()->LoadSettings();
            
            // Create counter damage forms (deferred from kDataLoaded so other
            // plugins like SkyPatcher finish iterating spells first)
            if (!CounterAttackState::CreateCounterDamageForms()) {
                logger::error("Failed to create counter-attack damage MGEF/spell");
            }
            
            // Register animation event handler for counter slow time
            // (needs player to exist, so done here)
            CounterAnimEventHandler::Register();
            break;
        }
        case SKSE::MessagingInterface::kNewGame: {
            logger::info("New game - reloading settings");
            Settings::GetSingleton()->LoadSettings();
            
            if (!CounterAttackState::CreateCounterDamageForms()) {
                logger::error("Failed to create counter-attack damage MGEF/spell");
            }
            
            // Register animation event handler for counter slow time
            CounterAnimEventHandler::Register();
            break;
        }
        default:
            break;
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    // Initialize log FIRST, before SKSE::Init
    InitializeLog();
    
    SKSE::Init(skse);
    
    logger::info("Simple Timed Block Addons v1.1.0");
    logger::info("By: AI Assistant");
    logger::info("================================");
    
    // Allocate trampoline memory for animation hooks (needed for hitstop)
    SKSE::AllocTrampoline(64);
    
    // Register messaging listener
    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener(MessageHandler)) {
        logger::error("Failed to register messaging listener!");
        return false;
    }
    
    // Register Papyrus functions for MCM
    auto papyrus = SKSE::GetPapyrusInterface();
    if (!papyrus->Register(Papyrus::RegisterFunctions)) {
        logger::error("Failed to register Papyrus functions!");
        return false;
    }
    
    logger::info("Plugin loaded successfully!");
    
    return true;
}
