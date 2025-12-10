#include "video_pipeline/config_parser.h"
#include "video_pipeline/logger.h"
#include <sstream>
#include <regex>

#ifdef HAVE_YAML_CPP
#include <yaml-cpp/yaml.h>
#endif

namespace video_pipeline {

ConfigParserPtr ConfigParserFactory::CreateParser(const std::string& format) {
    if (format == "json") {
        return std::make_unique<JsonConfigParser>();
    }
#ifdef HAVE_YAML_CPP
    else if (format == "yaml" || format == "yml") {
        return std::make_unique<YamlConfigParser>();
    }
#endif
    else if (format == "simple" || format == "ini") {
        return std::make_unique<SimpleConfigParser>();
    }
    
    return nullptr;
}

std::vector<std::string> ConfigParserFactory::GetSupportedFormats() {
    std::vector<std::string> formats = {"json", "simple", "ini"};
#ifdef HAVE_YAML_CPP
    formats.push_back("yaml");
    formats.push_back("yml");
#endif
    return formats;
}

// Simple JSON parser (basic implementation)
bool JsonConfigParser::Parse(const std::string& content, PipelineConfig& config) {
    // This is a very basic JSON parser - in production you'd use a proper JSON library
    last_error_ = "JSON parsing not fully implemented yet. Use simple format instead.";
    VP_LOG_ERROR(last_error_);
    return false;
}

#ifdef HAVE_YAML_CPP
// YAML parser implementation
bool YamlConfigParser::Parse(const std::string& content, PipelineConfig& config) {
    try {
        YAML::Node root = YAML::Load(content);
        
        // Parse pipeline info
        if (root["pipeline"]) {
            auto pipeline_node = root["pipeline"];
            config.name = pipeline_node["name"].as<std::string>("unnamed");
            config.platform = pipeline_node["platform"].as<std::string>("generic");
        }
        
        // Parse blocks
        if (root["blocks"]) {
            for (const auto& block_node : root["blocks"]) {
                PipelineConfig::BlockDef block;
                block.name = block_node["name"].as<std::string>();
                block.type = block_node["type"].as<std::string>();
                
                // Parse parameters
                if (block_node["parameters"]) {
                    for (const auto& param : block_node["parameters"]) {
                        std::string key = param.first.as<std::string>();
                        std::string value = param.second.as<std::string>();
                        block.parameters[key] = value;
                    }
                }
                
                config.blocks.push_back(block);
            }
        }
        
        // Parse connections
        if (root["connections"]) {
            for (const auto& conn_node : root["connections"]) {
                Connection conn;
                
                if (conn_node.IsSequence() && conn_node.size() >= 2) {
                    // Format: ["source_block.output", "sink_block.input"]
                    std::string source_str = conn_node[0].as<std::string>();
                    std::string sink_str = conn_node[1].as<std::string>();
                    
                    // Parse source
                    size_t dot_pos = source_str.find('.');
                    if (dot_pos != std::string::npos) {
                        conn.source_block = source_str.substr(0, dot_pos);
                        conn.source_output = source_str.substr(dot_pos + 1);
                    } else {
                        conn.source_block = source_str;
                    }
                    
                    // Parse sink
                    dot_pos = sink_str.find('.');
                    if (dot_pos != std::string::npos) {
                        conn.sink_block = sink_str.substr(0, dot_pos);
                        conn.sink_input = sink_str.substr(dot_pos + 1);
                    } else {
                        conn.sink_block = sink_str;
                    }
                } else {
                    // Object format
                    conn.source_block = conn_node["source"].as<std::string>();
                    conn.sink_block = conn_node["sink"].as<std::string>();
                    conn.source_output = conn_node["source_output"].as<std::string>("output");
                    conn.sink_input = conn_node["sink_input"].as<std::string>("input");
                }
                
                config.connections.push_back(conn);
            }
        }
        
        return true;
    } catch (const YAML::Exception& e) {
        last_error_ = "YAML parse error: " + std::string(e.what());
        VP_LOG_ERROR(last_error_);
        return false;
    }
}
#endif

// Simple INI-style parser
bool SimpleConfigParser::Parse(const std::string& content, PipelineConfig& config) {
    std::istringstream stream(content);
    std::string line;
    std::string current_section;
    
    config.name = "simple_pipeline";
    config.platform = "generic";
    
    while (std::getline(stream, line)) {
        // Remove comments
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.empty()) continue;
        
        // Check for section header
        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.length() - 2);
            continue;
        }
        
        // Parse key=value pairs
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            
            // Trim key and value
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            ParseSection(current_section, key + "=" + value, config);
        }
    }
    
    return true;
}

void SimpleConfigParser::ParseSection(const std::string& section, const std::string& content, PipelineConfig& config) {
    size_t eq_pos = content.find('=');
    if (eq_pos == std::string::npos) return;
    
    std::string key = content.substr(0, eq_pos);
    std::string value = content.substr(eq_pos + 1);
    
    if (section == "pipeline") {
        if (key == "name") config.name = value;
        else if (key == "platform") config.platform = value;
    }
    else if (section.find("block:") == 0) {
        // Block definition: [block:block_name]
        std::string block_name = section.substr(6);  // Remove "block:"
        
        // Find or create block
        PipelineConfig::BlockDef* block = nullptr;
        for (auto& b : config.blocks) {
            if (b.name == block_name) {
                block = &b;
                break;
            }
        }
        
        if (!block) {
            config.blocks.push_back(PipelineConfig::BlockDef{});
            block = &config.blocks.back();
            block->name = block_name;
        }
        
        if (key == "type") {
            block->type = value;
        } else {
            block->parameters[key] = value;
        }
    }
    else if (section == "connections") {
        // Parse connection: source_block -> sink_block
        std::regex conn_regex(R"(\s*(\w+)\s*->\s*(\w+)\s*)");
        std::smatch match;
        if (std::regex_match(value, match, conn_regex)) {
            Connection conn;
            conn.source_block = match[1].str();
            conn.sink_block = match[2].str();
            config.connections.push_back(conn);
        }
    }
}

} // namespace video_pipeline