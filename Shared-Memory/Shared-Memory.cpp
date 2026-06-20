// Shared-Memory.cpp : Este archivo contiene la función "main". La ejecución del programa comienza y termina ahí.
//

#include "Driver.h"

char* generateRandomData(size_t size)
{
	char* data = new char[size + 1];
	srand((unsigned)time(NULL));
	for (size_t i = 0; i < size; i++)
	{
		data[i] = 'A' + (rand() % 26);
	}
	data[size] = '\0';
	return data;
}


int main()
{
    printf("Initializing driver communication...\n");
    if (!Driver::Init())
    {
        printf("Driver initialization failed\n");
        return -1;
    }
    printf("Driver communication initialized successfully\n");
    int attemps = 0;
    while (attemps < 5)
    {
        printf("Waiting for driver response...\n");
        auto randomData = generateRandomData(10);
        printf("Sending data: %s\n", randomData);

        Driver::sendRequest<char*>(ControlCodes::ReadBuffer, 10, (uintptr_t)randomData);

        if (Driver::DriverMemory->result == 1)
        {
            Driver::DriverMemory->Data[sizeof(Driver::DriverMemory->Data) - 1] = '\0';
            char* driverResponse = reinterpret_cast<char*>(Driver::DriverMemory->Data);
            printf("Driver responded: %s\n", driverResponse);
        }
        else
        {
            printf("Driver response failed, attempt %d\n", attemps + 1);
        }

        delete[] randomData;
        attemps++;

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}