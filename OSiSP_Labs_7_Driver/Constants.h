#pragma once


#define DRIVER_NAME L"OSiSP_Labs_7_Driver"
#define SYNC_CREATE_PROC_EVENT L"CreateProcEvent"
#define SYNC_CLOSE_PROC_EVENT L"CloseProcEvent"

#define IOCTL_UNKNOWN_BASE FILE_DEVICE_UNKNOWN

#define IOCTL_DRIVER_CONTROLLER_SWITCH_CONTROL_PROC_STATE \
	CTL_CODE(IOCTL_UNKNOWN_BASE, 0x0800, METHOD_BUFFERED, FILE_ANY_ACCESS)


#define FILE_NAME_SIZE 260
typedef struct _ActivateHandlerProc
{
  BOOLEAN isActivate;
  CHAR managerName[FILE_NAME_SIZE];
  CHAR procName[FILE_NAME_SIZE];
}ActivateHandlerProc, *PActivateHandlerProc;