#pragma once
#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>
#include "communication_api.h"

typedef struct _TASK {
	CM_SERVER_CLIENT* Client;
	char TaskId;
	CM_DATA_BUFFER *FirstParam;
	CM_DATA_BUFFER *SecondParam;
	struct _TASK* Next;
}TASK, * PTASK;


typedef struct _TASKS_QUEUE {
	PTASK First;
	PTASK Last;
}TASKS_QUEUE, *PTASKS_QUEUE;

INT CreateTasksQueue(PTASKS_QUEUE* Queue);
PTASK CreateTask(CM_SERVER_CLIENT* Client, char TaskId, CM_DATA_BUFFER* FirstParam, CM_DATA_BUFFER* SecondParam);
INT DestroyTask(PTASK Task);
INT EnqueueTask(PTASKS_QUEUE Queue, PTASK Task);
PTASK DequeueTask(PTASKS_QUEUE Queue);
