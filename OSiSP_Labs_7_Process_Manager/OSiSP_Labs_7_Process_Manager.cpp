// OSiSP_Labs_7_Process_Manager.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "OSiSP_Labs_7_Process_Manager.h"
#include <iostream>
#include <queue>
#include "ThreadMutex.h"
using namespace std;

ActivateHandlerProc activateHandlerProc;
SC_HANDLE SC_ManagerHandle;
SC_HANDLE SC_ServiceHandle;
HANDLE controlDriver;
bool isAlive=true;
bool isSystemStartService=true;
bool isCreateService=false;
std::queue<HANDLE> controlProcesses;
ThreadMutex mutex;


std::string getProcessNameByHandle(HANDLE hProcess)
{
  if (NULL == hProcess)
    return "<unknown>";

  CHAR szProcessName[MAX_PATH] = "<unknown>";
  HMODULE hMod;
  DWORD cbNeeded;

  if (::EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
    ::GetModuleBaseNameA(hProcess, hMod, szProcessName, sizeof(szProcessName));

  return std::string(szProcessName);
}

std::string getProcessNameByID(DWORD processID)
{
  HANDLE hProcess =OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,false,processID);
  std::string result = getProcessNameByHandle(hProcess);
  CloseHandle(hProcess);

  return result;
}

void initializeActivateHandler()
{
  activateHandlerProc.isActivate=TRUE;
  std::string processName=getProcessNameByID(GetCurrentProcessId());
  strcpy(activateHandlerProc.managerName,processName.c_str());
}

void clearActivateHandler()
{
}

int _tmain(int argc, _TCHAR* argv[])
{
  initializeActivateHandler();
  if (GetProcXAndY())
  {
    wcout<<L"Open file " <<file_Name<<endl;
    initializeManager();
  }
  clearActivateHandler();
  system("PAUSE");
  return 0;
}

bool GetProcXAndY()
{
  wcout<<L"Enter X proc name"<<endl;
  cin>>activateHandlerProc.procName;
  wcout<<L"OK"<<endl;
  
  // open a file name
  OPENFILENAME ofn ;
  ZeroMemory( &ofn , sizeof( ofn));
  ofn.lStructSize = sizeof ( ofn );
  ofn.hwndOwner = NULL  ;
  ofn.lpstrFile = file_Name ;
  ofn.lpstrFile[0] = '\0';
  ofn.nMaxFile = sizeof( file_Name );
  ofn.lpstrFilter = NULL;
  ofn.nFilterIndex =1;
  ofn.lpstrFileTitle = NULL ;
  ofn.nMaxFileTitle = 0 ;
  ofn.lpstrInitialDir=NULL ;
  ofn.Flags = OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST ;
  return GetOpenFileName( &ofn )!=0;
}

void initializeManager()
{
  if (SC_ManagerHandle = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS))
  {
    initializeService();
    CloseServiceHandle(SC_ManagerHandle);
  }
  else
    wcout<< L"Error open SCManager"<< endl;
}

void closeService()
{
  if (!isSystemStartService)
  {
    wcout<<"Stopping service"<<endl;
    SERVICE_STATUS serviceStatus;
    if(!ControlService(SC_ServiceHandle,SERVICE_CONTROL_STOP,&serviceStatus))
      wcout<<"Don't stop service"<<endl;
  }
  if (isCreateService)
    DeleteService(SC_ServiceHandle);
  CloseServiceHandle(SC_ServiceHandle);
}

void initializeService()
{
  if ((SC_ServiceHandle = OpenService(SC_ManagerHandle, DRIVER_NAME,SERVICE_ALL_ACCESS))||
    (SC_ServiceHandle=createService()))
  {
    if (startService())
      syncWithDriver();
    closeService();
  }
  else
  {
    wcout <<L"Error find Service "DRIVER_NAME<<endl;
  }
}

bool startService()
{
  bool isComplete=true;
  SERVICE_STATUS serviceStatus;
  QueryServiceStatus(SC_ServiceHandle,&serviceStatus);
  if (serviceStatus.dwCurrentState!=SERVICE_RUNNING)
  {
    wcout<<"Running service"<<endl;
    if (!StartService(SC_ServiceHandle,NULL,NULL))
    {
      isComplete=false;
      wcout<<"Error start service"<<endl;
    }
    else
      isSystemStartService=false;
  }
  return isComplete;
}

SC_HANDLE createService()
{
  //get driver fileName
  TCHAR fullFileName[FILE_NAME];
  GetModuleFileName(NULL,fullFileName,FILE_NAME);
  TCHAR* lastSlach =wcsrchr(fullFileName, L'\\');
  ZeroMemory(lastSlach?&lastSlach[1]:fullFileName,wcslen(lastSlach?&lastSlach[1]:fullFileName));
  wcscpy(lastSlach?&lastSlach[1]:fullFileName,DRIVER_NAME L".sys");

  isCreateService=true;
  return CreateService(SC_ManagerHandle, DRIVER_NAME, DRIVER_NAME, SERVICE_ALL_ACCESS,SERVICE_KERNEL_DRIVER,SERVICE_DEMAND_START,
    SERVICE_ERROR_NORMAL,	fullFileName, NULL, NULL,	NULL, NULL, NULL);
}

DWORD WINAPI CreateProc(LPVOID state)
{
  HANDLE hEvent = OpenEvent(SYNCHRONIZE, FALSE, SYNC_CREATE_PROC_EVENT);
  do
  {
    DWORD status=WaitForSingleObject(hEvent, 1000);
    if (isAlive&&status==WAIT_OBJECT_0)
    {
      STARTUPINFO startupInfo;
      ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
      startupInfo.cb = sizeof(STARTUPINFO);

      PROCESS_INFORMATION processInformation;
      if (!CreateProcess(file_Name, NULL, NULL, NULL, FALSE, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &startupInfo, &processInformation))
        wcout<<"Error create process "<<file_Name<<" at "<<GetLastError()<<endl;
      else
      {
        mutex.Lock();
        controlProcesses.push(processInformation.hProcess);
        mutex.Unlock();
      }
    }
  }while (isAlive);

  CloseHandle(hEvent);
  return EXIT_SUCCESS;
}

DWORD WINAPI CloseProc(LPVOID state)
{
  HANDLE hEvent = OpenEvent(SYNCHRONIZE, FALSE, SYNC_CLOSE_PROC_EVENT);
  do
  {
    DWORD status=WaitForSingleObject(hEvent, 1000);

    if (isAlive&&status==WAIT_OBJECT_0)
    {
      HANDLE process=INVALID_HANDLE_VALUE;
      mutex.Lock();
      if (controlProcesses.size()>0)
      {
        process=controlProcesses.front();
        controlProcesses.pop();
      }
      mutex.Unlock();
      if (process!=INVALID_HANDLE_VALUE)
      {
        if (!TerminateProcess(process,EXIT_SUCCESS))
          wcout<<"Errors terminated process"<<endl;
        CloseHandle(process);
      }
      else
        wcout<<"invalid handle terminated"<<endl;
    }
  }while (isAlive);

  CloseHandle(hEvent);
  return EXIT_SUCCESS;
}

void waitExit()
{
  HANDLE createThread = CreateThread(0,0,CreateProc,NULL,0,NULL);
  HANDLE closeThread = CreateThread(0,0,CloseProc,NULL,0,NULL);
  do
  {
    WCHAR buf[1024];
    wcout<<"Write exit to close application"<<endl;
    wcin>>buf;
    if (!wcscmp(buf,L"exit"))
      break;
  }while (true);
  isAlive = false;
  if (WaitForSingleObject(createThread,2000)!=WAIT_OBJECT_0)
  {
    wcout<<L"Timeout close createThread, terminated"<<endl;
    TerminateThread(createThread,EXIT_FAILURE);
  }
  if (WaitForSingleObject(closeThread,2000)!=WAIT_OBJECT_0)
  {
    wcout<<L"Timeout close closeThread, terminated"<<endl;
    TerminateThread(closeThread,EXIT_FAILURE);
  }
}

void closeControl()
{
  activateHandlerProc.isActivate=FALSE;
  DWORD bytesReturn;
  if (!DeviceIoControl(controlDriver,IOCTL_DRIVER_CONTROLLER_SWITCH_CONTROL_PROC_STATE,&activateHandlerProc,sizeof(ActivateHandlerProc),0,0,&bytesReturn,NULL))
    wcout<<L"Error close notify proc"<<endl;
}

void syncWithDriver()
{
  controlDriver= CreateFile(L"\\\\.\\"DRIVER_NAME,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
  if (controlDriver!=INVALID_HANDLE_VALUE)
  {
    DWORD bytesReturn;
    if (!DeviceIoControl(controlDriver,IOCTL_DRIVER_CONTROLLER_SWITCH_CONTROL_PROC_STATE,&activateHandlerProc,sizeof(ActivateHandlerProc),0,0,&bytesReturn,NULL))
      wcout<<L"Error set device X proc"<<endl;
    else
    {
      waitExit();
      closeControl();
    }

    CloseHandle(controlDriver);
  }
  else
    wcout<<L"Error CreateFile "DRIVER_NAME<<endl;
}