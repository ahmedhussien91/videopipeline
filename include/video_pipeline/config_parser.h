#pragma once

#include "pipeline_manager.h"
#include <string>
#include <memory>

namespace video_pipeline {

/**
 * @brief Configuration parser interface
 */
class IConfigParser {
public:
    virtual ~IConfigParser() = default;
    virtual bool Parse(const std::string& content, PipelineConfig& config) = 0;
    virtual std::string GetLastError() const = 0;
};

using ConfigParserPtr = std::unique_ptr<IConfigParser>;

/**
 * @brief Configuration parser factory
 */
class ConfigParserFactory {
public:
    static ConfigParserPtr CreateParser(const std::string& format);
    static std::vector<std::string> GetSupportedFormats();
};

/**
 * @brief JSON configuration parser
 */
class JsonConfigParser : public IConfigParser {
public:
    bool Parse(const std::string& content, PipelineConfig& config) override;
    std::string GetLastError() const override { return last_error_; }
    
private:
    std::string last_error_;
};

#ifdef HAVE_YAML_CPP
/**
 * @brief YAML configuration parser
 */
class YamlConfigParser : public IConfigParser {
public:
    bool Parse(const std::string& content, PipelineConfig& config) override;
    std::string GetLastError() const override { return last_error_; }
    
private:
    std::string last_error_;
};
#endif

/**
 * @brief Simple INI-style configuration parser
 */
class SimpleConfigParser : public IConfigParser {
public:
    bool Parse(const std::string& content, PipelineConfig& config) override;
    std::string GetLastError() const override { return last_error_; }
    
private:
    std::string last_error_;
    void ParseSection(const std::string& section, const std::string& content, PipelineConfig& config);
};

} // namespace video_pipeline