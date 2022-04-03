#include "stdafx.h"

#define alphanumeric(C) ( ((C) >= 'a' && (C) <= 'z') || ((C) >= 'A' && (C) <= 'Z') || ((C) >= '0' && (C) <= '9') )
#define upperCase(C) ( (C) >= 'A' && (C) <= 'Z')
/**/
static CM_BYTE* FindNextArgm(CM_BYTE* Buffer, CM_SIZE Start, CM_SIZE* End, CM_SIZE BufferSize)
{
	if (Start >= BufferSize)
	{
		return NULL;
	}

	if (Buffer[Start] == '\0')
	{
		return NULL;
	}

	CM_SIZE i = Start;
	while (Buffer[i] != ' ' && Buffer[i] != '\0' && i < BufferSize)
	{
		i++;
	}
	if (i >= BufferSize)
	{
		i--;
		Buffer[i] = '\0';
	}

	while (Buffer[i] == ' ' && i < BufferSize)
	{
		Buffer[i] = '\0';
		i++;
	}
	if (i >= BufferSize)
	{
		i--;
	}

	*End = i;

	return (Buffer + Start);
}

static int FindCredentialsInUsersFile(SRWLOCK *UsersFile, CM_BYTE *User, CM_BYTE *Password)
{
	HANDLE hUsersFile = INVALID_HANDLE_VALUE;
	int returnCode = 2;

	if (UsersFile == NULL || User == NULL)
	{
		return FALSE;
	}

	AcquireSRWLockShared(UsersFile);
	__try
	{
		hUsersFile = CreateFileA(USERS_FILE_PATH, GENERIC_READ, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hUsersFile == INVALID_HANDLE_VALUE)
		{
			wchar_t buf[256];
			FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				buf, (sizeof(buf) / sizeof(wchar_t)), NULL);
			_tprintf_s(_T("%s\n"), buf);
			__leave;
		}
		BOOL test = FALSE;
		char* buffer = NULL;
		DWORD numberOfBytesRead = 0;

		DWORD fileSize = GetFileSize(hUsersFile, NULL);

		buffer = (char*)calloc(fileSize + 1, sizeof(char));
		if (buffer == NULL)
		{
			CloseHandle(hUsersFile);
			free(buffer);
			__leave;
		}

		test = ReadFile(hUsersFile, buffer, fileSize, &numberOfBytesRead, NULL);
		if (test == FALSE)
		{
			CloseHandle(hUsersFile);
			free(buffer);
			__leave;
		}

		char* username = NULL;
		char* searchStartInBuffer = NULL;
		SIZE_T usernameLenght = strnlen_s((char*)(User), 255);

		BOOL usernameAlreadyExist = FALSE;
		if (User != NULL)
		{
			searchStartInBuffer = buffer;
			while ((username = strstr(searchStartInBuffer, (char*)(User))) != NULL)
			{
				if (username[usernameLenght] == ',')
				{
					usernameAlreadyExist = TRUE;
					break;
				}
				searchStartInBuffer = username + usernameLenght;
			}
		}

		if (usernameAlreadyExist == FALSE)
		{
			CloseHandle(hUsersFile);
			free(buffer);
			returnCode = 0;//find anything
			__leave;
		}

		if (Password == NULL)
		{
			CloseHandle(hUsersFile);
			free(buffer);
			returnCode =  1; //find only user
			__leave;
		}

		char* password = NULL;
		searchStartInBuffer = NULL;

		BOOL passwordAlreadyExist = FALSE;
	
		searchStartInBuffer = username + usernameLenght + 1;
		password = strstr(searchStartInBuffer, (char*)(Password));
		if (password != NULL && password == searchStartInBuffer)
		{
			if (password[strnlen_s((char*)(Password), 255)] == '\n')
			{
				passwordAlreadyExist = TRUE;
			}
		}
	
		if (passwordAlreadyExist == FALSE)
		{
			CloseHandle(hUsersFile);
			free(buffer);
			returnCode = 1;//find only user
			__leave;
		}

		CloseHandle(hUsersFile);
		free(buffer);
	}
	__finally { ReleaseSRWLockShared(UsersFile); }

	return returnCode; //find user+password
}

PTASK CreateServerTask(CM_DATA_BUFFER *LoggedClientUsername, CM_SERVER_CLIENT *Client, CM_DATA_BUFFER *Command)
{
	CM_BYTE* command = NULL;
	CM_BYTE* firstArg = NULL;
	CM_BYTE* secondArg = NULL;

	if (Client == NULL)
	{
		return NULL;
	}

	if (Command == NULL)
	{
		return NULL;
	}
	
	if (Command->DataBuffer == NULL)
	{
		return NULL;
	}

	CM_SIZE end = 0;
	command = FindNextArgm(Command->DataBuffer, 0, &end, Command->UsedBufferSize);

	if (command == NULL)
	{
		return NULL;
	}

	if (strcmp((char*)command, "echo") == 0)
	{
		firstArg = Command->DataBuffer + end;
		secondArg = NULL;
		return CreateEchoTask(Client, firstArg);
	}

	if (strcmp((char*)command, "register") == 0)
	{
		firstArg = FindNextArgm(Command->DataBuffer, end, &end, Command->UsedBufferSize);
		secondArg = FindNextArgm(Command->DataBuffer, end, &end, Command->UsedBufferSize);
		return CreateRegisterTask(LoggedClientUsername, Client, firstArg, secondArg);
	}

	if (strcmp((char*)command, "login") == 0)
	{
		firstArg = FindNextArgm(Command->DataBuffer, end, &end, Command->UsedBufferSize);
		secondArg = FindNextArgm(Command->DataBuffer, end, &end, Command->UsedBufferSize);
		return CreateLoginTask(LoggedClientUsername, Client, firstArg, secondArg);
	}

	if (strcmp((char*)command, "logout") == 0)
	{
		return CreateLogoutTask(LoggedClientUsername, Client);
	}

	if (strcmp((char*)command, "msg") == 0)
	{
		firstArg = FindNextArgm(Command->DataBuffer, end, &end, Command->UsedBufferSize);
		secondArg = FindNextArgm(Command->DataBuffer, end, &end, Command->UsedBufferSize);
		return CreateMsgTask(LoggedClientUsername, Client, firstArg, secondArg);
	}

	if (strcmp((char*)command, "broadcast") == 0)
	{
		firstArg = Command->DataBuffer + end;
		secondArg = NULL;
		return CreateBroadcastTask(LoggedClientUsername, Client, firstArg);
	}

	if (strcmp((char*)command, "sendfile") == 0)
	{
		firstArg = FindNextArgm(Command->DataBuffer, end, &end, Command->UsedBufferSize);
		secondArg = FindNextArgm(Command->DataBuffer, end, &end, Command->UsedBufferSize);
		return CreateSendfileTask(LoggedClientUsername, Client, firstArg, secondArg);
	}

	return CreateMessageTask(Client, "command not found");
}

void CreateTransferData(PSERVER_SIDE_COMUNICATION ServerSideComunication, PTASK Task)
{
	PDATA newData = CreateData();
	if (newData == NULL)
	{
		_tprintf_s(_T("Error: Cannot allocate memory for DATA element!\n"));
		return;
	}
	newData->ClientTask = CreateClientTaskDataBuffer(Task->TaskId, Task->FirstParam);
	if (newData->ClientTask == NULL)
	{
		free(newData);
		return;
	}
	CM_ERROR error = CreateDataBufferByCopy(&(newData->DataBuffer), Task->SecondParam);
	if (CM_IS_ERROR(error))
	{
		_tprintf_s(TEXT("Error: CreateDataBufferByCopy failed with err-code=0x%X!\n"), error);
		DestroyDataBuffer(newData->ClientTask);
		free(newData);
		return;
	}
	ProtectedEnqueueData(ServerSideComunication, newData);
}

PTASK CreateMessageTask(CM_SERVER_CLIENT *Client, const char *Message)
{
	PTASK newTask = NULL;
	CM_DATA_BUFFER* messageBuffer = NULL;
	CM_SIZE messageSize = 0;

	messageSize = (CM_SIZE)(strnlen_s(Message, 255));
	CM_ERROR error = CreateDataBuffer(&messageBuffer, 256);
	if (CM_IS_ERROR(error))
	{
		return NULL;
	}
	
	error = CopyDataIntoBuffer(messageBuffer, (CM_BYTE*)Message, messageSize);
	if (CM_IS_ERROR(error))
	{
		DestroyDataBuffer(messageBuffer);
		return NULL;
	}

	newTask = CreateTask(Client, 0, NULL, NULL);
	if (newTask == NULL)
	{
		DestroyDataBuffer(messageBuffer);
		return NULL;
	}
	newTask->SecondParam = messageBuffer;

	return newTask;
}

PTASK CreateEchoTask(CM_SERVER_CLIENT *Client, CM_BYTE *Message)
{
	PTASK newTask = NULL;
	CM_DATA_BUFFER* message = NULL;

	if (Message == NULL || Client == NULL)
	{
		return NULL;
	}

	CM_ERROR error = CreateDataBuffer(&message, 256);
	if (CM_IS_ERROR(error))
	{
		_tprintf_s(TEXT("CreateDataBuffer failed with err-code=0x%X!\n"), error);;
		return NULL;
	}

	error = CopyDataIntoBuffer(message, Message, (CM_SIZE)(strnlen_s((char*)Message, 255) + 1));
	if (CM_IS_ERROR(error))
	{
		_tprintf_s(TEXT("CopyDataIntoBuffer failed with err-code=0x%X!\n"), error);;
		return NULL;
	}

	newTask = CreateTask(Client, 0, NULL, message);
	if (newTask == NULL)
	{
		DestroyDataBuffer(message);
		return NULL;
	}

	return newTask;
}
 
PTASK CreateRegisterTask(CM_DATA_BUFFER *LoggedClientUsername, CM_SERVER_CLIENT *Client, CM_BYTE *Username, CM_BYTE *Password)
{
	if (Client == NULL || Username == NULL || Password == NULL)
	{
		return CreateMessageTask(Client, "Error: Invalid command");
	}

	if (LoggedClientUsername != NULL)
	{
		return CreateMessageTask(Client, "Error: User already logged in");
	}

	//Check the username
	BOOL okUser = TRUE;
	int i = 0;
	while (Username[i] != '\0')
	{
		if (!alphanumeric(Username[i]))
		{
			okUser = FALSE;
			break;
		}
		i++;
	}

	if (okUser == FALSE)
	{
		return CreateMessageTask(Client, "Error: Invalid username");
	}

	//Check password
	BOOL okPassword = TRUE;
	BOOL upperLetter = FALSE;
	BOOL nonAlphanumeric = FALSE;

	i = 0;
	while (Password[i] != '\0')
	{
		if (Password[i] == ' ' || Password[i] == ',')
		{
			okPassword = FALSE;
			break;
		}
		if (upperCase(Password[i]))
		{
			upperLetter = TRUE;
		}
		if (!alphanumeric(Password[i]))
		{
			nonAlphanumeric = TRUE;
		}
		i++;
	}
	if (okPassword == FALSE)
	{
		return CreateMessageTask(Client, "Error: Invalid password");
	}
	if (upperLetter == FALSE || nonAlphanumeric == FALSE || i < 5)
	{
		return CreateMessageTask(Client, "Error: Password too weak");
	}

	PTASK newTask = NULL;
	CM_DATA_BUFFER* usernameBuffer = NULL;
	CM_DATA_BUFFER* passwordBuffer = NULL;

	CM_ERROR error = CreateDataBuffer(&usernameBuffer, 256);
	if (CM_IS_ERROR(error))
	{
		return NULL;
	}

	error = CopyDataIntoBuffer(usernameBuffer, Username, (CM_SIZE)(strnlen_s((char*)Username, 255) + 1));
	if (CM_IS_ERROR(error))
	{
		DestroyDataBuffer(usernameBuffer);
		return NULL;
	}

	 error = CreateDataBuffer(&passwordBuffer, 256);
	if (CM_IS_ERROR(error))
	{
		DestroyDataBuffer(usernameBuffer);
		return NULL;
	}

	error = CopyDataIntoBuffer(passwordBuffer, Password, (CM_SIZE)(strnlen_s((char*)Password, 255) + 1));
	if (CM_IS_ERROR(error))
	{
		DestroyDataBuffer(usernameBuffer);
		DestroyDataBuffer(passwordBuffer);
		return NULL;
	}

	newTask = CreateTask(Client, 1, NULL, NULL);
	if (newTask == NULL)
	{
		DestroyDataBuffer(usernameBuffer);
		DestroyDataBuffer(passwordBuffer);
		return NULL;
	}

	newTask->FirstParam = usernameBuffer;
	newTask->SecondParam = passwordBuffer;

	return newTask;
}

PTASK CreateLoginTask(CM_DATA_BUFFER* LoggedClientUsername, CM_SERVER_CLIENT* Client, CM_BYTE* Username, CM_BYTE* Password)
{
	if (Client == NULL || Username == NULL || Password == NULL)
	{
		return CreateMessageTask(Client, "Error: Invalid command");
	}

	if (LoggedClientUsername != NULL)
	{
		return CreateMessageTask(Client, "Error: Another user already logged in");
	}

	PTASK newTask = NULL;
	CM_DATA_BUFFER* usernameBuffer = NULL;
	CM_DATA_BUFFER* passwordBuffer = NULL;

	CM_ERROR error = CreateDataBuffer(&usernameBuffer, 256);
	if (CM_IS_ERROR(error))
	{
		return NULL;
	}

	error = CopyDataIntoBuffer(usernameBuffer, Username, (CM_SIZE)(strnlen_s((char*)Username, 255) + 1));
	if (CM_IS_ERROR(error))
	{
		DestroyDataBuffer(usernameBuffer);
		return NULL;
	}

	error = CreateDataBuffer(&passwordBuffer, 256);
	if (CM_IS_ERROR(error))
	{
		DestroyDataBuffer(usernameBuffer);
		return NULL;
	}

	error = CopyDataIntoBuffer(passwordBuffer, Password, (CM_SIZE)(strnlen_s((char*)Password, 255) + 1));
	if (CM_IS_ERROR(error))
	{
		DestroyDataBuffer(usernameBuffer);
		DestroyDataBuffer(passwordBuffer);
		return NULL;
	}

	newTask = CreateTask(Client, 2, NULL, NULL);
	if (newTask == NULL)
	{
		DestroyDataBuffer(usernameBuffer);
		DestroyDataBuffer(passwordBuffer);
		return NULL;
	}

	newTask->FirstParam = usernameBuffer;
	newTask->SecondParam = passwordBuffer;

	return newTask;
}

PTASK CreateLogoutTask(CM_DATA_BUFFER* LoggedClientUsername, CM_SERVER_CLIENT* Client)
{
	if (Client == NULL)
	{
		return CreateMessageTask(Client, "Error: Invalid command");
	}

	if (LoggedClientUsername == NULL)
	{
		return CreateMessageTask(Client, "Error: No user currently logged in");
	}

	PTASK newTask = NULL;

	newTask = CreateTask(Client, 3, NULL, NULL);
	if (newTask == NULL)
	{
		return NULL;
	}

	return newTask;
}

PTASK CreateMsgTask(CM_DATA_BUFFER* LoggedClientUsername, CM_SERVER_CLIENT* Client, CM_BYTE *DestinationUser, CM_BYTE *Message)
{
	if (DestinationUser == NULL || Message == NULL)
	{
		return CreateMessageTask(Client, "Error: Invalid command");
	}

	if (LoggedClientUsername == NULL)
	{
		return CreateMessageTask(Client, "Error: No user currently logged in");
	}

	PTASK newTask = NULL;
	CM_DATA_BUFFER* message = NULL;
	CM_DATA_BUFFER* destinationUsername = NULL;
	char buffer[256];

	CM_ERROR error = CreateDataBuffer(&message, 255);
	if (CM_IS_ERROR(error))
	{
		return NULL;
	}

	error = CreateDataBuffer(&destinationUsername, 255);
	if (CM_IS_ERROR(error))
	{
		DestroyDataBuffer(message);
		return NULL;
	}

	memset(buffer, 0, 256);
	memcpy(buffer, "Message from ", sizeof("Message from "));
	StringCchCatA((STRSAFE_LPSTR)buffer, 256, (STRSAFE_LPSTR)(LoggedClientUsername->DataBuffer));
	StringCchCatA((STRSAFE_LPSTR)buffer, 256, ": ");
	StringCchCatA((STRSAFE_LPSTR)buffer, 256, (STRSAFE_LPSTR)(Message));
	
	error = CopyDataIntoBuffer(message, (CM_BYTE*)buffer, (CM_SIZE)strnlen_s(buffer, 256));
	if (CM_IS_ERROR(error))
	{
		DestroyDataBuffer(message);
		DestroyDataBuffer(destinationUsername);
		return NULL;
	}
	
	error = CopyDataIntoBuffer(destinationUsername, DestinationUser, (CM_SIZE)strnlen_s((char*)DestinationUser, 256));
	if (CM_IS_ERROR(error))
	{
		DestroyDataBuffer(message);
		DestroyDataBuffer(destinationUsername);
		return NULL;
	}

	newTask = CreateTask(Client, 4, NULL, NULL);
	if (newTask == NULL)
	{
		DestroyDataBuffer(message);
		DestroyDataBuffer(destinationUsername);
		return NULL;
	}

	newTask->FirstParam = destinationUsername;
	newTask->SecondParam = message;

	return newTask;
}

PTASK CreateBroadcastTask(CM_DATA_BUFFER* LoggedClientUsername, CM_SERVER_CLIENT* Client, CM_BYTE *Message)
{
	if (Client == NULL || Message == NULL)
	{
		return NULL;
	}

	if (LoggedClientUsername == NULL)
	{
		return CreateMessageTask(Client, "Error: No user currently logged in");
	}

	PTASK newTask = NULL;
	CM_DATA_BUFFER* message = NULL;
	char buffer[256];

	CM_ERROR error = CreateDataBuffer(&message, 255);
	if (CM_IS_ERROR(error))
	{
		return NULL;
	}

	memset(buffer, 0, 256);
	memcpy(buffer, "Broadcast from ", sizeof("Broadcast from "));
	StringCchCatA((STRSAFE_LPSTR)buffer, 256, (STRSAFE_LPSTR)(LoggedClientUsername->DataBuffer));
	StringCchCatA((STRSAFE_LPSTR)buffer, 256, ": ");
	StringCchCatA((STRSAFE_LPSTR)buffer, 256, (STRSAFE_LPSTR)(Message));

	error = CopyDataIntoBuffer(message, (CM_BYTE*)buffer, (CM_SIZE)strnlen_s(buffer, 256));
	if (CM_IS_ERROR(error))
	{
		DestroyDataBuffer(message);
		return NULL;
	}
	newTask = CreateTask(Client, 5, NULL, NULL);
	if (newTask == NULL)
	{
		DestroyDataBuffer(message);
		return NULL;
	}

	newTask->SecondParam = message;

	return newTask;
}

PTASK CreateSendfileTask(CM_DATA_BUFFER* LoggedClientUsername, CM_SERVER_CLIENT* Client, CM_BYTE *DestinationUser, CM_BYTE *Path)
{
	if (Client == NULL || DestinationUser == NULL || Path == NULL)
	{
		return CreateMessageTask(Client, "Error: Invalid command");
	}

	if (LoggedClientUsername == NULL)
	{
		return CreateMessageTask(Client, "Error: No user currently logged in");
	}

	CM_DATA_BUFFER* dest = NULL;
	CM_DATA_BUFFER* path = NULL;

	CM_ERROR error = CreateDataBuffer(&dest, 255);
	if (CM_IS_ERROR(error))
	{
		return NULL;
	}

	error = CreateDataBuffer(&path, 255);
	if (CM_IS_ERROR(error))
	{
		DestroyDataBuffer(dest);
		return NULL;
	}

	error = CopyDataIntoBuffer(dest, DestinationUser, (CM_SIZE)strnlen_s((char*)DestinationUser, 255));
	if (CM_IS_ERROR(error))
	{
		DestroyDataBuffer(dest);
		DestroyDataBuffer(path);
		return NULL;
	}

	error = CopyDataIntoBuffer(dest, DestinationUser, (CM_SIZE)strnlen_s((char*)Path, 255));
	if (CM_IS_ERROR(error))
	{
		DestroyDataBuffer(dest);
		DestroyDataBuffer(path);
		return NULL;
	}

	PTASK newTask = CreateTask(Client, 5, NULL, NULL);
	if (newTask == NULL)
	{
		DestroyDataBuffer(dest);
		DestroyDataBuffer(path);
		return NULL;
	}

	newTask->FirstParam = dest;
	newTask->SecondParam = path;

	return newTask;
}

void ExecuteEchoTask(PSERVER_SIDE_COMUNICATION* ServerSideComunications, PTASK Task)
{
	PSERVER_SIDE_COMUNICATION serverSideComunication = NULL;

	if (ServerSideComunications == NULL || Task == NULL)
	{
		_tprintf_s(_T("Error: Cannot perform the task or the task doesn't exist!\n"));
		return;
	}

	if (Task->Client == NULL || Task->SecondParam == NULL)
	{
		return;
	}

	int i = 0;

	while (ServerSideComunications[i] != NULL)
	{
		serverSideComunication = ServerSideComunications[i];
		if (serverSideComunication->Client == Task->Client)
		{
			CreateTransferData(serverSideComunication, Task);
			return;
		}
		i++;
	}
}

void ExecuteRegisterTask(PSERVER_SIDE_COMUNICATION* ServerSideComunications, SRWLOCK *UsersFile, PTASK Task)
{
	HANDLE hUsersFile = INVALID_HANDLE_VALUE;

	PSERVER_SIDE_COMUNICATION serverSideComunication = NULL;;

	if (ServerSideComunications == NULL || UsersFile == NULL || Task == NULL)
	{
		return;
	}

	if (Task->Client == NULL || Task->FirstParam == NULL || Task->SecondParam == NULL)
	{
		return;
	}

	//Find User's SERVER_SIDE_COMUNICATION structure
	int i = 0;
	while (ServerSideComunications[i] != NULL)
	{
		if (ServerSideComunications[i]->Client == Task->Client)
		{
				serverSideComunication = ServerSideComunications[i];
				break;
		}
		i++;
	}

	if (serverSideComunication == NULL)
	{
		return;
	}

	//Check if the user exist
	if (FindCredentialsInUsersFile(UsersFile, Task->FirstParam->DataBuffer, NULL) == 1)
	{
		PTASK clientTask = CreateMessageTask(Task->Client, "Error: Username already registered");
		CreateTransferData(serverSideComunication, clientTask);
		DestroyTask(clientTask);
		return;
	}

	//Register the new user
	AcquireSRWLockExclusive(UsersFile);
	__try
	{
		hUsersFile = CreateFileA(USERS_FILE_PATH, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hUsersFile == INVALID_HANDLE_VALUE)
		{
			wchar_t buf[256];
			FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				buf, (sizeof(buf) / sizeof(wchar_t)), NULL);
			_tprintf_s(_T("%s\n"), buf);
			__leave;
		}

		char* buffer = NULL;

		buffer = (char*)calloc(256, sizeof(char));
		if (buffer == NULL)
		{
			CloseHandle(hUsersFile);
			free(buffer);
			__leave;
		}

		CM_SIZE usernameLenght = Task->FirstParam->UsedBufferSize - 1;
		CM_SIZE allLenght = usernameLenght + Task->SecondParam->UsedBufferSize - 1;
		DWORD bytesWrite = 0;

		memcpy(buffer, (char*)(Task->FirstParam->DataBuffer), usernameLenght);
		buffer[usernameLenght] = ',';
		allLenght++;

		memcpy(buffer + usernameLenght + 1, (char*)(Task->SecondParam->DataBuffer), Task->SecondParam->UsedBufferSize);
		buffer[allLenght] = '\n';
		allLenght++;

		LARGE_INTEGER position;
		position.QuadPart = 0;
		BOOL test = SetFilePointerEx(hUsersFile, position, NULL, FILE_END);
		if (test == FALSE)
		{
			CloseHandle(hUsersFile);
			free(buffer);
			__leave;
		}

		WriteFile(hUsersFile, buffer, allLenght, &bytesWrite, NULL);
		
		CloseHandle(hUsersFile);
		free(buffer);
	}
	__finally { ReleaseSRWLockExclusive(UsersFile); }

	PTASK clientTask = CreateMessageTask(Task->Client, "Success");
	CreateTransferData(serverSideComunication, clientTask);
	DestroyTask(clientTask);

	return;
}

void ExecuteLoginTask(PSERVER_SIDE_COMUNICATION* ServerSideComunications, SRWLOCK* UsersFile, PTASK Task)
{
	PSERVER_SIDE_COMUNICATION serverSideComunication = NULL;;

	if (ServerSideComunications == NULL || UsersFile == NULL || Task == NULL)
	{
		return;
	}

	if (Task->Client == NULL || Task->FirstParam == NULL || Task->SecondParam == NULL)
	{
		return;
	}

	//Find User's SERVER_SIDE_COMUNICATION structure
	int i = 0;
	BOOL alreadyConnected = FALSE;
	while (ServerSideComunications[i] != NULL)
	{
		if (ServerSideComunications[i]->Client == Task->Client)
		{
			serverSideComunication = ServerSideComunications[i];
		}
		if (ServerSideComunications[i]->Username != NULL)
		{
			if (ServerSideComunications[i]->Username->UsedBufferSize == Task->FirstParam->UsedBufferSize)
			{
				if (strcmp((char*)(ServerSideComunications[i]->Username->DataBuffer), (char*)(Task->FirstParam->DataBuffer)) == 0)
				{
					alreadyConnected = TRUE;
				}
			}
		}
		i++;
	}

	if (serverSideComunication == NULL)
	{
		return;
	}

	if (alreadyConnected == TRUE)
	{
		PTASK clientTask = CreateMessageTask(Task->Client, "Error: User already logged in");
		CreateTransferData(serverSideComunication, clientTask);
		DestroyTask(clientTask);
		return;
	}

	if (FindCredentialsInUsersFile(UsersFile, Task->FirstParam->DataBuffer, Task->SecondParam->DataBuffer) != 2)
	{
		PTASK clientTask = CreateMessageTask(Task->Client, "Error: Invalid username/password combination");
		CreateTransferData(serverSideComunication, clientTask);
		DestroyTask(clientTask);
	}
	
	CM_DATA_BUFFER* usernameBuffer = NULL;
	CM_ERROR error = CreateDataBufferByCopy(&usernameBuffer, Task->FirstParam);
	if (CM_IS_ERROR(error))
	{
		return;
	}

	serverSideComunication->Username = usernameBuffer;

	PTASK clientTask = CreateMessageTask(Task->Client, "Success");
	CreateTransferData(serverSideComunication, clientTask);
	DestroyTask(clientTask);

	
}

void ExecuteLogoutTask(PSERVER_SIDE_COMUNICATION* ServerSideComunications, PTASK Task)
{
	PSERVER_SIDE_COMUNICATION serverSideComunication = NULL;

	if (ServerSideComunications == NULL || Task == NULL)
	{
		return;
	}

	if (Task->Client == NULL)
	{
		return;
	}

	int i = 0;
	while (ServerSideComunications[i] != NULL)
	{
		serverSideComunication = ServerSideComunications[i];
		if (serverSideComunication->Client == Task->Client)
		{
			if (serverSideComunication->Username != NULL)
			{
				DestroyDataBuffer(serverSideComunication->Username);
			}
			serverSideComunication->Username = NULL;
			PTASK clientTask = CreateMessageTask(Task->Client, "Success");
			CreateTransferData(serverSideComunication, clientTask);
			DestroyTask(clientTask);
			return;
		}
		i++;
	}

	return;
}

void ExecuteMsgTask(PSERVER_SIDE_COMUNICATION* ServerSideComunications, SRWLOCK* UsersFile, PTASK Task)
{
	PSERVER_SIDE_COMUNICATION thisClient = NULL;
	PSERVER_SIDE_COMUNICATION destination = NULL;

	if (ServerSideComunications == NULL || Task == NULL)
	{
		return;
	}

	if (Task->Client == NULL || Task->FirstParam == NULL || Task->SecondParam == NULL)
	{
		return;
	}

	int i = 0;
	while (ServerSideComunications[i] != NULL)
	{
		if (ServerSideComunications[i]->Client == Task->Client)
		{
			thisClient = ServerSideComunications[i];
		}
		if (ServerSideComunications[i]->Username != NULL)
		{
			//if (ServerSideComunications[i]->Username->UsedBufferSize == Task->FirstParam->UsedBufferSize)
			//{
				if (strcmp((char*)(ServerSideComunications[i]->Username->DataBuffer), (char*)Task->FirstParam->DataBuffer) == 0)
				{
					destination = ServerSideComunications[i];
				}
			//}
		}
		i++;
	}

	if (thisClient == NULL)
	{
		return;
	}

	if (destination == NULL)
	{
		if (FindCredentialsInUsersFile(UsersFile, Task->FirstParam->DataBuffer, NULL) == 1)
		{
			PTASK clientTask = CreateMessageTask(thisClient->Client, "Error: No user active");
			CreateTransferData(thisClient, clientTask);
			DestroyTask(clientTask);
		}
		else
		{
			PTASK clientTask = CreateMessageTask(thisClient->Client, "Error: No such user");
			CreateTransferData(thisClient, clientTask);
			DestroyTask(clientTask);
		}
		return;
	}

	PTASK clientTask = CreateMessageTask(NULL, (char*)Task->SecondParam->DataBuffer);
	CreateTransferData(destination, clientTask);
	DestroyTask(clientTask);
}

void ExecuteBroadcastTask(PSERVER_SIDE_COMUNICATION* ServerSideComunications, PTASK Task)
{
	PSERVER_SIDE_COMUNICATION serverSideComunication = NULL;


	if (ServerSideComunications == NULL || Task == NULL)
	{
		_tprintf_s(_T("Error: Cannot perform the task or the task doesn't exist!\n"));
		return;
	}
	
	int i = 0;


	while (ServerSideComunications[i] != NULL)
	{
		serverSideComunication = ServerSideComunications[i];
		//check if the Thread is connected to a client
		if (serverSideComunication->Client == Task->Client)
		{
			PTASK clientTask = CreateMessageTask(NULL, "Success");
			CreateTransferData(serverSideComunication, clientTask);
			DestroyTask(clientTask);
		}
		else if (serverSideComunication->Username != NULL)
		{
			CreateTransferData(serverSideComunication, Task);
		}
		i++;
	}
}

void ExecuteSendfileTask(PSERVER_SIDE_COMUNICATION* ServerSideComunications, SRWLOCK *UsersFile, PTASK Task)
{
	PSERVER_SIDE_COMUNICATION destStruct = NULL;
	PSERVER_SIDE_COMUNICATION sourceStruct = NULL;
	HANDLE file = INVALID_HANDLE_VALUE;

	if (ServerSideComunications == NULL || Task == NULL)
	{
		return;
	}

	int i = 0;
	while (ServerSideComunications[i] != NULL)
	{
		if (ServerSideComunications[i]->Client == Task->Client)
		{
			sourceStruct = ServerSideComunications[i];
		}
		if (ServerSideComunications[i]->Username == Task->FirstParam)
		{
			destStruct = ServerSideComunications[i];
		}
		i++;
	}

	if (FindCredentialsInUsersFile(UsersFile, Task->FirstParam->DataBuffer, NULL) < 1)
	{
		PTASK clientTask = CreateMessageTask(NULL, "Error: No such user");
		CreateTransferData(sourceStruct, clientTask);
		DestroyTask(clientTask);
	}

	if (sourceStruct == NULL)
	{
		return;
	}

	if (destStruct == NULL)
	{
		PTASK clientTask = CreateMessageTask(NULL, "Error: User not active");
		CreateTransferData(sourceStruct, clientTask);
		DestroyTask(clientTask);
	}

	__try
	{
		file = CreateFileA((LPCSTR)Task->FirstParam->DataBuffer, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE)
		{
			PTASK clientTask = CreateMessageTask(NULL, "Error: File not found");
			CreateTransferData(sourceStruct, clientTask);
			DestroyTask(clientTask);
			__leave;
		}

		char buffer[2050];
		DWORD receivedBytes = 1;

		while (ReadFile(file, buffer, 2048, &receivedBytes, NULL) && receivedBytes != 0)
		{
			CM_DATA_BUFFER* sendBuf = NULL;
			CM_ERROR error = CreateDataBuffer(&sendBuf, 2048);
			if (CM_IS_ERROR(error))
			{
				break;
			}

			error = CopyDataIntoBuffer(sendBuf, (CM_BYTE*)buffer, receivedBytes);
			if (CM_IS_ERROR(error))
			{
				DestroyDataBuffer(sendBuf);
				break;
			}
			PTASK newClientTask = CreateTask(NULL, 1, Task->SecondParam, NULL);
			if (newClientTask == NULL)
			{
				DestroyDataBuffer(sendBuf);
				break;
			}
			newClientTask->SecondParam = sendBuf;
			CreateTransferData(destStruct, newClientTask);
			DestroyTask(newClientTask);
		}

		PTASK clientTask = CreateMessageTask(NULL, "Receive file finished");
		CreateTransferData(sourceStruct, clientTask);
		DestroyTask(clientTask);
		__leave;
	}
	__finally { return; }
	
}