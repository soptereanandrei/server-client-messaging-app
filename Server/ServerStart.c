#include "stdafx.h"

DWORD ConnexionThread(PVOID ServerSideComunication);
DWORD ServerReceiverThread(PVOID ServerSideComunication);
DWORD ServerSenderThread(PVOID ServerSideComunication);
DWORD ServerWorker(PVOID Void);

CRITICAL_SECTION gQueueSection;
CONDITION_VARIABLE gWorkAvaible;
SRWLOCK gSharedData;
SRWLOCK gUsersFile;
PTASKS_QUEUE gServerTasksQueue = NULL;
PSERVER_SIDE_COMUNICATION* gServerSideComunications = NULL;
INT gMaximumConnections;

int _tmain(int argc, TCHAR** argv)
{
	if (argc != 2)
	{
		_tprintf(_T("usage: ./MessageServer param"));
		return -1;
	}

	gMaximumConnections = _ttoi(argv[1]);
	if (gMaximumConnections <= 0 || gMaximumConnections > MAX_CLIENTS_CONNECTIONS)
	{
		_tprintf(_T("Error: invalid maximum number of connections\n"));
		return -1;
	}

	EnableCommunicationModuleLogger();

	CM_ERROR error = InitCommunicationModule();
	if (CM_IS_ERROR(error))
	{
		_tprintf_s(TEXT("InitCommunicationModule failed with err-code=0x%X!\n"), error);
		return -1;
	}
	
	CM_SERVER *server = NULL;
	error = CreateServer(&server);
	if (CM_IS_ERROR(error))
	{
		_tprintf_s(TEXT("CreateServer failed with err-code=0x%X!\n"), error);
		UninitCommunicationModule();
		return -1;
	}

	_tprintf_s(TEXT("Server is Up & Running...\n"));

	if (CreateTasksQueue(&gServerTasksQueue) < 0)
	{
		_tprintf_s(TEXT("Error: Cannot allocate memory for tasks queue!\n"));
		DestroyServer(server);
		UninitCommunicationModule();
		return -1;
	}

	InitializeCriticalSection(&gQueueSection);

	InitializeConditionVariable(&gWorkAvaible);
	WakeAllConditionVariable(&gWorkAvaible);

	InitializeSRWLock(&gSharedData);
	InitializeSRWLock(&gUsersFile);

	// Create Thread Pool
	HANDLE workersThreadPool[WORKER_THREADS];

	for (int i = 0; i < WORKER_THREADS; i++)
	{
		HANDLE newWorker = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ServerWorker, NULL, 0, NULL);
		if (newWorker == INVALID_HANDLE_VALUE)
		{
			_tprintf_s(TEXT("Error: Cannot create Worker Threads!\n"));
			DestroyServer(server);
			UninitCommunicationModule();
			return -1;
		}
		workersThreadPool[i] = newWorker;
	}

	gServerSideComunications = (PSERVER_SIDE_COMUNICATION*)malloc(sizeof(PSERVER_SIDE_COMUNICATION) * ((int)gMaximumConnections + 1));
	if (gServerSideComunications == NULL)
	{
		_tprintf_s(TEXT("Error: Cannot allocate memory for PSERVER_SIDE_COMUNICATION!\n"));
		DestroyServer(server);
		UninitCommunicationModule();
		return -1;
	}

	for (int i = 0; i < gMaximumConnections; i++)
	{
		gServerSideComunications[i] = NULL;
	}
	gServerSideComunications[gMaximumConnections] = NULL;

	//Create initial threads for wait the new clients
	for (int i = 0; i < gMaximumConnections; i++)
	{
		PSERVER_SIDE_COMUNICATION newSSComunication = CreateSSComunication(server, gServerTasksQueue);
		if (newSSComunication == NULL)
		{
			_tprintf_s(TEXT("Error: CreateSSComunication failed!\n"));
			goto fail;
		}
		HANDLE hNewThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ConnexionThread, newSSComunication, CREATE_SUSPENDED, NULL);
		if (hNewThread == INVALID_HANDLE_VALUE)
		{
			_tprintf_s(TEXT("Error: CreateThread failed!\n"));
			goto fail;
		}
		newSSComunication->Thread = hNewThread;
		gServerSideComunications[i] = newSSComunication;
	}

	DWORD dwActiveThreadsCount = 0;

	for (int i = 0; i < gMaximumConnections; i++)
	{
		ResumeThread(gServerSideComunications[i]->Thread);
		dwActiveThreadsCount++;
	}

	//Checks client exit via thread exit and create new threads for new clients
	while (1)
	{
		HANDLE* hThreadsHandles = (HANDLE*)malloc(sizeof(HANDLE) * dwActiveThreadsCount);
		if (hThreadsHandles == NULL)
		{
			_tprintf_s(TEXT("Error: Cannot allocate memory for thread HANDLES!\n"));
			goto fail;
		}

		for (DWORD i = 0; i < dwActiveThreadsCount; i++)
		{
			hThreadsHandles[i] = gServerSideComunications[i]->Thread;
		}
		
		DWORD dwWait = WaitForMultipleObjects(dwActiveThreadsCount, hThreadsHandles, FALSE, INFINITE);
		free(hThreadsHandles);
		hThreadsHandles = NULL;

		//Do clean in the HANDLEs vector

		AcquireSRWLockExclusive(&gSharedData); //Change shared data
		DWORD dwHandleIndex = dwWait - WAIT_OBJECT_0;
		//DWORD dwHandleIndex = 0;
		for (; dwHandleIndex < dwActiveThreadsCount; dwHandleIndex++)
		{
			if (WaitForSingleObject(gServerSideComunications[dwHandleIndex]->Thread, 0) == WAIT_OBJECT_0)
			{
				DWORD temp = dwHandleIndex;

				CloseHandle(gServerSideComunications[temp]->Thread);
				DestroySSComunication(gServerSideComunications[temp]);
				free(gServerSideComunications[temp]);

				for (; temp < dwActiveThreadsCount - 1; temp++)
				{
					gServerSideComunications[temp] = gServerSideComunications[temp + 1];
				}
				gServerSideComunications[temp] = NULL;
				dwActiveThreadsCount--;
				dwHandleIndex--;
			}
		}
		//Create the new threads for the new clients
		for (DWORD i = dwActiveThreadsCount; i < (DWORD)gMaximumConnections; i++)
		{
			PSERVER_SIDE_COMUNICATION newSSComunication = CreateSSComunication(server, gServerTasksQueue);
			if (newSSComunication == NULL)
			{
				_tprintf_s(TEXT("Error: CreateSSComunication failed!\n"));
				goto fail;
			}
			HANDLE hNewThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ConnexionThread, newSSComunication, CREATE_SUSPENDED, NULL);
			if (hNewThread == INVALID_HANDLE_VALUE)
			{
				_tprintf_s(TEXT("Error: CreateThread failed!\n"));
				goto fail;
			}
			//Necesita modificare
			newSSComunication->Thread = hNewThread;
			gServerSideComunications[i] = newSSComunication;
		}

		for (DWORD i = dwActiveThreadsCount; i < (DWORD)gMaximumConnections; i++)
		{
			ResumeThread(gServerSideComunications[i]->Thread);
			dwActiveThreadsCount++;
		}

		ReleaseSRWLockExclusive(&gSharedData);
	}
	
	return 0;

fail:
	DestroyServer(server);
	for (int i = 0; i < gMaximumConnections; i++)
	{
		if (gServerSideComunications[i] != NULL)
		{
			free(gServerSideComunications[i]);
		}
	}
	free(gServerSideComunications);
	UninitCommunicationModule();
	return -1;
}

DWORD ConnexionThread(PVOID ServerSideComunication)
{
	PSERVER_SIDE_COMUNICATION pServerSideComunication = (PSERVER_SIDE_COMUNICATION)ServerSideComunication;
	CM_SERVER_CLIENT* newClient = NULL;

	CM_ERROR error = AwaitNewClient(pServerSideComunication->Server, &newClient);
	if (CM_IS_ERROR(error))
	{
		_tprintf_s(TEXT("AwaitNewClient failed with err-code=0x%X!\n"), error);
		return 1;
	}

	pServerSideComunication->Client = newClient;

	HANDLE hThreadHandles[2];

	HANDLE newThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ServerReceiverThread, ServerSideComunication, 0, NULL);
	if (newThread == INVALID_HANDLE_VALUE)
	{
		_tprintf_s(TEXT("Error: Cannot create ReceiverThread!\n"));
		return 2;
	}
	hThreadHandles[0] = newThread;
	newThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ServerSenderThread, ServerSideComunication, 0, NULL);
	if (newThread == INVALID_HANDLE_VALUE)
	{
		_tprintf_s(TEXT("Error: Cannot create SenderThread!\n"));
		return 3;
	}
	hThreadHandles[1] = newThread;

	WaitForMultipleObjects(2, hThreadHandles, TRUE, INFINITE);

	_tprintf_s(TEXT("Connection end!\n"));
	AbandonClient(newClient);

	return 0;
}

DWORD ServerReceiverThread(PVOID ServerSideComunication)
{
	PSERVER_SIDE_COMUNICATION pServerSideComunication = NULL;

	pServerSideComunication = (PSERVER_SIDE_COMUNICATION)ServerSideComunication;

	CM_DATA_BUFFER* dataToReceive = NULL;
	CM_SIZE dataToReceiveSize = 255;


	while (1)
	{
		BOOL receiveFailure = FALSE;

		CM_ERROR error = CreateDataBuffer(&dataToReceive, dataToReceiveSize);
		if (CM_IS_ERROR(error))
		{
			_tprintf_s(TEXT("Failed to create RECEIVE data buffer with err-code=0x%X!\n"), error);;
			return 1;
		}

		CM_SIZE receivedByteCount = 0;
		error = ReceiveDataFromClient(pServerSideComunication->Client, dataToReceive, &receivedByteCount);
		if (CM_IS_ERROR(error))
		{
			_tprintf_s(TEXT("ReceiveDataFromClient failed with err-code=0x%X!\n"), error);
			DestroyDataBuffer(dataToReceive);
			receiveFailure = TRUE; //Create a special task which will disconnect thread from actual client
		}

		if (receiveFailure == FALSE)
		{
			_tprintf_s(TEXT("Successfully received data from client:\n \tReceived data: \" %.*S \" \n \tReceived data size: %d\n")
				, (int)receivedByteCount
				, (char*)dataToReceive->DataBuffer
				, receivedByteCount
			);
		}

		//Create server task for workers

		PTASK newTask = NULL;

		if (receiveFailure == FALSE)
		{
			newTask = CreateServerTask(pServerSideComunication->Username, pServerSideComunication->Client, dataToReceive);//atentie la crearea bufferului
		}
		//else
		//{
		//	newTask = CreateTask(-1, NULL, NULL);//special task which disconnect thread from actual client
		//}

		//enqueue task in ServerTasksQueue
		if (newTask != NULL)
		{
			EnterCriticalSection(&gQueueSection);
			__try
			{
				if (EnqueueTask(gServerTasksQueue, newTask) >= 0)
				{
					WakeConditionVariable(&gWorkAvaible);
				}
				else
				{
					_tprintf_s(_TEXT("Error: EnqueueTask failed!\n"));
				}
			}
			__finally { LeaveCriticalSection(&gQueueSection); }
		}

		if (receiveFailure == TRUE)
		{
			break;
		}

		DestroyDataBuffer(dataToReceive);
	}

	return 0;
}

DWORD ServerSenderThread(PVOID ServerSideComunication)
{
	PSERVER_SIDE_COMUNICATION pServerSideComunication = NULL;

	pServerSideComunication = (PSERVER_SIDE_COMUNICATION)ServerSideComunication;

	while (1)
	{
		PDATA dataToSend = NULL;
		EnterCriticalSection(&(pServerSideComunication->CritSection));
		__try
		{
			while ((dataToSend = DequeueData(pServerSideComunication->SendDataQueue)) == NULL)
			{
				SleepConditionVariableCS(&(pServerSideComunication->EmptyQueue), &(pServerSideComunication->CritSection), INFINITE);
			}
		}
		__finally { LeaveCriticalSection(&(pServerSideComunication->CritSection)); }

		CM_SIZE sendByteCount = 0;

		//Send task to client
		CM_ERROR error = SendDataToClient(pServerSideComunication->Client, dataToSend->ClientTask, &sendByteCount);
		if (CM_IS_ERROR(error))
		{
			_tprintf_s(TEXT("SendDataToClient failed with err-code=0x%X!\n"), error);
			DestroyDataBuffer(dataToSend->ClientTask);
			DestroyDataBuffer(dataToSend->DataBuffer);
			free(dataToSend);
			return 1;
		}

		//Send data to client
		error = SendDataToClient(pServerSideComunication->Client, dataToSend->DataBuffer, &sendByteCount);
		if (CM_IS_ERROR(error))
		{
			_tprintf_s(TEXT("SendDataToClient failed with err-code=0x%X!\n"), error);
			DestroyDataBuffer(dataToSend->ClientTask);
			DestroyDataBuffer(dataToSend->DataBuffer);
			free(dataToSend);
			return 1;
		}

		_tprintf_s(TEXT("Successfully send data from server to client:\n \tSend data size: %d\n")
			, sendByteCount
		);

		//Destroy task after sent it
		DestroyDataBuffer(dataToSend->ClientTask);
		DestroyDataBuffer(dataToSend->DataBuffer);
		free(dataToSend);
	}

	return 0;
}

DWORD ServerWorker(PVOID Void)
{
	(void)Void;

	PTASK task = NULL;

	while (1)
	{
		EnterCriticalSection(&gQueueSection);
		__try
		{
			while ((task = DequeueTask(gServerTasksQueue)) == NULL)
			{
				SleepConditionVariableCS(&gWorkAvaible, &gQueueSection, INFINITE);
			}
			WakeConditionVariable(&gWorkAvaible);
		}
		__finally { LeaveCriticalSection(&gQueueSection); }

		AcquireSRWLockShared(&gSharedData);
		if (task != NULL)
		{
			switch (task->TaskId)
			{
			case 0 : 
				ExecuteEchoTask(gServerSideComunications, task);
				break;
			case 1 :
				ExecuteRegisterTask(gServerSideComunications, &gUsersFile, task);
				break;
			case 2:
				ExecuteLoginTask(gServerSideComunications, &gUsersFile, task);
				break;
			case 3:
				ExecuteLogoutTask(gServerSideComunications, task);
				break;
			case 4:
				ExecuteMsgTask(gServerSideComunications, &gUsersFile, task);
				break;
			case 5:
				ExecuteBroadcastTask(gServerSideComunications, task);
				break;
			case 6:
				ExecuteSendfileTask(gServerSideComunications, &gUsersFile, task);
			default :
				break;
			}
		}
		ReleaseSRWLockShared(&gSharedData);

		DestroyTask(task);
	}
}