#include "stdafx.h"

/*Create from data receiver from server an Task struct*/
PTASK CreateClientTask(CM_DATA_BUFFER *TaskBuffer, CM_DATA_BUFFER *DataBuffer)
{
	PTASK newTask = NULL;
	CM_BYTE* buffer = NULL;

	if (TaskBuffer == NULL || DataBuffer == NULL)
	{
		return NULL;
	}

	//get the task id from TaskBuffer

	char taskId = -1;

	buffer = TaskBuffer->DataBuffer;
	taskId = buffer[0];

	CM_DATA_BUFFER* temp = NULL;
	if (TaskBuffer->UsedBufferSize > 2)
	{
		CM_ERROR error = CreateDataBuffer(&temp, 256);
		if (CM_IS_ERROR(error))
		{
			return NULL;
		}
		error = CopyDataIntoBuffer(temp, buffer + 1, TaskBuffer->UsedBufferSize - 1);
		if (CM_IS_ERROR(error))
		{
			DestroyDataBuffer(temp);
			return NULL;
		}
	}

	switch (taskId)
	{
	case 0:
		newTask = CreateTask(NULL, taskId, NULL, DataBuffer);
		break;
	case 1:
		newTask = CreateTask(NULL, taskId, temp, DataBuffer);
		if (newTask == NULL)
		{
			break;
		}
		break;
	default:
		newTask = NULL;
		break;
	}

	if (temp != NULL)
	{
		DestroyDataBuffer(temp);
	}

	return newTask;
}

void PrintMessage(CRITICAL_SECTION* WriteSection, CM_DATA_BUFFER* Message)
{
	if (Message == NULL)
	{
		return;
	}

	EnterCriticalSection(WriteSection);
	__try
	{
		//_tprintf_s(TEXT("%.S\n"), (char*)Message->DataBuffer);
		printf("%s\n", Message->DataBuffer);
	}
	__finally { LeaveCriticalSection(WriteSection); }
}

void PrintMessageFromSource(CRITICAL_SECTION* WriteSection, CM_DATA_BUFFER* FirstArgm, CM_DATA_BUFFER* SecondArgm)
{
	if (WriteSection == NULL || FirstArgm == NULL || SecondArgm == NULL)
	{
		return;
	}
	EnterCriticalSection(WriteSection);
	__try
	{
		printf("%s %s\n", FirstArgm->DataBuffer, SecondArgm->DataBuffer);
	}
	__finally { LeaveCriticalSection(WriteSection); }
}

void ReceiveFile(CRITICAL_SECTION *WriteFileSection, PTASK Task)
{
	HANDLE file = INVALID_HANDLE_VALUE;
	if (Task == NULL)
	{
		return;
	}

	EnterCriticalSection(WriteFileSection);
	__try
	{
		file = CreateFileA((LPCSTR)Task->FirstParam->DataBuffer, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE)
		{
			__leave;
		}

		LARGE_INTEGER position;
		position.QuadPart = 0;
		SetFilePointerEx(file, position, NULL, FILE_END);
		DWORD bytesWrited = 0;

		WriteFile(file, Task->SecondParam->DataBuffer, Task->SecondParam->UsedBufferSize, &bytesWrited, NULL);
	}
	__finally { LeaveCriticalSection(WriteFileSection); }

}