#pragma once
#include <Windows.h>
#include <thread>
#include <chrono>
enum class ControlCodes : DWORD64
{
    None = 0,
    ReadBuffer = 0x80002020,
    UnloadDriver = 0x8000200C,
};

struct DriverRequest
{
    char Pattern[49];
    ControlCodes ControlCode;
    size_t size;
    DWORD64 adress;
    SIZE_T size_out;
    volatile LONG result;
    volatile LONG write;
    char Data[4000];
};

static_assert(sizeof(DriverRequest) == 4096, "DriverRequest size is not 4096 bytes"); // Ensure the structure is 4096 bytes in size (One page)

class Driver
{
public:
    inline static DriverRequest* DriverMemory;
    static bool Init()
    {
        DriverMemory = (DriverRequest*)VirtualAlloc(NULL, sizeof(DriverRequest),
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

        if (!DriverMemory)
        {
            printf("Error allocating memory, last error: %d\n", GetLastError());
            return false;
        }

        memset(DriverMemory, 0, sizeof(DriverRequest));

        memcpy(DriverMemory->Pattern, "kR7mZ2pLqW9xBn4vTjY1sHdF8cGaE3uNiO6wXrK5tMbJ0yDq", 49);

        VirtualLock(DriverMemory, sizeof(DriverRequest));

        printf("Waiting for driver handshake...\n");

        auto start = std::chrono::steady_clock::now();
        while (ReadNoFence(&DriverMemory->write) != 1)
        {
            _mm_pause();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (std::chrono::steady_clock::now() - start >= std::chrono::seconds(120))
            {
                printf("Driver handshake timeout\n");
                VirtualUnlock(DriverMemory, sizeof(DriverRequest));
                VirtualFree(DriverMemory, 0, MEM_RELEASE);
                DriverMemory = nullptr;
                return false;
            }
        }

        InterlockedExchange(&DriverMemory->write, 0);
        ZeroMemory(DriverMemory, sizeof(DriverRequest));
        printf("Driver initialized\n");
        return true;
    }
    template <typename T>
    static T sendRequest(ControlCodes controlCode, size_t size, DWORD64 address)
    {
        if (!DriverMemory) return T();

        DriverMemory->ControlCode = controlCode;
        DriverMemory->size = size;
        DriverMemory->adress = address;

        memcpy(DriverMemory->Data, reinterpret_cast<void*>(address), size);

        InterlockedExchange(&DriverMemory->write, 1);
        waitAnswer();

        return T(0);
    }

    static void unload()
    {
        if (!DriverMemory) return;

        DriverMemory->ControlCode = ControlCodes::UnloadDriver;
        InterlockedExchange(&DriverMemory->write, 1);

        while (ReadNoFence(&DriverMemory->write) != 0)
            _mm_pause();
        _mm_lfence();

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        printf("Unloading\n");

        VirtualUnlock(DriverMemory, sizeof(DriverRequest));
        VirtualFree(DriverMemory, 0, MEM_RELEASE);
        DriverMemory = nullptr;
    }

    static void waitAnswer()
    {
        while (ReadNoFence(&DriverMemory->write) != 0)
            _mm_pause();
        _mm_lfence();
    }

};
