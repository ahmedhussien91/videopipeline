#include "video_pipeline/pipeline_manager.h"
#include "video_pipeline/block_registry.h"
#include "video_pipeline/config_parser.h"
#include "video_pipeline/logger.h"
#include <fstream>
#include <sstream>

namespace video_pipeline {

std::string Connection::ToString() const {
    return source_block + "." + source_output + " -> " + sink_block + "." + sink_input;
}

PipelineManager::PipelineManager() {
    // Set up error callback for blocks
    error_callback_ = [this](IBlock* block, const std::string& error) {
        OnBlockError(block, error);
    };
}

PipelineManager::~PipelineManager() {
    Shutdown();
}

bool PipelineManager::Initialize(const PipelineConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (is_running_.load()) {
        last_error_ = "Cannot initialize while pipeline is running";
        VP_LOG_ERROR(last_error_);
        return false;
    }
    
    // Store configuration
    config_ = config;
    
    VP_LOG_INFO_F("Initializing pipeline: {}", config_.name);
    VP_LOG_INFO_F("Platform: {}", config_.platform);
    VP_LOG_INFO_F("Blocks: {}, Connections: {}", config_.blocks.size(), config_.connections.size());
    
    // Create and initialize blocks
    if (!CreateBlocks()) {
        return false;
    }
    
    if (!ConfigureBlocks()) {
        return false;
    }
    
    if (!ConnectBlocks()) {
        return false;
    }
    
    VP_LOG_INFO_F("Pipeline '{}' initialized successfully", config_.name);
    return true;
}

bool PipelineManager::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (blocks_.empty()) {
        last_error_ = "No blocks to start. Call Initialize() first.";
        VP_LOG_ERROR(last_error_);
        return false;
    }
    
    if (is_running_.load()) {
        VP_LOG_WARNING("Pipeline already running");
        return true;
    }
    
    VP_LOG_INFO_F("Starting pipeline: {}", config_.name);
    
    // Start all blocks in dependency order
    // For now, start sinks first, then sources
    std::vector<BlockPtr> sinks, sources, others;
    
    for (const auto& pair : blocks_) {
        auto source = std::dynamic_pointer_cast<IVideoSource>(pair.second);
        auto sink = std::dynamic_pointer_cast<IVideoSink>(pair.second);
        
        if (source) {
            sources.push_back(pair.second);
        } else if (sink) {
            sinks.push_back(pair.second);
        } else {
            others.push_back(pair.second);
        }
    }
    
    // Start in order: sinks, others, sources
    for (auto block : sinks) {
        if (!block->Start()) {
            last_error_ = "Failed to start sink: " + block->GetName();
            VP_LOG_ERROR(last_error_);
            return false;
        }
    }
    
    for (auto block : others) {
        if (!block->Start()) {
            last_error_ = "Failed to start block: " + block->GetName();
            VP_LOG_ERROR(last_error_);
            return false;
        }
    }
    
    for (auto block : sources) {
        if (!block->Start()) {
            last_error_ = "Failed to start source: " + block->GetName();
            VP_LOG_ERROR(last_error_);
            return false;
        }
    }
    
    is_running_.store(true);
    VP_LOG_INFO_F("Pipeline '{}' started successfully", config_.name);
    return true;
}

bool PipelineManager::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_running_.load()) {
        return true;
    }
    
    VP_LOG_INFO_F("Stopping pipeline: {}", config_.name);
    
    // Stop blocks in reverse order: sources first, then others, then sinks
    std::vector<BlockPtr> sinks, sources, others;
    
    for (const auto& pair : blocks_) {
        auto source = std::dynamic_pointer_cast<IVideoSource>(pair.second);
        auto sink = std::dynamic_pointer_cast<IVideoSink>(pair.second);
        
        if (source) {
            sources.push_back(pair.second);
        } else if (sink) {
            sinks.push_back(pair.second);
        } else {
            others.push_back(pair.second);
        }
    }
    
    // Stop in reverse order
    for (auto block : sources) {
        block->Stop();
    }
    
    for (auto block : others) {
        block->Stop();
    }
    
    for (auto block : sinks) {
        block->Stop();
    }
    
    is_running_.store(false);
    VP_LOG_INFO_F("Pipeline '{}' stopped", config_.name);
    return true;
}

bool PipelineManager::Shutdown() {
    Stop();
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Shutdown all blocks
    for (const auto& pair : blocks_) {
        pair.second->Shutdown();
    }
    
    blocks_.clear();
    config_ = PipelineConfig{};
    
    VP_LOG_INFO("Pipeline shutdown complete");
    return true;
}

bool PipelineManager::IsRunning() const {
    return is_running_.load();
}

std::string PipelineManager::GetStatus() const {
    std::ostringstream oss;
    oss << "Pipeline: " << config_.name << "\n";
    oss << "State: " << (IsRunning() ? "RUNNING" : "STOPPED") << "\n";
    oss << "Blocks: " << blocks_.size() << "\n";
    
    for (const auto& pair : blocks_) {
        oss << "  " << pair.first << " [" << pair.second->GetType() << "] - " 
            << pair.second->GetStateString() << "\n";
    }
    
    return oss.str();
}

BlockPtr PipelineManager::GetBlock(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = blocks_.find(name);
    return (it != blocks_.end()) ? it->second : nullptr;
}

std::vector<BlockPtr> PipelineManager::GetBlocks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<BlockPtr> blocks;
    blocks.reserve(blocks_.size());
    
    for (const auto& pair : blocks_) {
        blocks.push_back(pair.second);
    }
    
    return blocks;
}

std::vector<std::string> PipelineManager::GetBlockNames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(blocks_.size());
    
    for (const auto& pair : blocks_) {
        names.push_back(pair.first);
    }
    
    return names;
}

std::map<std::string, BlockStats> PipelineManager::GetAllStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::string, BlockStats> stats;
    
    for (const auto& pair : blocks_) {
        stats[pair.first] = pair.second->GetStats();
    }
    
    return stats;
}

void PipelineManager::ResetAllStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& pair : blocks_) {
        pair.second->ResetStats();
    }
    
    VP_LOG_INFO("All block statistics reset");
}

bool PipelineManager::LoadConfiguration(const std::string& config_file) {
    // Read file content
    std::ifstream file(config_file);
    if (!file.is_open()) {
        last_error_ = "Failed to open config file: " + config_file;
        VP_LOG_ERROR(last_error_);
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    // Determine format from file extension
    std::string format = "yaml";  // default
    size_t dot_pos = config_file.find_last_of('.');
    if (dot_pos != std::string::npos) {
        std::string ext = config_file.substr(dot_pos + 1);
        if (ext == "json") format = "json";
        else if (ext == "ini" || ext == "conf") format = "simple";
    }
    
    return LoadConfigurationFromString(buffer.str(), format);
}

bool PipelineManager::LoadConfigurationFromString(const std::string& config_content, const std::string& format) {
    auto parser = ConfigParserFactory::CreateParser(format);
    if (!parser) {
        last_error_ = "Unsupported configuration format: " + format;
        VP_LOG_ERROR(last_error_);
        return false;
    }
    
    PipelineConfig config;
    if (!parser->Parse(config_content, config)) {
        last_error_ = "Failed to parse configuration: " + parser->GetLastError();
        VP_LOG_ERROR(last_error_);
        return false;
    }
    
    return Initialize(config);
}

void PipelineManager::SetErrorCallback(ErrorCallback callback) {
    error_callback_ = callback;
}

std::string PipelineManager::GetLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

bool PipelineManager::CreateBlocks() {
    blocks_.clear();
    
    auto& registry = BlockRegistry::Instance();
    
    for (const auto& block_def : config_.blocks) {
        VP_LOG_DEBUG_F("Creating block '{}' of type '{}'", block_def.name, block_def.type);
        
        auto block = registry.CreateBlock(block_def.type, block_def.name);
        if (!block) {
            last_error_ = "Failed to create block '" + block_def.name + "' of type '" + block_def.type + "'";
            VP_LOG_ERROR(last_error_);
            return false;
        }
        
        // Set error callback
        block->SetErrorCallback(error_callback_);
        
        blocks_[block_def.name] = block;
    }
    
    VP_LOG_INFO_F("Created {} blocks", blocks_.size());
    return true;
}

bool PipelineManager::ConfigureBlocks() {
    for (const auto& block_def : config_.blocks) {
        auto block = blocks_[block_def.name];
        if (!block) {
            last_error_ = "Block not found: " + block_def.name;
            return false;
        }
        
        VP_LOG_DEBUG_F("Configuring block '{}'", block_def.name);
        
        // Set parameters
        for (const auto& param : block_def.parameters) {
            block->SetParameter(param.first, param.second);
        }
        
        // Initialize block
        if (!block->Initialize(block_def.parameters)) {
            last_error_ = "Failed to initialize block '" + block_def.name + "': " + block->GetLastError();
            VP_LOG_ERROR(last_error_);
            return false;
        }
    }
    
    VP_LOG_INFO("All blocks configured successfully");
    return true;
}

bool PipelineManager::ConnectBlocks() {
    for (const auto& connection : config_.connections) {
        VP_LOG_DEBUG_F("Connecting: {}", connection.ToString());
        
        auto source_block = blocks_.find(connection.source_block);
        auto sink_block = blocks_.find(connection.sink_block);
        
        if (source_block == blocks_.end()) {
            last_error_ = "Source block not found: " + connection.source_block;
            VP_LOG_ERROR(last_error_);
            return false;
        }
        
        if (sink_block == blocks_.end()) {
            last_error_ = "Sink block not found: " + connection.sink_block;
            VP_LOG_ERROR(last_error_);
            return false;
        }
        
        // Connect source to sink
        auto source = std::dynamic_pointer_cast<IVideoSource>(source_block->second);
        auto sink = std::dynamic_pointer_cast<IVideoSink>(sink_block->second);
        
        if (!source) {
            last_error_ = "Block '" + connection.source_block + "' is not a video source";
            VP_LOG_ERROR(last_error_);
            return false;
        }
        
        if (!sink) {
            last_error_ = "Block '" + connection.sink_block + "' is not a video sink";
            VP_LOG_ERROR(last_error_);
            return false;
        }
        
        // Set up frame callback
        source->SetFrameCallback([sink](VideoFramePtr frame) {
            sink->ProcessFrame(frame);
        });
        
        // Match formats if possible
        auto output_format = source->GetOutputFormat();
        if (!sink->SupportsFormat(output_format.pixel_format)) {
            VP_LOG_WARNING_F("Format mismatch between '{}' and '{}'", 
                           connection.source_block, connection.sink_block);
        } else {
            sink->SetInputFormat(output_format);
        }
    }
    
    VP_LOG_INFO_F("Connected {} block pairs", config_.connections.size());
    return true;
}

void PipelineManager::OnBlockError(IBlock* block, const std::string& error) {
    VP_LOG_ERROR_F("Block '{}' error: {}", block ? block->GetName() : "unknown", error);
    
    // For now, just log the error. In a production system, you might want to
    // implement more sophisticated error handling like restarting blocks,
    // switching to backup sources, etc.
}

} // namespace video_pipeline