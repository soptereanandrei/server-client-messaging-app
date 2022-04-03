#pragma once
#include "stdafx.h"

typedef struct _DATA {
	CM_DATA_BUFFER *ClientTask;
	CM_DATA_BUFFER *DataBuffer;
	struct _DATA* Next;
}DATA, * PDATA;

typedef struct _DATA_QUEUE {
	PDATA First;
	PDATA Last;
}DATA_QUEUE, * PDATA_QUEUE;

PDATA_QUEUE CreateDataQueue();
void DestroyDataQueue(PDATA_QUEUE DataQueue);
PDATA CreateData();
void DestroyData(PDATA Data);
INT EnqueueData(PDATA_QUEUE Queue, PDATA Data);
PDATA DequeueData(PDATA_QUEUE Queue);

typedef struct _SERVER_SIDE_COMUNICATION {
	CM_SERVER* Server;
	CM_SERVER_CLIENT* Client;
	PTASKS_QUEUE ServerTasksQueque;
	HANDLE Thread;
	CM_DATA_BUFFER *Username;
	PDATA_QUEUE SendDataQueue; //Data which wait to be send to client
	CRITICAL_SECTION CritSection; //To protect DataQueue from corruption
	CONDITION_VARIABLE EmptyQueue;
}SERVER_SIDE_COMUNICATION, * PSERVER_SIDE_COMUNICATION;

PSERVER_SIDE_COMUNICATION CreateSSComunication(CM_SERVER* Server, PTASKS_QUEUE ServerTasksQueue);
INT DestroySSComunication(PSERVER_SIDE_COMUNICATION ServerSideCommunication);

/*
Concatenate clientTaskId with TaskArgument
*/
CM_DATA_BUFFER *CreateClientTaskDataBuffer(char TaskId, CM_DATA_BUFFER *TaskArgument);
void ProtectedEnqueueData(PSERVER_SIDE_COMUNICATION ServerSideComunication, PDATA Data);
