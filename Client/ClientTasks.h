#pragma once
#include "stdafx.h"

PTASK CreateClientTask(CM_DATA_BUFFER* TaskBuffer, CM_DATA_BUFFER* DataBuffer);

void PrintMessage(CRITICAL_SECTION* WriteSection, CM_DATA_BUFFER* Message);

void PrintMessageFromSource(CRITICAL_SECTION *WriteSection, CM_DATA_BUFFER* FirstArgm, CM_DATA_BUFFER* SecondArgm);

void ReceiveFile(CRITICAL_SECTION* WriteFileSection, PTASK Task);