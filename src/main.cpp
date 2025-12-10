#include "video_pipeline/video_pipeline.h"
#include "video_pipeline/blocks/test_pattern_source.h"
#include "video_pipeline/blocks/file_sink.h"
#include "video_pipeline/blocks/console_sink.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>
#include <iomanip>
#include <memory>

using namespace video_pipeline;

// Global variables for signal handling
static std::atomic<bool> g_shutdown_requested{false};
static std::unique_ptr<PipelineManager> g_pipeline;

// Signal handler
void SignalHandler(int signal) {
    VP_LOG_INFO_F("Received signal {}, requesting shutdown", signal);
    g_shutdown_requested.store(true);
    
    if (g_pipeline) {
        g_pipeline->Stop();
    }
}

void PrintUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n"
              << "\n"
              << "Options:\n"
              << "  -c, --config <file>     Configuration file (YAML/JSON/simple)\n"
              << "  -t, --time <seconds>    Run for specified time (0 = infinite)\n"
              << "  -v, --verbose           Enable verbose logging\n"
              << "  -l, --log-file <file>   Log to file instead of console\n"
              << "  -s, --stats             Print statistics every second\n"
              << "  -h, --help              Show this help message\n"
              << "\n"
              << "Examples:\n"
              << "  " << program_name << " --config pipeline.yaml --time 10\n"
              << "  " << program_name << " --verbose --stats\n"
              << "\n"
              << "Without a config file, a default test pattern -> console pipeline will be created.\n";
}

PipelineConfig CreateDefaultConfig() {
    PipelineConfig config;
    config.name = "default_test_pipeline";
    config.platform = "generic";
    
    // Test pattern source
    PipelineConfig::BlockDef source;
    source.name = "test_source";
    source.type = "TestPatternSource";
    source.parameters["width"] = "640";
    source.parameters["height"] = "480";
    source.parameters["fps"] = "30";
    source.parameters["pattern"] = "bars";
    config.blocks.push_back(source);
    
    // Console sink
    PipelineConfig::BlockDef sink;
    sink.name = "console_sink";
    sink.type = "ConsoleSink";
    sink.parameters["verbose"] = "false";
    config.blocks.push_back(sink);
    
    // Connection
    Connection conn;
    conn.source_block = "test_source";
    conn.sink_block = "console_sink";
    config.connections.push_back(conn);
    
    return config;
}

void PrintStatistics(const PipelineManager& pipeline) {
    auto stats = pipeline.GetAllStats();
    
    std::cout << "\n=== Pipeline Statistics ===\n";
    for (const auto& pair : stats) {
        const auto& block_name = pair.first;
        const auto& block_stats = pair.second;
        
        std::cout << block_name << ":\n";
        std::cout << "  Frames processed: " << block_stats.frames_processed << "\n";
        std::cout << "  Frames dropped: " << block_stats.frames_dropped << "\n";
        std::cout << "  Bytes processed: " << block_stats.bytes_processed << "\n";
        std::cout << "  Average FPS: " << std::fixed << std::setprecision(1) << block_stats.avg_fps << "\n";
        std::cout << "  Average latency: " << std::fixed << std::setprecision(2) << block_stats.avg_latency_ms << "ms\n";
        std::cout << "  Queue depth: " << block_stats.queue_depth << "\n";
        std::cout << "\n";
    }
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string config_file;
    int run_time_seconds = 0;
    bool verbose = false;
    std::string log_file;
    bool show_stats = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            PrintUsage(argv[0]);
            return 0;
        }
        else if (arg == "-c" || arg == "--config") {
            if (++i < argc) {
                config_file = argv[i];
            } else {
                std::cerr << "Error: --config requires a filename\n";
                return 1;
            }
        }
        else if (arg == "-t" || arg == "--time") {
            if (++i < argc) {
                run_time_seconds = std::atoi(argv[i]);
            } else {
                std::cerr << "Error: --time requires a number\n";
                return 1;
            }
        }
        else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        }
        else if (arg == "-l" || arg == "--log-file") {
            if (++i < argc) {
                log_file = argv[i];
            } else {
                std::cerr << "Error: --log-file requires a filename\n";
                return 1;
            }
        }
        else if (arg == "-s" || arg == "--stats") {
            show_stats = true;
        }
        else {
            std::cerr << "Error: Unknown option " << arg << "\n";
            PrintUsage(argv[0]);
            return 1;
        }
    }
    
    // Initialize framework
    if (!Framework::Initialize()) {
        std::cerr << "Failed to initialize video pipeline framework\n";
        return 1;
    }
    
    // Set up logging
    if (!log_file.empty()) {
        auto file_logger = std::make_shared<FileLogger>(log_file);
        if (file_logger->IsOpen()) {
            Logger::SetLogger(file_logger);
        } else {
            std::cerr << "Warning: Failed to open log file " << log_file << "\n";
        }
    }
    
    if (verbose) {
        Framework::SetLogLevel(LogLevel::DEBUG);
    }
    
    // Register blocks
    auto& registry = BlockRegistry::Instance();
    registry.RegisterBlock("TestPatternSource", []() -> BlockPtr {
        return std::make_shared<TestPatternSource>();
    });
    registry.RegisterBlock("ConsoleSink", []() -> BlockPtr {
        return std::make_shared<ConsoleSink>();
    });
    registry.RegisterBlock("FileSink", []() -> BlockPtr {
        return std::make_shared<FileSink>();
    });
    
    VP_LOG_INFO_F("Video Pipeline Framework v{}", Framework::GetVersion());
    VP_LOG_INFO_F("Registered {} block types", registry.GetRegisteredCount());
    
    // Create and initialize pipeline
    g_pipeline = std::make_unique<PipelineManager>();
    
    bool success = false;
    if (!config_file.empty()) {
        VP_LOG_INFO_F("Loading configuration from: {}", config_file);
        success = g_pipeline->LoadConfiguration(config_file);
    } else {
        VP_LOG_INFO("Using default configuration");
        auto default_config = CreateDefaultConfig();
        success = g_pipeline->Initialize(default_config);
    }
    
    if (!success) {
        std::cerr << "Failed to initialize pipeline: " << g_pipeline->GetLastError() << "\n";
        Framework::Shutdown();
        return 1;
    }
    
    // Set up signal handlers
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
    
    // Start pipeline
    VP_LOG_INFO("Starting pipeline...");
    if (!g_pipeline->Start()) {
        std::cerr << "Failed to start pipeline: " << g_pipeline->GetLastError() << "\n";
        Framework::Shutdown();
        return 1;
    }
    
    std::cout << "Pipeline started. Press Ctrl+C to stop.\n";
    std::cout << g_pipeline->GetStatus() << "\n";
    
    // Main loop
    Timer runtime_timer;
    Timer stats_timer;
    
    while (!g_shutdown_requested.load() && g_pipeline->IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Check runtime limit
        if (run_time_seconds > 0 && runtime_timer.GetElapsedSeconds() >= run_time_seconds) {
            VP_LOG_INFO_F("Runtime limit reached ({} seconds)", run_time_seconds);
            break;
        }
        
        // Print statistics
        if (show_stats && stats_timer.GetElapsedSeconds() >= 1.0) {
            PrintStatistics(*g_pipeline);
            stats_timer.Reset();
        }
    }
    
    // Stop pipeline
    VP_LOG_INFO("Stopping pipeline...");
    g_pipeline->Stop();
    
    // Print final statistics
    if (show_stats) {
        std::cout << "\n=== Final Statistics ===\n";
        PrintStatistics(*g_pipeline);
    }
    
    // Shutdown
    g_pipeline->Shutdown();
    g_pipeline.reset();
    
    VP_LOG_INFO_F("Pipeline ran for {}", runtime_timer.ToString());
    VP_LOG_INFO("Shutting down framework");
    Framework::Shutdown();
    
    return 0;
}