// MessageClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

DWORD ClientSenderThread(PVOID ServerSideComunication);
DWORD ClientReceiverThread(PVOID ServerSideComunication);
DWORD ClientWorker(PVOID Void);

PTASKS_QUEUE gClientTasksQueue;
CRITICAL_SECTION gQueueSection;
CONDITION_VARIABLE gEmptyQueue;

CRITICAL_SECTION gConsole;//using for mutual exclude in context of console read or write
CRITICAL_SECTION gWriteFile;

int _tmain(int argc, TCHAR* argv[])
{
    (void)argc;
    (void)argv;

    EnableCommunicationModuleLogger();

    CM_ERROR error = InitCommunicationModule();
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("InitCommunicationModule failed with err-code=0x%X!\n"), error);
        return -1;
    }

    CM_CLIENT* client = NULL;
    error = CreateClientConnectionToServer(&client);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("CreateClientConnectionToServer failed with err-code=0x%X!\n"), error);
        UninitCommunicationModule();
        return -1;
    }

    if (CreateTasksQueue(&gClientTasksQueue) < 0)
    {
        _tprintf_s(TEXT("Error: Cannot allocate memory for tasks queue!\n"));
        DestroyClient(client);
        UninitCommunicationModule();
        return -1;
    }

    _tprintf_s(TEXT("We are connected to the server...\n"));

    InitializeCriticalSection(&gQueueSection);

    InitializeConditionVariable(&gEmptyQueue);
    WakeAllConditionVariable(&gEmptyQueue);

    InitializeCriticalSection(&gConsole);
    InitializeCriticalSection(&gWriteFile);

    HANDLE hWorkersHandles[WORKER_THREADS];

    for (int i = 0; i < WORKER_THREADS; i++)
    {
        HANDLE newWorkerThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ClientWorker, NULL, 0, NULL);
        if (newWorkerThread == INVALID_HANDLE_VALUE)
        {
            DestroyClient(client);
            return -1;
        }
        CloseHandle(newWorkerThread);
        hWorkersHandles[i] = newWorkerThread;
    }

    HANDLE hThreadHandles[2];

    HANDLE newThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ClientReceiverThread, client, 0, NULL);
    if (newThread == INVALID_HANDLE_VALUE)
    {
        _tprintf_s(TEXT("Error: Cannot create ReceiverThread!\n"));
        return 2;
    }
    hThreadHandles[0] = newThread;
    newThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ClientSenderThread, client, 0, NULL);
    if (newThread == INVALID_HANDLE_VALUE)
    {
        _tprintf_s(TEXT("Error: Cannot create SenderThread!\n"));
        return 3;
    }
    hThreadHandles[1] = newThread;

    WaitForMultipleObjects(2, hThreadHandles, TRUE, INFINITE);

    _tprintf_s(TEXT("Connection end!\n"));
    return 0;
}

DWORD ClientSenderThread(PVOID ServerSideComunication)
{
    CM_CLIENT* client = (CM_CLIENT*)ServerSideComunication;

    char messageToSend[256];
    const SIZE_T messageToSendSize = 255;

    CM_DATA_BUFFER* dataToSend = NULL;
    CM_SIZE dataToSendSize = (CM_SIZE)messageToSendSize;

    while (TRUE)
    {
        //EnterCriticalSection(&gConsole);
        //__try
        //{
            if (gets_s(messageToSend, messageToSendSize) == NULL)
            {
                _tprintf_s(TEXT("Failed to read from stdin!\n"));
                //__leave;
                break;
            }
        //}
        //__finally { LeaveCriticalSection(&gConsole);}

        CM_ERROR error = CreateDataBuffer(&dataToSend, dataToSendSize);
        if (CM_IS_ERROR(error))
        {
            _tprintf_s(TEXT("Failed to create SEND data buffer with err-code=0x%X!\n"), error);
            break;
        }

        error = CopyDataIntoBuffer(dataToSend, (const CM_BYTE*)messageToSend, (CM_SIZE)(strnlen_s(messageToSend, 255) + 1));
        if (CM_IS_ERROR(error))
        {
            _tprintf_s(TEXT("CopyDataIntoBuffer failed with err-code=0x%X!\n"), error);
            DestroyDataBuffer(dataToSend);
            break;
        }

        CM_SIZE sendBytesCount = 0;
        error = SendDataToServer(client, dataToSend, &sendBytesCount);
        if (CM_IS_ERROR(error))
        {
            _tprintf_s(TEXT("SendDataToServer failed with err-code=0x%X!\n"), error);
            DestroyDataBuffer(dataToSend);
            break;
        }

        /* DEBUG
        _tprintf_s(TEXT("Successfully send data to server:\n \t Send data size: %d\n")
            , sendBytesCount
        );
        */

        DestroyDataBuffer(dataToSend);
    }

    DestroyClient(client);
    UninitCommunicationModule();

    return 0;
}

DWORD ClientReceiverThread(PVOID ServerSideComunication)
{
    CM_CLIENT* client = (CM_CLIENT*)ServerSideComunication;

    CM_DATA_BUFFER* taskBuffer = NULL;
    CM_DATA_BUFFER* dataBuffer = NULL;

    while (TRUE)
    {
        CM_SIZE receivedByteCount = 0;

        CM_ERROR error = CreateDataBuffer(&taskBuffer, 256);
        if (CM_IS_ERROR(error))
        {
            _tprintf_s(TEXT("Failed to create taskBuffer with err-code=0x%X!\n"), error);
            return 0;
        }

        error = CreateDataBuffer(&dataBuffer, 256);
        if (CM_IS_ERROR(error))
        {
            _tprintf_s(TEXT("Failed to create taskBuffer with err-code=0x%X!\n"), error);
            DestroyDataBuffer(taskBuffer);
            return 0;
        }

        //Receive task from server
        error = ReceiveDataFormServer(client, taskBuffer, &receivedByteCount);
        if (CM_IS_ERROR(error))
        {
            _tprintf_s(TEXT("ReceiveDataFormServer failed with err-code=0x%X!\n"), error);
            break;
        }

        //Receive data from server
        error = ReceiveDataFormServer(client, dataBuffer, &receivedByteCount);
        if (CM_IS_ERROR(error))
        {
            _tprintf_s(TEXT("ReceiveDataFormServer failed with err-code=0x%X!\n"), error);
            break;
        }

        /* DEBUG
        _tprintf_s(TEXT("Successfully received data from server:\n \tReceived data: \" %.*S \" \n \tReceived data size: %d\n")
            , (int)receivedByteCount
            , (char*)dataBuffer->DataBuffer
            , receivedByteCount
        );
        */

        PTASK clientTask = CreateClientTask(taskBuffer, dataBuffer);

        if (clientTask != NULL)
        {
            //Enqueue task
            EnterCriticalSection(&gQueueSection);
            __try
            {
                if (EnqueueTask(gClientTasksQueue, clientTask) >= 0)
                {
                    WakeAllConditionVariable(&gEmptyQueue);
                }
            }
            __finally { LeaveCriticalSection(&gQueueSection); }
        }

        DestroyDataBuffer(taskBuffer);
        DestroyDataBuffer(dataBuffer);
    }

    DestroyDataBuffer(taskBuffer);
    DestroyDataBuffer(dataBuffer);
    UninitCommunicationModule();

    return 0;
}

DWORD ClientWorker(PVOID Void)
{
    (void)Void;

    while (TRUE)
    {
        PTASK clientTask = NULL;
        //Dequeue task
        EnterCriticalSection(&gQueueSection);
        __try
        {
            while ((clientTask = DequeueTask(gClientTasksQueue)) == NULL)
            {
                SleepConditionVariableCS(&gEmptyQueue, &gQueueSection, INFINITE);
            }
        }
        __finally { LeaveCriticalSection(&gQueueSection); }

        //Manipulate task
        if (clientTask != NULL)
        {
            switch (clientTask->TaskId)
            {
            case 0:
                PrintMessage(&gConsole, clientTask->SecondParam);
                break;
            case 1:
                //receive file
                break;
            default:
                break;
            }

            DestroyTask(clientTask);
            clientTask = NULL;
        }
    }
}