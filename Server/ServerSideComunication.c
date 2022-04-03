#include "stdafx.h"

PSERVER_SIDE_COMUNICATION CreateSSComunication(CM_SERVER* Server, PTASKS_QUEUE ServerTasksQueue)
{
	PSERVER_SIDE_COMUNICATION newSSComunication = NULL;

	if (Server == NULL || ServerTasksQueue == NULL)
	{
		return NULL;
	}

	newSSComunication = (PSERVER_SIDE_COMUNICATION)malloc(sizeof(SERVER_SIDE_COMUNICATION));
	if (newSSComunication == NULL)
	{
		return NULL;
	}
	newSSComunication->Server = Server;
	newSSComunication->Client = NULL;
	newSSComunication->ServerTasksQueque = ServerTasksQueue;
	newSSComunication->Thread = INVALID_HANDLE_VALUE;
	newSSComunication->Username = NULL;
	PDATA_QUEUE newDataQueue = CreateDataQueue();
	if (newDataQueue == NULL)
	{
		free(newSSComunication);
		return NULL;
	}
	newSSComunication->SendDataQueue = newDataQueue;
	InitializeCriticalSection(&(newSSComunication->CritSection));
	InitializeConditionVariable(&(newSSComunication->EmptyQueue));
	WakeAllConditionVariable(&(newSSComunication->EmptyQueue));
	
	return newSSComunication;
}

INT DestroySSComunication(PSERVER_SIDE_COMUNICATION ServerSideCommunication)
{
	if (ServerSideCommunication == NULL)
	{
		return -1;
	}

	DestroyDataQueue(ServerSideCommunication->SendDataQueue);
	if (ServerSideCommunication->Username != NULL)
	{
		DestroyDataBuffer(ServerSideCommunication->Username);
	}
	
	return 0;
}

CM_DATA_BUFFER* CreateClientTaskDataBuffer(char TaskId, CM_DATA_BUFFER *TaskArgument)
{
	// Concatenate TaskId with TaskArgument
	
	CM_DATA_BUFFER* taskBuffer = NULL;
	CM_SIZE bufferSize = 0;
	STRSAFE_LPSTR data;

	data = (STRSAFE_LPSTR)malloc(sizeof(CHAR) * 255);
	if (data == NULL)
	{
		return NULL;
	}

	CM_ERROR error = CreateDataBuffer(&taskBuffer, 255);
	if (CM_IS_ERROR(error))
	{
		_tprintf_s(TEXT("CreateDataBuffer failed with err-code=0x%X!\n"), error);
		free(data);
		return NULL;
	}

	// Create data buffer with taskId and arguments concatenated

	LPSTR taskIdBuffer = (LPSTR)malloc(sizeof(CHAR) * 2);
	memset(taskIdBuffer, 0, 2);
	taskIdBuffer[0] = TaskId;
	taskIdBuffer[1] = '\0';

	StringCbCopyA(data, 255, (STRSAFE_LPSTR)taskIdBuffer);
	bufferSize = 2;

	if (TaskArgument != NULL)
	{
		StringCchCatA(data, 255, (STRSAFE_LPCSTR)TaskArgument->DataBuffer);
		bufferSize += TaskArgument->UsedBufferSize;
	}

	
	error = CopyDataIntoBuffer(taskBuffer, (CM_BYTE*)data, bufferSize);
	if (CM_IS_ERROR(error))
	{
		_tprintf_s(TEXT("CreateDataBuffer failed with err-code=0x%X!\n"), error);
		free(data);
		DestroyDataBuffer(taskBuffer);
		return NULL;
	}

	free(data);

	return taskBuffer;
}


void ProtectedEnqueueData(PSERVER_SIDE_COMUNICATION ServerSideComunication, PDATA Data)
{
	if (Data == NULL)
	{
		return;
	}

	if (ServerSideComunication == NULL)
	{
		DestroyData(Data);
		return;
	}

	EnterCriticalSection(&(ServerSideComunication->CritSection));
	__try
	{
		if (EnqueueData(ServerSideComunication->SendDataQueue, Data) >= 0)
		{
			WakeConditionVariable(&(ServerSideComunication->EmptyQueue));
		}
		else
		{
			_tprintf_s(TEXT("Error: Cannot enqueue DATA element!\n"));
			DestroyData(Data);
		}
	}
	__finally { LeaveCriticalSection(&(ServerSideComunication->CritSection)); }
}

PDATA_QUEUE CreateDataQueue()
{
	PDATA_QUEUE newDataQueue = NULL;

	newDataQueue = (PDATA_QUEUE)malloc(sizeof(DATA_QUEUE));
	if (newDataQueue == NULL)
	{
		return NULL;
	}

	newDataQueue->First = NULL;
	newDataQueue->Last = NULL;

	return newDataQueue;
}

void DestroyDataQueue(PDATA_QUEUE DataQueue)
{
	PDATA start = NULL;
	PDATA temp = NULL;

	if (DataQueue == NULL)
	{
		return;
	}
	start = DataQueue->First;
	while (start)
	{
		temp = start;
		start = start->Next;
		free(temp->ClientTask);
		free(temp->DataBuffer);
		free(temp);
	}
	free(DataQueue);
}

PDATA CreateData()
{
	PDATA newData = NULL;

	newData = (PDATA)malloc(sizeof(DATA));
	if (newData == NULL)
	{
		return NULL;
	}
	newData->ClientTask = NULL;
	newData->DataBuffer = NULL;
	newData->Next = NULL;

	return newData;
}

void DestroyData(PDATA Data)
{
	if (Data == NULL)
	{
		return;
	}

	DestroyDataBuffer(Data->ClientTask);
	DestroyDataBuffer(Data->DataBuffer);
	free(Data);
}

INT EnqueueData(PDATA_QUEUE Queue, PDATA Data)
{
	PDATA first = NULL;
	PDATA last = NULL;
	if (Queue == NULL || Data == NULL)
	{
		return -1;
	}

	first = Queue->First;
	last = Queue->Last;

	if (first == NULL && last == NULL)
	{
		first = Data;
		last = Data;
	}
	else
	{
		last->Next = Data;
		last = Data;
	}

	Queue->First = first;
	Queue->Last = last;

	return 0;
}

PDATA DequeueData(PDATA_QUEUE Queue)
{
	PDATA first = NULL;
	PDATA last = NULL;
	PDATA data = NULL;

	if (Queue == NULL)
	{
		return NULL;
	}

	first = Queue->First;
	last = Queue->Last;

	if (first == NULL && last == NULL)
	{
		return NULL;
	}
	else if (first == last)
	{
		data = first;
		first = NULL;
		last = NULL;
	}
	else
	{
		data = first;
		first = first->Next;
	}

	Queue->First = first;
	Queue->Last = last;

	return data;
}