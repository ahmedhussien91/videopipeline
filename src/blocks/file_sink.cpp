#include "video_pipeline/blocks/file_sink.h"
#include "video_pipeline/logger.h"
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace video_pipeline {

FileSink::FileSink()
    : BaseVideoSink("FileSink", "FileSink") {
    output_path_ = "output";
}

FileSink::~FileSink() {
    CloseOutputFile();
}

bool FileSink::SupportsFormat(PixelFormat format) const {
    // FileSink can handle any format for raw output
    return true;
}

std::vector<PixelFormat> FileSink::GetSupportedFormats() const {
    return {
        PixelFormat::RGB24,
        PixelFormat::BGR24,
        PixelFormat::RGBA32,
        PixelFormat::BGRA32,
        PixelFormat::YUV420P,
        PixelFormat::NV12,
        PixelFormat::NV21,
        PixelFormat::YUYV,
        PixelFormat::UYVY
    };
}

bool FileSink::Initialize(const BlockParams& params) {
    if (!BaseVideoSink::Initialize(params)) {
        return false;
    }
    
    // Parse file sink specific parameters
    auto path = BaseBlock::GetParameter("path");
    if (!path.empty()) {
        SetOutputPath(path);
    }
    
    auto format_str = BaseBlock::GetParameter("format");
    if (!format_str.empty()) {
        if (format_str == "raw") file_format_ = FileFormat::RAW;
        else if (format_str == "ppm") file_format_ = FileFormat::PPM;
        else if (format_str == "pgm") file_format_ = FileFormat::PGM;
        else if (format_str == "yuv") file_format_ = FileFormat::YUV;
        else {
            VP_LOG_WARNING_F("Unknown file format '{}', using raw", format_str);
        }
    }
    
    auto single_file_str = BaseBlock::GetParameter("single_file");
    if (!single_file_str.empty()) {
        single_file_ = (single_file_str == "true" || single_file_str == "1");
    }
    
    VP_LOG_INFO_F("FileSink initialized: path='{}', format={}, single_file={}",
                  output_path_, static_cast<int>(file_format_), single_file_);
    return true;
}

bool FileSink::Shutdown() {
    CloseOutputFile();
    return BaseVideoSink::Shutdown();
}

bool FileSink::SetOutputPath(const std::string& path) {
    if (BaseBlock::GetState() == BlockState::RUNNING) {
        SetError("Cannot change output path while running");
        return false;
    }
    
    if (path.empty()) {
        VP_LOG_WARNING("Empty output path specified; using default 'output'");
        output_path_ = "output";
    } else {
        output_path_ = path;
    }
    
    // Create directory if it doesn't exist (for multi-file mode)
    if (!single_file_) {
        try {
            auto parent = std::filesystem::path(output_path_).parent_path();
            if (!parent.empty()) {
                std::filesystem::create_directories(parent);
            }
        } catch (const std::exception& e) {
            VP_LOG_WARNING_F("Failed to create directory: {}", e.what());
        }
    }
    
    return true;
}

bool FileSink::SetFileFormat(FileFormat format) {
    if (BaseBlock::GetState() == BlockState::RUNNING) {
        SetError("Cannot change file format while running");
        return false;
    }
    
    file_format_ = format;
    return true;
}

bool FileSink::ProcessFrameImpl(VideoFramePtr frame) {
    if (!frame || !frame->IsValid()) {
        VP_LOG_ERROR("Invalid frame received");
        return false;
    }
    
    bool success = false;
    
    switch (file_format_) {
        case FileFormat::RAW:
            success = WriteFrameRaw(frame);
            break;
        case FileFormat::PPM:
            success = WriteFramePPM(frame);
            break;
        case FileFormat::PGM:
            success = WriteFramePGM(frame);
            break;
        case FileFormat::YUV:
            success = WriteFrameYUV(frame);
            break;
        default:
            VP_LOG_ERROR_F("Unsupported file format: {}", static_cast<int>(file_format_));
            return false;
    }
    
    if (success) {
        frames_written_++;
        
        if (frames_written_ % 100 == 0) {
            VP_LOG_INFO_F("FileSink '{}' wrote {} frames", BaseBlock::GetName(), frames_written_);
        }
    }
    
    return success;
}

bool FileSink::WriteFrameRaw(VideoFramePtr frame) {
    std::string filename;
    
    if (single_file_) {
        filename = output_path_;
        if (frames_written_ == 0) {
            if (!OpenOutputFile(filename)) {
                return false;
            }
        }
    } else {
        filename = GenerateFilename(output_path_, frames_written_, "raw");
        if (!OpenOutputFile(filename)) {
            return false;
        }
    }
    
    // Write frame data
    const void* data = frame->GetData();
    size_t size = frame->GetSize();
    
    output_file_->write(static_cast<const char*>(data), size);
    bool ok = output_file_->good();
    
    if (!single_file_) {
        CloseOutputFile();
    }
    
    return ok;
}

bool FileSink::WriteFramePPM(VideoFramePtr frame) {
    const auto& info = frame->GetFrameInfo();
    
    // PPM only supports RGB formats
    if (info.pixel_format != PixelFormat::RGB24 && 
        info.pixel_format != PixelFormat::RGBA32) {
        VP_LOG_ERROR("PPM format only supports RGB24 and RGBA32");
        return false;
    }
    
    std::string filename = GenerateFilename(output_path_, frames_written_, "ppm");
    if (!OpenOutputFile(filename)) {
        return false;
    }
    
    // Write PPM header
    *output_file_ << "P6\n";
    *output_file_ << info.width << " " << info.height << "\n";
    *output_file_ << "255\n";
    
    // Write pixel data
    const uint8_t* data = static_cast<const uint8_t*>(frame->GetData());
    
    if (info.pixel_format == PixelFormat::RGB24) {
        output_file_->write(reinterpret_cast<const char*>(data), info.width * info.height * 3);
    } else if (info.pixel_format == PixelFormat::RGBA32) {
        // Convert RGBA to RGB
        for (uint32_t i = 0; i < info.width * info.height; ++i) {
            output_file_->write(reinterpret_cast<const char*>(&data[i * 4]), 3);
        }
    }
    
    bool ok = output_file_->good();
    CloseOutputFile();
    return ok;
}

bool FileSink::WriteFramePGM(VideoFramePtr frame) {
    const auto& info = frame->GetFrameInfo();
    
    std::string filename = GenerateFilename(output_path_, frames_written_, "pgm");
    if (!OpenOutputFile(filename)) {
        return false;
    }
    
    // Write PGM header
    *output_file_ << "P5\n";
    *output_file_ << info.width << " " << info.height << "\n";
    *output_file_ << "255\n";
    
    const uint8_t* data = static_cast<const uint8_t*>(frame->GetData());
    
    // Convert to grayscale if needed
    if (info.pixel_format == PixelFormat::RGB24) {
        for (uint32_t i = 0; i < info.width * info.height; ++i) {
            uint8_t gray = static_cast<uint8_t>(
                0.299 * data[i * 3 + 0] + 
                0.587 * data[i * 3 + 1] + 
                0.114 * data[i * 3 + 2]);
            output_file_->write(reinterpret_cast<const char*>(&gray), 1);
        }
    } else {
        // Assume grayscale or use Y plane for YUV
        size_t gray_size = info.width * info.height;
        output_file_->write(reinterpret_cast<const char*>(data), gray_size);
    }
    
    bool ok = output_file_->good();
    CloseOutputFile();
    return ok;
}

bool FileSink::WriteFrameYUV(VideoFramePtr frame) {
    std::string filename;
    
    if (single_file_) {
        filename = output_path_ + ".yuv";
        if (frames_written_ == 0) {
            if (!OpenOutputFile(filename)) {
                return false;
            }
        }
    } else {
        filename = GenerateFilename(output_path_, frames_written_, "yuv");
        if (!OpenOutputFile(filename)) {
            return false;
        }
    }
    
    // Write YUV data
    const void* data = frame->GetData();
    size_t size = frame->GetSize();
    
    output_file_->write(static_cast<const char*>(data), size);
    bool ok = output_file_->good();
    
    if (!single_file_) {
        CloseOutputFile();
    }
    
    return ok;
}

std::string FileSink::GenerateFilename(const std::string& base_path, size_t frame_number, const std::string& extension) {
    std::ostringstream oss;
    oss << base_path << "_" << std::setfill('0') << std::setw(6) << frame_number << "." << extension;
    return oss.str();
}

bool FileSink::OpenOutputFile(const std::string& filename) {
    if (current_filename_ == filename && output_file_ && output_file_->is_open()) {
        return true;  // File already open
    }
    
    CloseOutputFile();
    
    try {
        output_file_ = std::make_unique<std::ofstream>(filename, std::ios::binary);
        if (!output_file_->is_open()) {
            SetError("Failed to open output file: " + filename);
            return false;
        }
        current_filename_ = filename;
        VP_LOG_DEBUG_F("Opened output file: {}", filename);
        return true;
    } catch (const std::exception& e) {
        SetError("Exception opening file '" + filename + "': " + e.what());
        return false;
    }
}

void FileSink::CloseOutputFile() {
    if (output_file_ && output_file_->is_open()) {
        output_file_->close();
        VP_LOG_DEBUG_F("Closed output file: {}", current_filename_);
    }
    output_file_.reset();
    current_filename_.clear();
}

} // namespace video_pipeline
