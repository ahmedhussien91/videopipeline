#pragma once

#include "video_pipeline/video_sink.h"
#include <fstream>
#include <memory>

namespace video_pipeline {

/**
 * @brief File output formats
 */
enum class FileFormat {
    RAW = 0,        // Raw frame data
    PPM,            // Portable Pixmap (for RGB formats)
    PGM,            // Portable Graymap (for Y plane)
    YUV             // Raw YUV data
};

/**
 * @brief Video sink that writes frames to files
 */
class FileSink : public BaseVideoSink {
public:
    FileSink();
    virtual ~FileSink();
    
    // IVideoSink implementation
    bool SupportsFormat(PixelFormat format) const override;
    std::vector<PixelFormat> GetSupportedFormats() const override;
    
    // IBlock implementation
    bool Initialize(const BlockParams& params) override;
    bool Shutdown() override;
    
    // File sink specific
    bool SetOutputPath(const std::string& path);
    std::string GetOutputPath() const { return output_path_; }
    
    bool SetFileFormat(FileFormat format);
    FileFormat GetFileFormat() const { return file_format_; }
    
    void SetSingleFile(bool single_file) { single_file_ = single_file; }
    bool IsSingleFile() const { return single_file_; }
    
    size_t GetFramesWritten() const { return frames_written_; }
    
protected:
    bool ProcessFrameImpl(VideoFramePtr frame) override;
    
private:
    bool WriteFrameRaw(VideoFramePtr frame);
    bool WriteFramePPM(VideoFramePtr frame);
    bool WriteFramePGM(VideoFramePtr frame);
    bool WriteFrameYUV(VideoFramePtr frame);
    
    std::string GenerateFilename(const std::string& base_path, size_t frame_number, const std::string& extension);
    bool OpenOutputFile(const std::string& filename);
    void CloseOutputFile();
    
    std::string output_path_;
    FileFormat file_format_{FileFormat::RAW};
    bool single_file_{false};
    size_t frames_written_{0};
    
    std::unique_ptr<std::ofstream> output_file_;
    std::string current_filename_;
};

} // namespace video_pipeline