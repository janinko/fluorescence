
#include "socket.hpp"

#include <errno.h>
#include <string.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#endif

#include <misc/logger.hpp>

#include "manager.hpp"
#include "packetwriter.hpp"
#include "packetreader.hpp"

namespace uome {
namespace net {

Socket::Socket() : socketFd_(0), decompressedSize_(0), sendSize_(0), useDecompress_(false), running_(false) {
}

Socket::~Socket() {
    close();
    receiveThread_.join();
}

void Socket::setEncryption(boost::shared_ptr<Encryption> value) {
    encryption_ = value;
}

bool Socket::connect(const std::string& host, unsigned short port) {
    if (socketFd_) {
        LOG_ERROR(LOGTYPE_NETWORK, "Trying to reconnect an already connected socket!");
        return false;
    }

    socketFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd_ == -1) {
        #ifdef WIN32
            LOGARG_ERROR(LOGTYPE_NETWORK, "Unable to open socket: %s", strerror(WSAGetLastError()));
        #else
            LOGARG_ERROR(LOGTYPE_NETWORK, "Unable to open socket: %s", strerror(errno));
        #endif

        return false;
    }

    hostent* hostent = gethostbyname(host.c_str());

    if (!hostent) {
        LOGARG_ERROR(LOGTYPE_NETWORK, "Unknown host: %s", host.c_str());
        close();
        return false;
    }

    sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    memcpy(&(dest_addr.sin_addr), hostent->h_addr_list[0], 4);

    if (::connect(socketFd_, (sockaddr*)&dest_addr, sizeof(sockaddr)) == -1) {
        #ifdef WIN32
            LOGARG_ERROR(LOGTYPE_NETWORK, "Unable to connect socket: %s", strerror(WSAGetLastError()));
        #else
            LOGARG_ERROR(LOGTYPE_NETWORK, "Unable to connect socket: %s", strerror(errno));
        #endif
        close();
        return false;
    }

    // set socket nonblocking
    #ifdef WIN32
        u_long iMode = 1;
        if (ioctlsocket(m_sock, FIONBIO, &iMode) != 0) {
            LOGARG_ERROR(LOGTYPE_NETWORK, "Unable to set socket to nonblocking: %s", strerror(WSAGetLastError()));
            close();
            return false;
        }
    #else
        int flags;

        if ((flags = fcntl(socketFd_, F_GETFL, 0)) < 0 ||
                fcntl(socketFd_, F_SETFL, flags | O_NONBLOCK) < 0) {
            LOGARG_ERROR(LOGTYPE_NETWORK, "Unable to set socket to nonblocking: %s", strerror(errno));
            close();
            return false;
        }
    #endif

    LOGARG_INFO(LOGTYPE_NETWORK, "Socket connected to %s:%u", host.c_str(), port);

    sendSize_ = 0;
    running_ = true;
    criticalError_ = false;

    receiveThread_ = boost::thread(&Socket::receiveRun, this);

    return true;
}

void Socket::close() {
    running_ = false;

    if (socketFd_) {
        LOG_INFO(LOGTYPE_NETWORK, "Closing socket");
        ::close(socketFd_);
        socketFd_ = 0;
    }
}

void Socket::receiveRun() {
    while (running_ && !criticalError_) {
        socketMutex_.lock();
        int recvLen = recv(socketFd_, rawBuffer_, 0x4000, 0);
        socketMutex_.unlock();

        if (recvLen > 0) {
            // decrypt data
            if (encryption_) {
                LOG_INFO(LOGTYPE_NETWORK, "decrypt");
                encryption_->decrypt(rawBuffer_, rawBuffer_, recvLen);
            }

            unsigned int decompLen = recvLen;
            if (useDecompress_) {
                // decompress data
                LOG_INFO(LOGTYPE_NETWORK, "decompress");
                decompLen = decompress_.huffmanDecompress(decompressedBuffer_ + decompressedSize_, 0x10000 - decompressedSize_, rawBuffer_, recvLen);
            } else {
                memcpy(decompressedBuffer_ + decompressedSize_, rawBuffer_, recvLen);
            }

            decompressedSize_ += decompLen;

            LOG_INFO(LOGTYPE_NETWORK, "buffer dump:");
            dumpBuffer(decompressedBuffer_, decompLen);

            parsePackets();

            // sleep a little
            usleep(10);

        } else if (recvLen == 0) { // peer has shut down
            LOG_INFO(LOGTYPE_NETWORK, "Peer has shut down socket");
            // set error indicator ?
            break;
        } else if (recvLen == -1 && running_) { // error
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                #ifdef WIN32
                    LOGARG_ERROR(LOGTYPE_NETWORK, "Socket error in receiveRun(): %s", strerror(WSAGetLastError()));
                #else
                    LOGARG_ERROR(LOGTYPE_NETWORK, "Socket error in receiveRun(): %s", strerror(errno));
                #endif

                criticalError_ = true;
                break;
            }
        }
    }
}

void Socket::dumpBuffer(int8_t* buf, unsigned int length) {
    for (unsigned int i = 0; i < length; ++i) {
        if (i % 8 == 0) {
            printf("\n%4u: ", i);
        }
        printf("%x(%c) ", (uint8_t)buf[i], buf[i]);
    }
    printf("\n");
}

bool Socket::sendAll() {
    if (sendSize_ == 0 || criticalError_) {
        return true;
    }

    LOG_INFO(LOGTYPE_NETWORK, "sendAll pre send:");
    dumpBuffer(sendBuffer_, sendSize_);

    socketMutex_.lock();
    unsigned int sendLen = ::send(socketFd_, sendBuffer_, sendSize_, 0);
    socketMutex_.unlock();

    LOG_INFO(LOGTYPE_NETWORK, "sendAll post send:");

    if (sendLen < 0) {
        #ifdef WIN32
            LOGARG_ERROR(LOGTYPE_NETWORK, "Socket error in sendAll(): %s", strerror(WSAGetLastError()));
        #else
            LOGARG_ERROR(LOGTYPE_NETWORK, "Socket error in sendAll(): %s", strerror(errno));
        #endif

        criticalError_ = true;

        return false;
    }

    if (sendLen != sendSize_) {
        LOG_ERROR(LOGTYPE_NETWORK, "Socket error in sendAll(): Unable to send all bytes");
        return false;
    }

    sendSize_ = 0;

    return true;
}

void Socket::writeSeed(uint32_t seed) {
    PacketWriter::write(sendBuffer_, 0x10000, sendSize_, seed);
}

void Socket::setUseDecompress(bool value) {
    useDecompress_ = value;
}

void Socket::parsePackets() {
    unsigned int idx = 0;
    unsigned int lastPacketStart = 0;
    while ((lastPacketStart = idx) < decompressedSize_) {

        bool readSuccess = true;
        uint8_t packetId;
        readSuccess = PacketReader::read(decompressedBuffer_, decompressedSize_, idx, packetId);

        LOGARG_DEBUG(LOGTYPE_NETWORK, "Parsing packet id=%u", packetId);

        boost::shared_ptr<Packet> newPacket = Manager::createPacket(packetId);

        if (!newPacket) {
            LOGARG_ERROR(LOGTYPE_NETWORK, "Unable to create packet for id %u", packetId);
            continue;
        }

        uint16_t packetSize;
        if (newPacket->hasVariableSize()) {
            readSuccess = readSuccess && PacketReader::read(decompressedBuffer_, decompressedSize_, idx, packetSize);
        } else {
            packetSize = newPacket->getSize();
        }

        LOGARG_DEBUG(LOGTYPE_NETWORK, "Parsing packet size=%u", packetSize);

        readSuccess = readSuccess && newPacket->read(decompressedBuffer_, decompressedSize_, idx);

        LOGARG_DEBUG(LOGTYPE_NETWORK, "Parsing packet idx=%u", idx);

        if (readSuccess) {
            if (idx - lastPacketStart < packetSize) {
                LOGARG_DEBUG(LOGTYPE_NETWORK, "Not enough bytes read for packet id=%u len=%u read=%u", packetId, packetSize, idx - lastPacketStart);
            } else {
                LOGARG_DEBUG(LOGTYPE_NETWORK, "Read of packet id=%u len=%u successful!", packetId, packetSize);

                packetQueueMutex_.lock();
                packetQueue_.push(newPacket);
                packetQueueMutex_.unlock();
            }
        } else {
            // not enouth bytes received yet
            break;
        }

        // set idx to start of next packet
        idx = lastPacketStart + packetSize;
    }

    if (lastPacketStart != decompressedSize_) { // some bytes left
        if (lastPacketStart > 0) {
            // move unread bytes to start of buffer
            memmove(decompressedBuffer_, decompressedBuffer_ + lastPacketStart, decompressedSize_ - lastPacketStart);
            decompressedSize_ -= lastPacketStart;
        }
    } else {
        decompressedSize_ = 0;
    }
}

bool Socket::isConnected() {
    return socketFd_ != -1;
}

boost::shared_ptr<Packet> Socket::getNextPacket() {
    boost::shared_ptr<Packet> ret;

    packetQueueMutex_.lock();
    if (!packetQueue_.empty()) {
        ret = packetQueue_.front();
        packetQueue_.pop();
    }
    packetQueueMutex_.unlock();

    return ret;
}

}
}