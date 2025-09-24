#pragma once
extern "C"
{
#include <enet/enet.h>
}
#include <vector>
#include <string>
#include <functional>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <cstring>
#include <iostream>

namespace SimpleNet 
{

    enum class PacketReliability 
    {
        Reliable,
        Unreliable
    };

    enum class NetRole 
    {
        Server,
        Client
    };

    class Packet 
    {
    public:
        std::vector<uint8_t> data;

        Packet() = default;
        Packet(const std::vector<uint8_t>& d) : data(d) {}
        Packet(std::vector<uint8_t>&& d) : data(std::move(d)) {}

        //Appending raw bytes
        void appendBytes(const void* src, size_t len) 
        {
            const uint8_t* b = reinterpret_cast<const uint8_t*>(src);
            data.insert(data.end(), b, b + len);
        }

        //Appending little-endian POD
        template<typename T>
        void appendPOD(const T& v) 
        {
            appendBytes(&v, sizeof(T));
        }

        //Appending string (uint32 length + bytes)
        void appendString(const std::string& s) {
            uint32_t len = static_cast<uint32_t>(s.size());
            appendPOD(len);
            if (len) appendBytes(s.data(), len);
        }

        //Reading helpers
        size_t cursor = 0;

        bool readBytes(void* dst, size_t len) 
        {
            if (cursor + len > data.size())
            {
                return false;
            }

            std::memcpy(dst, data.data() + cursor, len);
            cursor += len;

            return true;
        }

        template<typename T>
        bool readPOD(T& out) 
        {
            return readBytes(&out, sizeof(T));
        }

        bool readString(std::string& out) 
        {
            uint32_t len = 0;
            if (!readPOD(len))
            {
                return false;
            }

            if (cursor + len > data.size())
            {
                return false;
            }

            out.assign(reinterpret_cast<const char*>(data.data() + cursor), len);
            cursor += len;

            return true;
        }
    };

    struct NetEvent 
    {
        enum Type { Connect, Disconnect, Receive } type;
        uint32_t peerId;  //For client role
        Packet packet;    //Only for Receive
    };

    class NetServer;
    class NetClient;

    class Net 
    {
    public:
        static bool Initialize() 
        {
            if (enet_initialize() != 0) 
            {
                std::cerr << "ENet initialization failed\n";
                return false;
            }
            return true;
        }
        static void Deinitialize() 
        {
            enet_deinitialize();
        }
    };

    class NetServer 
    {
    public:
        using EventCallback = std::function<void(const NetEvent&)>;

        NetServer() : host(nullptr), nextPeerId(1) {}
        ~NetServer() 
        {
            if (host) 
            {
                enet_host_destroy(host);
                host = nullptr;
            }
        }

        bool create(uint16_t port, uint32_t maxClients = 32) 
        {
            ENetAddress address;
            address.host = ENET_HOST_ANY;
            address.port = port;
            host = enet_host_create(&address, maxClients, 2, 0, 0);

            if (!host) 
            {
                std::cerr << "Failed to create ENet server host on port " << port << "\n";
                return false;
            }
            return true;
        }

        void service(uint32_t timeoutMs, const EventCallback& cb) 
        {
            if (!host)
            {
                return;
            }

            ENetEvent event;
            while (enet_host_service(host, &event, timeoutMs) > 0) 
            {
                switch (event.type) 
                {
                case ENET_EVENT_TYPE_CONNECT: 
                {
                    uint32_t id = nextPeerId++;
                    peerMap[event.peer] = id;
                    idMap[id] = event.peer;
                    event.peer->data = reinterpret_cast<void*>(static_cast<uintptr_t>(id));
                    NetEvent e;
                    e.type = NetEvent::Connect;
                    e.peerId = id;
                    cb(e);
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT: 
                {
                    auto it = peerMap.find(event.peer);
                    uint32_t id = 0;

                    if (it != peerMap.end()) 
                    {
                        id = it->second;
                        peerMap.erase(it);
                        idMap.erase(id);
                    }

                    NetEvent e;
                    e.type = NetEvent::Disconnect;
                    e.peerId = id;
                    cb(e);
                    break;
                }
                case ENET_EVENT_TYPE_RECEIVE: {
                    auto it = peerMap.find(event.peer);
                    uint32_t id = (it != peerMap.end()) ? it->second : 0;
                    NetEvent e;
                    e.type = NetEvent::Receive;
                    e.peerId = id;
                    e.packet.data.resize(event.packet->dataLength);
                    std::memcpy(e.packet.data.data(), event.packet->data, event.packet->dataLength);
                    cb(e);
                    enet_packet_destroy(event.packet);
                    break;
                }
                default: break;
                }
            }
        }

        bool sendTo(uint32_t peerId, const Packet& p, PacketReliability r = PacketReliability::Reliable, int channel = 0) 
        {
            auto it = idMap.find(peerId);
            if (it == idMap.end())
            {
                return false;
            }

            ENetPeer* peer = it->second;
            enet_uint32 flags = (r == PacketReliability::Reliable) ? ENET_PACKET_FLAG_RELIABLE : 0;
            ENetPacket* packet = enet_packet_create(p.data.data(), p.data.size(), flags);

            if (!packet)
            {
                return false;
            }

            enet_peer_send(peer, channel, packet);
            enet_host_flush(host);

            return true;
        }

        void broadcast(const Packet& p, PacketReliability r = PacketReliability::Reliable, int channel = 0) 
        {
            if (!host)
            {
                return;
            }

            enet_uint32 flags = (r == PacketReliability::Reliable) ? ENET_PACKET_FLAG_RELIABLE : 0;
            ENetPacket* packet = enet_packet_create(p.data.data(), p.data.size(), flags);

            enet_host_broadcast(host, channel, packet);
            enet_host_flush(host);
        }

        void disconnect(uint32_t peerId, uint32_t data = 0) 
        {
            auto it = idMap.find(peerId);
            if (it == idMap.end())
            {
                return;
            }

            enet_peer_disconnect(it->second, data);
        }

        size_t connectedCount() const { return peerMap.size(); }

    private:

        ENetHost* host;
        uint32_t nextPeerId;

        std::unordered_map<ENetPeer*, uint32_t> peerMap;
        std::unordered_map<uint32_t, ENetPeer*> idMap;
    };

    class NetClient 
    {
    public:
        using EventCallback = std::function<void(const NetEvent&)>;

        NetClient() : clientHost(nullptr), serverPeer(nullptr) {}
        ~NetClient() 
        {
            if (clientHost) 
            {
                enet_host_destroy(clientHost);
                clientHost = nullptr;
            }
        }

        bool connect(const std::string& host, uint16_t port, uint32_t timeoutMs = 5000) 
        {
            clientHost = enet_host_create(NULL, 1, 2, 0, 0);
            if (!clientHost) 
            {
                std::cerr << "Failed to create ENet client host\n";
                return false;
            }

            ENetAddress address;
            enet_address_set_host(&address, host.c_str());
            address.port = port;

            serverPeer = enet_host_connect(clientHost, &address, 2, 0);
            if (!serverPeer) 
            {
                std::cerr << "No available peers for initiating connection\n";
                return false;
            }

            ENetEvent event;
            if (enet_host_service(clientHost, &event, timeoutMs) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) 
            {
                return true;
            }

            enet_peer_reset(serverPeer);
            serverPeer = nullptr;
            return false;
        }

        void service(uint32_t timeoutMs, const EventCallback& cb) 
        {
            if (!clientHost)
            {
                return;
            }

            ENetEvent event;
            while (enet_host_service(clientHost, &event, timeoutMs) > 0) 
            {
                switch (event.type)
                {
                case ENET_EVENT_TYPE_CONNECT:
                {
                    NetEvent e;
                    e.type = NetEvent::Connect;
                    e.peerId = 0;
                    cb(e);

                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    NetEvent e;
                    e.type = NetEvent::Disconnect;
                    e.peerId = 0;
                    cb(e);

                    break;
                }
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    NetEvent e;
                    e.type = NetEvent::Receive;
                    e.peerId = 0;
                    e.packet.data.resize(event.packet->dataLength);

                    std::memcpy(e.packet.data.data(), event.packet->data, event.packet->dataLength);
                    cb(e);
                    enet_packet_destroy(event.packet);

                    break;
                }
                default:
                {
                    break;
                }
                }
            }
        }

        bool send(const Packet& p, PacketReliability r = PacketReliability::Reliable, int channel = 0) 
        {
            if (!serverPeer)
            {
                return false;
            }

            enet_uint32 flags = (r == PacketReliability::Reliable) ? ENET_PACKET_FLAG_RELIABLE : 0;
            ENetPacket* packet = enet_packet_create(p.data.data(), p.data.size(), flags);

            enet_peer_send(serverPeer, channel, packet);
            enet_host_flush(clientHost);

            return true;
        }

        void disconnect(uint32_t data = 0) 
        {
            if (serverPeer)
            {
                enet_peer_disconnect(serverPeer, data);
            }
        }

    private:

        ENetHost* clientHost;
        ENetPeer* serverPeer;
    };
}
