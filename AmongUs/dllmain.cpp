// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <locale>
#include <codecvt>
#include <map>

class Player {

    bool dead;
    BYTE id;
    std::string name;

public:
    Player(BYTE id) {
        this->dead = false;
        this->name = "";
        this->id = id;
    }

    void setDead(bool dead) {
        this->dead = dead;
    }
    bool getDead() {
        return this->dead;
    }

    void setName(std::string name) {
        this->name = name;
    }
    std::string getName() {
        return this->name;
    }
    BYTE getId() {
        return this->id;
    }
};

std::vector<Player*> players = {};
bool gameStart = false;
bool meeting = false;


void saveData() {
    std::ofstream stream;
    std::string plrs = "";

    for (int i = 0; i < players.size(); i++) {
        plrs += (*players[i]).getName() + ":" + std::to_string((*players[i]).getDead()) + ",";
    }

    stream.open("C:\\Users\\rman9\\Desktop\\reversing\\reverseData\\info.txt");

    stream << plrs + "||" + std::to_string(gameStart) + "|" + std::to_string(meeting);

    stream.close();
}

void Hook(void* src, void* dst, unsigned int len)
{
    if (len < 5) return;

    DWORD curProtection;
    VirtualProtect(src, len, PAGE_EXECUTE_READWRITE, &curProtection);

    memset(src, 0x90, len);

    uintptr_t relativeAddress = (uintptr_t)dst - (uintptr_t)src - 5;

    *(BYTE*)src = 0xE9;

    *(uintptr_t*)((BYTE*)src + 1) = relativeAddress;
    VirtualProtect(src, len, curProtection, &curProtection);
}

BYTE* TrampHook(void* src, void* dst, unsigned int len)
{
    if (len < 5) return 0;

    //Create Gateway.
    BYTE* gateway = (BYTE*)VirtualAlloc(0, len, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    //Write stolen bytes.
    memcpy_s(gateway, len, src, len);

    //Get the gateway to destination address.
    uintptr_t gatewayRelativeAddr = (BYTE*)src - gateway - 5;
    *(gateway + len) = 0xE9;

    //Write address of gateway to jmp.
    *(uintptr_t*)((uintptr_t)gateway + len + 1) = gatewayRelativeAddr;

    //Perform HOOK.
    Hook(src, dst, len);
    return gateway;
}

uintptr_t moduleAssembly = (uintptr_t)GetModuleHandleW(L"GameAssembly.dll");

typedef void* (*tAddPlayer)(void* GameData, void* PlayerControl);
tAddPlayer oAddPlayer = (tAddPlayer)(moduleAssembly + 0xB01660);;

typedef wchar_t (*tGetChar)(void* Str, int Pos);
tGetChar oGetChar = (tGetChar)(moduleAssembly + 0x4E0670);

typedef int (*tGetLegnth)(void* Str);
tGetLegnth oGetLength = (tGetLegnth)(moduleAssembly + 0x19E470);

typedef void* (*tUpdateName)(void* GameData, void* playerId, void* name);
tUpdateName oUpdateName = (tUpdateName)(moduleAssembly + 0xB04370);

typedef void* (*tOnStartGame)(void* AmongUsClient);
tOnStartGame oOnStartGame = (tOnStartGame)(moduleAssembly + 0xB9D0A0);

typedef void* (*tOnEndGame)(void* AmongUsClient, void* reason, void* showAd);
tOnEndGame oOnEndGame = (tOnEndGame)(moduleAssembly + 0xB9B460);

typedef void* (*tDie)(void* PlayerControl, void* reason);
tDie oDie = (tDie)(moduleAssembly + 0x8878B0);

typedef void* (*tOpenMeetingRoom)(void* HudManager, void* PlayerControl);
tOpenMeetingRoom oOpenMeetingRoom = (tOpenMeetingRoom)(moduleAssembly + 0xA99990);

typedef void* (*tVotingComplete)(void* MeetingHud, void* states, void* exiled, void* tie);
tVotingComplete oVotingComplete = (tVotingComplete)(moduleAssembly + 0x8E02A0);



std::string getString(uintptr_t ptr) {
   int strLength = oGetLength((void*)ptr);
   std::string str = "";
    std::wstring_convert< std::codecvt_utf8<wchar_t>, wchar_t > converter;

    for (int i = 0; i < strLength; i++) {
        try {
            str += converter.to_bytes(oGetChar((void*)ptr, i));
        }
        catch (const char* msg) {
            //str = msg;
        }
    }
    
    return str;
}

void hUpdateName(void* GameData, void* playerId, void* name) {
    oUpdateName(GameData, playerId, name);

    BYTE id = (BYTE)playerId;

    for (int i = 0; i < players.size(); i++) {
        if ((*players[i]).getId() == id) {
            (*players[i]).setName(getString((uintptr_t)name));
        }
    }

    saveData();
}

void hAddPlayer(void* GameData, void* PlayerControl) {
    oAddPlayer(GameData, PlayerControl);
    try {
        BYTE id = *(BYTE*)((BYTE*)PlayerControl + 0x28);
        bool exists = true;


        for (int i = 0; i < players.size(); i++) {
            if ((*players[i]).getId() == id) {
                exists = false;
            }
        }

        if (exists) { 
            Player* player = new Player(*(BYTE*)((BYTE*)PlayerControl + 0x28));

            players.push_back(player);
        }
    }
    catch (const char* msg) {

    }
}

void hOnEndGame(void* AmongUsClient, void* reason, void* showAd) {
    oOnEndGame(AmongUsClient, reason, showAd);

    gameStart = false;

    for (int i = 0; i < players.size(); i++) {
        (*players[i]).setDead(false);
    }

    saveData();
}


void hOnStartGame(void* AmongUsClient) {
    oOnStartGame(AmongUsClient);

    gameStart = true;

    saveData();
}

void hDie(void* PlayerControl, void* reason) {
    oDie(PlayerControl, reason);
    BYTE id = *(BYTE*)((BYTE*)PlayerControl + 0x28);

    for (int i = 0; i < players.size(); i++) {
        if ((*players[i]).getId() == id) {
            (*players[i]).setDead(true);
        }
    }

    saveData();
}

void hOpenMeetingRoom(void* HudManager, void* PlayerControl) {
    oOpenMeetingRoom(HudManager, PlayerControl);

    meeting = true;

    saveData();
}

void hVotingComplete(void* MeetingHud, void* states,  void* exiled, void* tie) {
    oVotingComplete(MeetingHud, states, exiled, tie);

    meeting = false;

    saveData();
}

DWORD WINAPI myThread(HMODULE hModule)
{
    moduleAssembly = (uintptr_t)GetModuleHandleW(L"GameAssembly.dll");
    oAddPlayer = (tAddPlayer)TrampHook(oAddPlayer, hAddPlayer, 10);
    oUpdateName = (tUpdateName)TrampHook(oUpdateName, hUpdateName, 7);
    oOnStartGame = (tOnStartGame)TrampHook(oOnStartGame, hOnStartGame, 11);
    oOnEndGame = (tOnEndGame)TrampHook(oOnEndGame, hOnEndGame, 11);
    oDie = (tDie)TrampHook(oDie, hDie, 11);
    oOpenMeetingRoom = (tOpenMeetingRoom)TrampHook(oOpenMeetingRoom, hOpenMeetingRoom, 11);
    oVotingComplete = (tVotingComplete)TrampHook(oVotingComplete, hVotingComplete, 11);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CloseHandle(CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)myThread, hModule, NULL, nullptr));
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}