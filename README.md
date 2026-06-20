Shared-Memory

Shared-Memory is a small C++ project where I explore a way to communicate between a kernel mode driver and a user mode application on Windows without relying on the usual IOCTL or named device approach.

The core idea is pretty straightforward: on the user mode side, I allocate a single page of memory (exactly 4096 bytes) with VirtualAlloc, lock it into physical RAM with VirtualLock, and stamp it with a unique 49 character alphanumeric pattern that acts as a signature. Then I just wait for the driver to find it.

On the kernel side, I spin up a system thread that walks through every physical memory range on the machine using MmGetPhysicalMemoryRanges and MmCopyMemory, scanning page by page until it hits the magic pattern. Once found, I map that physical address with MmMapIoSpace, and from that point on both sides share the same structure. Communication happens through atomic flags (InterlockedExchange, ReadNoFence) as a simple signaling mechanism — one side writes and sets the flag to 1, the other processes it and drops it back to 0.

For the demo, the user-mode app sends random strings and the driver replies with a "Hello from kernel!". There's also a clean unload command to shut the driver down gracefully.

This is mostly an educational exercise, I built it to understand how the kernel/userland boundary works at a low level, outside the usual WDM abstractions. It's a learning project for physical memory, paging, and cross-mode synchronization
