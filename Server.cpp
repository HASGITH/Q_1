#include <enet/enet.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <random>
#include <chrono>
#include "NetworkProtocol.h"

struct Match {
    ENetPeer* p1;
    ENetPeer* p2;
    uint32_t seed;
};

struct Room {
    std::string code;
    ENetPeer* host;
    ENetPeer* guest;
    uint32_t seed;
};

std::vector<ENetPeer*> quickQueue;
std::map<std::string, Room> rooms;
std::vector<Match> activeMatches;

void SendPacket(ENetPeer* peer, const void* data, size_t size) {
    if (!peer) return;
    ENetPacket* packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
}

std::string GenerateRoomCode() {
    static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::string code = "";
    for (int i = 0; i < 4; i++) {
        code += alphabet[rand() % (sizeof(alphabet) - 1)];
    }
    return code;
}

ENetPeer* GetOpponent(ENetPeer* sender) {
    for (auto& match : activeMatches) {
        if (match.p1 == sender) return match.p2;
        if (match.p2 == sender) return match.p1;
    }
    return nullptr;
}

void HandleDisconnect(ENetPeer* peer) {
    std::cout << "[SERVER] Client disconnected." << std::endl;

    for (auto it = quickQueue.begin(); it != quickQueue.end(); ) {
        if (*it == peer) it = quickQueue.erase(it);
        else ++it;
    }

    for (auto it = activeMatches.begin(); it != activeMatches.end(); ) {
        if (it->p1 == peer || it->p2 == peer) {
            ENetPeer* opp = (it->p1 == peer) ? it->p2 : it->p1;
            if (opp) {
                PacketHeader dcPkt{ PacketType::S2C_OPPONENT_DC };
                SendPacket(opp, &dcPkt, sizeof(dcPkt));
            }
            it = activeMatches.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = rooms.begin(); it != rooms.end(); ) {
        if (it->second.host == peer || it->second.guest == peer) {
            if (it->second.host == peer && it->second.guest) {
                PacketHeader dcPkt{ PacketType::S2C_OPPONENT_DC };
                SendPacket(it->second.guest, &dcPkt, sizeof(dcPkt));
            }
            it = rooms.erase(it);
        } else {
            ++it;
        }
    }
}

int main(int argc, char** argv) {
    srand((unsigned int)time(nullptr));

    if (enet_initialize() != 0) {
        std::cerr << "[ERROR] Failed to initialize ENet!" << std::endl;
        return 1;
    }
    atexit(enet_deinitialize);

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 7777;

    ENetHost* server = enet_host_create(&address, 64, 2, 0, 0);
    if (!server) {
        std::cerr << "[ERROR] Failed to create ENet server on port 7777!" << std::endl;
        return 1;
    }

    std::cout << "=========================================" << std::endl;
    std::cout << "  ANIMAL TACTICS SERVER STARTED ON 7777  " << std::endl;
    std::cout << "=========================================" << std::endl;

    ENetEvent event;
    while (true) {
        while (enet_host_service(server, &event, 10) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    std::cout << "[SERVER] New connection established." << std::endl;
                    break;

                case ENET_EVENT_TYPE_RECEIVE: {
                    if (event.packet->dataLength < sizeof(PacketHeader)) {
                        enet_packet_destroy(event.packet);
                        break;
                    }

                    PacketHeader* header = (PacketHeader*)event.packet->data;

                    if (header->type == PacketType::C2S_QUICK_MATCH) {
                        std::cout << "[MATCHMAKING] Player joined queue." << std::endl;
                        quickQueue.push_back(event.peer);

                        if (quickQueue.size() >= 2) {
                            ENetPeer* p1 = quickQueue[0];
                            ENetPeer* p2 = quickQueue[1];
                            quickQueue.erase(quickQueue.begin(), quickQueue.begin() + 2);

                            uint32_t matchSeed = (uint32_t)rand();
                            activeMatches.push_back({ p1, p2, matchSeed });

                            PacketMatchStart startP1{ PacketType::S2C_MATCH_START, 1, matchSeed, "Opponent" };
                            PacketMatchStart startP2{ PacketType::S2C_MATCH_START, 2, matchSeed, "Opponent" };

                            SendPacket(p1, &startP1, sizeof(startP1));
                            SendPacket(p2, &startP2, sizeof(startP2));

                            std::cout << "[MATCHMAKING] Match created! Seed: " << matchSeed << std::endl;
                        }
                    }
                    else if (header->type == PacketType::C2S_CREATE_ROOM) {
                        std::string code = GenerateRoomCode();
                        rooms[code] = { code, event.peer, nullptr, (uint32_t)rand() };

                        PacketRoomCode pkt{ PacketType::S2C_ROOM_CREATED };
                        snprintf(pkt.code, sizeof(pkt.code), "%s", code.c_str());
                        SendPacket(event.peer, &pkt, sizeof(pkt));

                        std::cout << "[ROOM] Created room code: " << code << std::endl;
                    }
                    else if (header->type == PacketType::C2S_JOIN_ROOM) {
                        PacketRoomCode* joinPkt = (PacketRoomCode*)event.packet->data;
                        std::string code = joinPkt->code;

                        if (rooms.find(code) != rooms.end() && rooms[code].guest == nullptr) {
                            Room& room = rooms[code];
                            room.guest = event.peer;

                            activeMatches.push_back({ room.host, room.guest, room.seed });

                            PacketMatchStart startP1{ PacketType::S2C_MATCH_START, 1, room.seed, "Friend" };
                            PacketMatchStart startP2{ PacketType::S2C_MATCH_START, 2, room.seed, "Host" };

                            SendPacket(room.host, &startP1, sizeof(startP1));
                            SendPacket(room.guest, &startP2, sizeof(startP2));

                            std::cout << "[ROOM] Player joined room " << code << "! Match started." << std::endl;
                            rooms.erase(code);
                        } else {
                            PacketHeader err{ PacketType::S2C_ROOM_ERROR };
                            SendPacket(event.peer, &err, sizeof(err));
                        }
                    }
                    else if (header->type == PacketType::C2S_CANCEL) {
                        HandleDisconnect(event.peer);
                    }
                    else if (header->type == PacketType::C2S_GAME_ACTION) {
                        ENetPeer* opponent = GetOpponent(event.peer);
                        if (opponent) {
                            PacketActionData* actionData = (PacketActionData*)event.packet->data;
                            actionData->type = PacketType::S2C_GAME_ACTION;
                            SendPacket(opponent, actionData, sizeof(PacketActionData));
                        }
                    }

                    enet_packet_destroy(event.packet);
                    break;
                }

                case ENET_EVENT_TYPE_DISCONNECT:
                    HandleDisconnect(event.peer);
                    break;

                default:
                    break;
            }
        }
    }

    enet_host_destroy(server);
    return 0;
}