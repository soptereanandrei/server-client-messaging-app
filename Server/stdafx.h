// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <strsafe.h>
#include "communication_api.h"
#include "TasksQueue.h"
#include "ServerSideComunication.h"
#include "ServerTasks.h"

#define MAX_CLIENTS_CONNECTIONS 100
#define WORKER_THREADS 20
#define USERS_FILE_PATH "C:\\registration.txt"

// TODO: reference additional headers your program requires here
