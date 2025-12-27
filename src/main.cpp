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
    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%H:%M:%S] [%l] %v"s);
}

void MessageHandler(SKSE::MessagingInterface::Message* message) noexcept {
    switch (message->type) {
        case SKSE::MessagingInterface::kDataLoaded: {
            logger::info("Data loaded, initializing addon...");
            
            // Load settings
            Settings::GetSingleton()->LoadSettings();
            
            // Initialize the addon (loads forms from SimpleTimedBlock.esp and installs hooks)
            TimedBlockAddon::GetSingleton()->Initialize();
            
            // Register event sink
            TimedBlockAddon::Register();
            
            // Register SKSE Menu Framework menu
            Menu::Register();
            
            logger::info("Simple Timed Block Addons initialized!");
            break;
        }
        case SKSE::MessagingInterface::kPostLoadGame: {
            logger::debug("Post load game - reloading settings");
            Settings::GetSingleton()->LoadSettings();
            break;
        }
        case SKSE::MessagingInterface::kNewGame: {
            logger::debug("New game - reloading settings");
            Settings::GetSingleton()->LoadSettings();
            break;
        }
        default:
            break;
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    
    InitializeLog();
    
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
