#include "video_pipeline/blocks/tcp_sink.h"
#include "video_pipeline/logger.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace video_pipeline {

TcpSink::TcpSink()
    : BaseVideoSink("TcpSink", "TcpSink") {}

TcpSink::~TcpSink() {
    CloseSocket();
}

bool TcpSink::SupportsFormat(PixelFormat /*format*/) const {
    // Accept any format; receiver must know how to interpret the raw stream.
    return true;
}

std::vector<PixelFormat> TcpSink::GetSupportedFormats() const {
    return {
        PixelFormat::RGB24, PixelFormat::BGR24, PixelFormat::RGBA32, PixelFormat::BGRA32,
        PixelFormat::YUV420P, PixelFormat::NV12,  PixelFormat::NV21,  PixelFormat::YUYV,
        PixelFormat::UYVY};
}

bool TcpSink::Initialize(const BlockParams& params) {
    if (!BaseVideoSink::Initialize(params)) {
        return false;
    }

    auto host = BaseBlock::GetParameter("host");
    if (!host.empty()) {
        host_ = host;
    }

    auto port_str = BaseBlock::GetParameter("port");
    if (!port_str.empty()) {
        try {
            int port_val = std::stoi(port_str);
            if (port_val > 0 && port_val <= 65535) {
                port_ = static_cast<uint16_t>(port_val);
            } else {
                VP_LOG_WARNING_F("TcpSink '{}' invalid port '{}', keeping default {}", GetName(), port_str, port_);
            }
        } catch (const std::exception&) {
            VP_LOG_WARNING_F("TcpSink '{}' failed to parse port '{}', keeping default {}", GetName(), port_str, port_);
        }
    }

    auto reconnect_str = BaseBlock::GetParameter("reconnect");
    if (!reconnect_str.empty()) {
        reconnect_ = (reconnect_str == "true" || reconnect_str == "1");
    }

    VP_LOG_INFO_F("TcpSink initialized: host={}, port={}, reconnect={}", host_, port_, reconnect_);
    return true;
}

bool TcpSink::Start() {
    if (!Connect()) {
        return false;
    }
    return BaseVideoSink::Start();
}

bool TcpSink::Stop() {
    bool ok = BaseVideoSink::Stop();
    CloseSocket();
    return ok;
}

bool TcpSink::Shutdown() {
    bool ok = BaseVideoSink::Shutdown();
    CloseSocket();
    return ok;
}

bool TcpSink::ProcessFrameImpl(VideoFramePtr frame) {
    if (!frame || !frame->IsValid()) {
        VP_LOG_WARNING_F("TcpSink '{}' received invalid frame", GetName());
        return false;
    }

    const uint8_t* data = static_cast<const uint8_t*>(frame->GetData());
    size_t size = frame->GetSize();

    if (socket_fd_ < 0) {
        if (!reconnect_ || !Connect()) {
            return false;
        }
    }

    if (!SendAll(data, size)) {
        if (reconnect_) {
            VP_LOG_WARNING_F("TcpSink '{}' reconnecting after send failure", GetName());
            CloseSocket();
            if (!Connect()) {
                return false;
            }
            return SendAll(data, size);
        }
        return false;
    }

    return true;
}

bool TcpSink::Connect() {
    CloseSocket();

    socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        SetError("TcpSink failed to create socket: " + std::string(std::strerror(errno)));
        return false;
    }

    int flag = 1;
    ::setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

#ifdef SO_NOSIGPIPE
    ::setsockopt(socket_fd_, SOL_SOCKET, SO_NOSIGPIPE, &flag, sizeof(flag));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0) {
        SetError("TcpSink invalid host: " + host_);
        CloseSocket();
        return false;
    }

    if (::connect(socket_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        SetError("TcpSink connect failed: " + std::string(std::strerror(errno)));
        CloseSocket();
        return false;
    }

    VP_LOG_INFO_F("TcpSink '{}' connected to {}:{}", GetName(), host_, port_);
    return true;
}

void TcpSink::CloseSocket() {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}

bool TcpSink::SendAll(const uint8_t* data, size_t size) {
    size_t total_sent = 0;
    while (total_sent < size) {
        ssize_t sent = ::send(socket_fd_, data + total_sent, size - total_sent,
#ifdef MSG_NOSIGNAL
                              MSG_NOSIGNAL
#else
                              0
#endif
        );

        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }

            VP_LOG_WARNING_F("TcpSink '{}' send failed: {}", GetName(), std::strerror(errno));
            return false;
        }

        if (sent == 0) {
            VP_LOG_WARNING_F("TcpSink '{}' connection closed by peer", GetName());
            return false;
        }

        total_sent += static_cast<size_t>(sent);
    }

    return true;
}

} // namespace video_pipeline
