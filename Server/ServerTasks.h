#pragma once
#include "stdafx.h"

static CM_BYTE* FindNextArgm(CM_BYTE* Buffer, CM_SIZE Start, CM_SIZE* End, CM_SIZE BufferSize);


static int FindCredentialsInUsersFile(SRWLOCK* UsersFile, CM_BYTE* User, CM_BYTE* Password);
/*
Check for users credentials
ret 0 - no one matching
ret 1 - only user matching, or search only after user
ret 2 - user+password matching
*/

PTASK CreateServerTask(CM_DATA_BUFFER* LoggedClientUsername, CM_SERVER_CLIENT* Client, CM_DATA_BUFFER* Command);

void CreateTransferData(PSERVER_SIDE_COMUNICATION ServerSideComunication, PTASK Task);

PTASK CreateMessageTask(CM_SERVER_CLIENT* Client, const char* Message);

PTASK CreateEchoTask(CM_SERVER_CLIENT* Client, CM_BYTE* Message);
PTASK CreateRegisterTask(CM_DATA_BUFFER* LoggedClientUsername, CM_SERVER_CLIENT* Client, CM_BYTE* Username, CM_BYTE* Password);
PTASK CreateLoginTask(CM_DATA_BUFFER* LoggedClientUsername, CM_SERVER_CLIENT* Client, CM_BYTE* Username, CM_BYTE* Password);
PTASK CreateLogoutTask(CM_DATA_BUFFER* LoggedClientUsername, CM_SERVER_CLIENT* Client);
PTASK CreateMsgTask(CM_DATA_BUFFER* LoggedClientUsername, CM_SERVER_CLIENT* Client, CM_BYTE* DestinationUser, CM_BYTE* Message);
PTASK CreateBroadcastTask(CM_DATA_BUFFER* LoggedClientUsername, CM_SERVER_CLIENT* Client, CM_BYTE* Message);
PTASK CreateSendfileTask(CM_DATA_BUFFER* LoggedClientUsername, CM_SERVER_CLIENT* Client, CM_BYTE* DestinationUser, CM_BYTE* Path);

void ExecuteEchoTask(PSERVER_SIDE_COMUNICATION* ServerSideComunications, PTASK Task);
void ExecuteRegisterTask(PSERVER_SIDE_COMUNICATION* ServerSideComunications, SRWLOCK* UsersFile, PTASK Task);
void ExecuteLoginTask(PSERVER_SIDE_COMUNICATION* ServerSideComunications, SRWLOCK* UsersFile, PTASK Task);
void ExecuteLogoutTask(PSERVER_SIDE_COMUNICATION* ServerSideComunications, PTASK Task);
void ExecuteMsgTask(PSERVER_SIDE_COMUNICATION* ServerSideComunications, SRWLOCK* UsersFile, PTASK Task);
void ExecuteBroadcastTask(PSERVER_SIDE_COMUNICATION* ServerSideComunications, PTASK Task);
void ExecuteSendfileTask(PSERVER_SIDE_COMUNICATION* ServerSideComunications, SRWLOCK* UsersFile, PTASK Task);