#pragma once
#include <cstdint>

#pragma pack(push, 1)

enum class PacketType : uint8_t {
    // Клиент -> Сервер
    C2S_QUICK_MATCH = 1,
    C2S_CREATE_ROOM = 2,
    C2S_JOIN_ROOM   = 3,
    C2S_CANCEL      = 4,
    C2S_GAME_ACTION = 5,

    // Сервер -> Клиент
    S2C_ROOM_CREATED = 10,
    S2C_ROOM_ERROR   = 11,
    S2C_MATCH_START  = 12,
    S2C_GAME_ACTION  = 13,
    S2C_OPPONENT_DC  = 14
};

enum class NetActionType : uint8_t {
    PLACE_CARD = 1,
    MOVE       = 2,
    ATTACK     = 3,
    ABILITY    = 4,
    END_TURN   = 5,
    SURRENDER  = 6
};

struct NetGameAction {
    NetActionType type;
    int8_t handIndex;
    int8_t fromX, fromY;
    int8_t toX, toY;
};

struct PacketHeader {
    PacketType type;
};

struct PacketRoomCode {
    PacketType type;
    char code[5];
};

struct PacketMatchStart {
    PacketType type;
    uint8_t playerRole; // 1 = Player 1, 2 = Player 2
    uint32_t seed;
    char opponentName[32];
};

struct PacketActionData {
    PacketType type;
    NetGameAction action;
};

#pragma pack(pop)