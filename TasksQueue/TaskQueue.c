#include "TasksQueue.h"

INT CreateTasksQueue(PTASKS_QUEUE* Queue)
{
	PTASKS_QUEUE newQueue = NULL;

	if (Queue == NULL)
	{
		return -1;
	}

	newQueue = (PTASKS_QUEUE)malloc(sizeof(TASKS_QUEUE));
	if (newQueue == NULL)
	{
		return -1;
	}

	newQueue->First = NULL;
	newQueue->Last = NULL;

	*Queue = newQueue;

	return 0;
}

PTASK CreateTask(CM_SERVER_CLIENT* Client, char TaskId, CM_DATA_BUFFER* FirstParam, CM_DATA_BUFFER* SecondParam)
{
	PTASK newTask = NULL;

	newTask = (PTASK)malloc(sizeof(TASK));
	if (newTask == NULL)
	{
		return NULL;
	}

	newTask->Client = Client;
	newTask->TaskId = TaskId;

	if (FirstParam != NULL)
	{
		CM_ERROR error = CreateDataBufferByCopy(&(newTask->FirstParam), FirstParam);
		if (CM_IS_ERROR(error))
		{
			goto _fail;
		}
	}
	else
	{
		newTask->FirstParam = NULL;
	}

	if (SecondParam != NULL)
	{
		CM_ERROR error = CreateDataBufferByCopy(&(newTask->SecondParam), SecondParam);
		if (CM_IS_ERROR(error))
		{
			DestroyDataBuffer(newTask->FirstParam);
			goto _fail;
		}
	}
	else
	{
		newTask->SecondParam = NULL;
	}

	newTask->Next = NULL;

	return newTask;

_fail:
	free(newTask);
	return NULL;
}

INT DestroyTask(PTASK Task)
{
	if (Task == NULL)
	{
		return -1;
	}

	if (Task->FirstParam != NULL)
	{
		DestroyDataBuffer(Task->FirstParam);
	}

	if (Task->SecondParam != NULL)
	{
		DestroyDataBuffer(Task->SecondParam);
	}

	free(Task);

	return 0;
}

INT EnqueueTask(PTASKS_QUEUE Queue, PTASK Task)
{
	PTASK first = NULL;
	PTASK last = NULL;
	if (Queue == NULL || Task == NULL)
	{
		return -1;
	}

	first = Queue->First;
	last = Queue->Last;

	if (first == NULL && last == NULL)
	{
		first = Task;
		last = Task;
	}
	else
	{
		last->Next = Task;
		last = Task;
	}

	Queue->First = first;
	Queue->Last = last;

	return 0;
}

PTASK DequeueTask(PTASKS_QUEUE Queue)
{
	PTASK first = NULL;
	PTASK last = NULL;
	PTASK task = NULL;

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
		task = first;
		first = NULL;
		last = NULL;
	}
	else
	{
		task = first;
		first = first->Next;
	}

	Queue->First = first;
	Queue->Last = last;

	return task;
}