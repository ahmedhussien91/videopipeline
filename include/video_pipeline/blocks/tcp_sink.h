#pragma once

#include "video_pipeline/video_sink.h"
#include <string>

namespace video_pipeline {

/**
 * @brief Streams raw frame bytes over a TCP socket.
 *
 * Useful for piping into tools like netcat/ffplay on another machine.
 * Expects the receiver to already know width/height/pixel_format.
 */
class TcpSink : public BaseVideoSink {
public:
    TcpSink();
    ~TcpSink() override;

    bool SupportsFormat(PixelFormat format) const override;
    std::vector<PixelFormat> GetSupportedFormats() const override;

    bool Initialize(const BlockParams& params) override;
    bool Start() override;
    bool Stop() override;
    bool Shutdown() override;

protected:
    bool ProcessFrameImpl(VideoFramePtr frame) override;

private:
    bool Connect();
    void CloseSocket();
    bool SendAll(const uint8_t* data, size_t size);

    std::string host_{"127.0.0.1"};
    uint16_t port_{5000};
    bool reconnect_{true};
    int socket_fd_{-1};
};

} // namespace video_pipeline
