#pragma once

#include "stdafx.h"
#include "Constants.h"

#define FILE_NAME 256
//
//global variables
//

extern SC_HANDLE SC_ManagerHandle;
extern SC_HANDLE SC_ServiceHandle;
extern HANDLE controlDriver;
extern WCHAR file_Name[FILE_NAME] ={0};
extern ActivateHandlerProc activateHandlerProc;
//

void initializeActivateHandler();
void clearActivateHandler();
bool GetProcXAndY();
void initializeManager();
void closeService();
void initializeService();
bool startService();
SC_HANDLE createService();
DWORD WINAPI CreateProc(LPVOID state);
DWORD WINAPI CloseProc(LPVOID state);
void waitExit();
void closeControl();
void syncWithDriver();