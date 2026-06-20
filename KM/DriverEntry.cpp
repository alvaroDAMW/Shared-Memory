#include <wdm.h>
#include <stdarg.h>
#include <ntstrsafe.h>

enum class ControlCodes : DWORD64
{
	None = 0,
	ReadBuffer = 0x80002020,
	UnloadDriver = 0x8000200C,
};
struct DriverRequest
{
	char Pattern[49] = "kR7mZ2pLqW9xBn4vTjY1sHdF8cGaE3uNiO6wXrK5tMbJ0yDq";
	ControlCodes ControlCode;
	size_t size;
	DWORD64 adress;
	SIZE_T size_out;
	volatile LONG result;
	volatile LONG write;
	char Data[4000];
};
typedef struct _MM_COPY_ADDRESS {
    union {
        PVOID            VirtualAddress;
        PHYSICAL_ADDRESS PhysicalAddress;
    };
} MM_COPY_ADDRESS, * PMMCOPY_ADDRESS;

EXTERN_C NTSTATUS MmCopyMemory(
    PVOID           TargetAddress,
    MM_COPY_ADDRESS SourceAddress,
    SIZE_T          NumberOfBytes,
    ULONG           Flags,
    PSIZE_T         NumberOfBytesTransferred
);

extern "C" NTSTATUS NTAPI MmCopyVirtualMemory(
    PEPROCESS SourceProcess,
    PVOID SourceAddress,
    PEPROCESS TargetProcess,
    PVOID TargetAddress,
    SIZE_T BufferSize,
    KPROCESSOR_MODE PreviousMode,
    PSIZE_T ReturnSize
);
typedef struct _PHYSICAL_MEMORY_RANGE {

    PHYSICAL_ADDRESS BaseAddress;

    LARGE_INTEGER NumberOfBytes;

} PHYSICAL_MEMORY_RANGE, * PPHYSICAL_MEMORY_RANGE;
EXTERN_C PPHYSICAL_MEMORY_RANGE MmGetPhysicalMemoryRanges();
inline DriverRequest* MyComunnicationMemory;
inline PMDL mappedMdl = nullptr;
inline SIZE_T mappedSize;
EXTERN_C NTSYSAPI NTSTATUS ZwFlushBuffersFile(
     HANDLE           FileHandle,
     PIO_STATUS_BLOCK IoStatusBlock
);
void writeLog(const char* log, ...)
{

    char buffer[8096] = { 0 };

    va_list args;
    va_start(args, log);

    NTSTATUS status = RtlStringCbVPrintfA(
        buffer,
        sizeof(buffer),
        log,
        args
    );

    va_end(args);

    if (!NT_SUCCESS(status))
        return;

    size_t length = 0;
    RtlStringCbLengthA(buffer, sizeof(buffer), &length);

    UNICODE_STRING path;
    RtlInitUnicodeString(&path, L"\\??\\C:\\DriverLogs.txt");

    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(
        &oa,
        &path,
        OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
        nullptr,
        nullptr
    );

    HANDLE file = nullptr;
    IO_STATUS_BLOCK iosb = { 0 };

    status = ZwCreateFile(
        &file,
        FILE_APPEND_DATA | SYNCHRONIZE,
        &oa,
        &iosb,
        nullptr,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        FILE_OPEN_IF,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_WRITE_THROUGH,
        nullptr,
        0
    );

    if (!NT_SUCCESS(status))
        return;

    status = ZwWriteFile(
        file,
        nullptr,
        nullptr,
        nullptr,
        &iosb,
        buffer,
        (ULONG)length,
        nullptr,
        nullptr
    );

    if (NT_SUCCESS(status))
    {
        //ZwFlushBuffersFile(file, &iosb);
    }

    ZwClose(file);
}

DriverRequest* findUmMemoryRequest()
{
    if (KeGetCurrentIrql() > APC_LEVEL)
    {
        writeLog("ERROR: IRQL too high for physical memory scan\n");
        return nullptr;
    }
    auto PhysicalMemoryRanges = MmGetPhysicalMemoryRanges();
    auto buffer = (DriverRequest*)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        sizeof(DriverRequest),
        'test'
    );
    if (!buffer) return nullptr;
    DriverRequest* result = nullptr;
    for (int i = 0;
        PhysicalMemoryRanges[i].BaseAddress.QuadPart ||
        PhysicalMemoryRanges[i].NumberOfBytes.QuadPart;
        i++)
    {
        if (!PhysicalMemoryRanges[i].BaseAddress.QuadPart || !PhysicalMemoryRanges[i].NumberOfBytes.QuadPart)
            continue;
        auto start = PhysicalMemoryRanges[i].BaseAddress;
        auto totalSize = (SIZE_T)PhysicalMemoryRanges[i].NumberOfBytes.QuadPart;
        for (SIZE_T offset = 0; offset < totalSize; offset += PAGE_SIZE)
        {
            PHYSICAL_ADDRESS chunkStart;
            chunkStart.QuadPart = start.QuadPart + offset;
            SIZE_T bytesRead = 0;
            MM_COPY_ADDRESS addr;
            addr.PhysicalAddress = chunkStart;
            auto status = MmCopyMemory(
                buffer, addr, sizeof(DriverRequest),
                0x1, &bytesRead
            );
            if (!NT_SUCCESS(status) || bytesRead != sizeof(DriverRequest))
                continue;
            if (!strcmp(buffer->Pattern, "kR7mZ2pLqW9xBn4vTjY1sHdF8cGaE3uNiO6wXrK5tMbJ0yDq"))
            {
                SIZE_T mappedSize = (sizeof(DriverRequest) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
                PVOID mapped = MmMapIoSpace(chunkStart, mappedSize, MmCached);
                mappedSize = mappedSize;
                if (mapped)
                {
                    DriverRequest* realRequest = (DriverRequest*)mapped;
                    realRequest->write = true;
                    result = realRequest;
                }
                goto done;
            }
        }
    }
done:
    ExFreePool(buffer);
    ExFreePool(PhysicalMemoryRanges);
    return result;
}


bool ProcessCommand()
{
    switch (MyComunnicationMemory->ControlCode)
    {
    case ControlCodes::ReadBuffer:
    {
        ULONG64 size = MyComunnicationMemory->size;
        if (size == 0 || size > sizeof(MyComunnicationMemory->Data))
        {
            writeLog("ReadBuffer: invalid size %llu\n", size);
            break;
        }

        char* data = (char*)ExAllocatePoolWithTag(NonPagedPool, size, 'test');
        if (!data)
        {
            writeLog("ReadBuffer: allocation failed\n");
            break;
        }

		const char* reply = "Hello from kernel!";
        RtlCopyMemory(data, (void*)MyComunnicationMemory->Data, size);
        writeLog("ReadBuffer: read %llu bytes from address %llX\n", size, MyComunnicationMemory->adress);
        RtlZeroMemory(MyComunnicationMemory->Data, sizeof(MyComunnicationMemory->Data));
        RtlCopyMemory(MyComunnicationMemory->Data, reply, strlen(reply));

        ExFreePoolWithTag(data, 'test');

        MyComunnicationMemory->result = 1;
        break;
    }
    case ControlCodes::UnloadDriver:
        return false;
    default:
        break;
    }
    MyComunnicationMemory->ControlCode = ControlCodes::None;
    InterlockedExchange(&MyComunnicationMemory->write, 0);
    return true;
}

NTSTATUS Sleep(ULONGLONG milliseconds)
{
    LARGE_INTEGER delay;
    ULONG* split;

    milliseconds *= 1000000;

    milliseconds /= 100;

    milliseconds = -milliseconds;

    split = (ULONG*)&milliseconds;

    delay.LowPart = *split;

    split++;

    delay.HighPart = *split;


    KeDelayExecutionThread(KernelMode, 0, &delay);

    return STATUS_SUCCESS;
}

void ComunicationThread()
{
    MyComunnicationMemory = findUmMemoryRequest();
    if (MyComunnicationMemory)
    {
        writeLog("Found usermode request at %p\n", MyComunnicationMemory);
        InterlockedExchange(&MyComunnicationMemory->write, 1);
        while (ReadNoFence(&MyComunnicationMemory->write) != 0)
            _mm_pause();
    }
    bool running = true;

    ULONG spins = 0;
    while (running)
    {
        if (ReadNoFence(&MyComunnicationMemory->write) == 1)
        {
            spins = 0;
            if (!ProcessCommand()) goto exit_loop;
        }
        else
        {
            if (spins < 2000)           _mm_pause();
            else                       Sleep(1);
            spins++;
        }
    }
exit_loop:
    writeLog("Driver loop exited cleanly\n");

    MyComunnicationMemory->ControlCode = ControlCodes::None;
    InterlockedExchange(&MyComunnicationMemory->write, 0);
    MyComunnicationMemory = nullptr;
}
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	UNREFERENCED_PARAMETER(DriverObject);
	HANDLE thread_handle;
	CLIENT_ID clientId;
	PsCreateSystemThread(&thread_handle, THREAD_ALL_ACCESS, 0, 0, &clientId, (PKSTART_ROUTINE)ComunicationThread, 0);
    writeLog("Thread Id %llX\n", clientId.UniqueThread);
	return STATUS_SUCCESS;
}