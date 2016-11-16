#include "stdafx.h"
#include "OSiSP_Labs_7_Driver.h"

PDEVICE_OBJECT g_pDeviceObject;

PDriverVariables GetDeviceVariable()
{
  return (PDriverVariables)g_pDeviceObject->DeviceExtension;
}

PCSTR GetProcessFileNameById(IN HANDLE handle)
{
  PEPROCESS Process;
  PsLookupProcessByProcessId(handle,&Process);

  return gPsGetProcessImageFileName(Process);
}

VOID SwitchControlProcXState()
{
  PDriverVariables driverInformation = GetDeviceVariable();
  DbgPrint("NotifyProc %s is %s\n",driverInformation->activateProc.procName,
    driverInformation->processNotify.isCreate ? "created" : "closed");
  if (driverInformation->processNotify.isCreate)
    driverInformation->syncObjects.setCreateEvent();
  else
    driverInformation->syncObjects.setCloseEvent();
}

BOOLEAN IsParentIdManager()
{
  PDriverVariables driverInformation = GetDeviceVariable();
  PCSTR szProcessName=GetProcessFileNameById(driverInformation->processNotify.hParentID);
  return RtlEqualMemory(szProcessName,driverInformation->activateProc.managerName,strlen(szProcessName));
}

BOOLEAN IsControlProc()
{
  PDriverVariables driverInformation = GetDeviceVariable();
  PCSTR szProcessName=GetProcessFileNameById(driverInformation->processNotify.hProcessID);
  DbgPrint("WorkItem %s.\n",szProcessName);
  return RtlEqualMemory(szProcessName,driverInformation->activateProc.procName,strlen(szProcessName));
}

VOID WorkItem(
  IN PDEVICE_OBJECT  DeviceObject,
  IN OPTIONAL PVOID  Context
  )
{
  if (IsControlProc())
  {
  //protection by recursive call(if X proc equals Y)
    if (!IsParentIdManager())
      SwitchControlProcXState();
    else
      DbgPrint("ParentId is current manager\n");
  }
  IoFreeWorkItem((PIO_WORKITEM)Context);
}

VOID ProcessCallback(
  IN HANDLE  hParentId, 
  IN HANDLE  hProcessId, 
  IN BOOLEAN isCreate
  )
{
  DbgPrint("Process CallBack %s.\n",(isCreate ? "Create" : "Close"));


  //create System Worker Thread to check process
  PDriverVariables driverInformation = GetDeviceVariable();
  driverInformation->processNotify.initProcess(hParentId,hProcessId,isCreate);
  PIO_WORKITEM allocWorkItem=IoAllocateWorkItem(g_pDeviceObject);
  IoQueueWorkItem(allocWorkItem,WorkItem,DelayedWorkQueue,allocWorkItem);
}  

VOID initializeVariable()
{
  PDriverVariables driverInformation = GetDeviceVariable();
  driverInformation->startInitialize();
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath)
{
  //find PsGetProcessImageFileName
  UNICODE_STRING sPsGetProcessImageFileName = RTL_CONSTANT_STRING( L"PsGetProcessImageFileName" ); 
  gPsGetProcessImageFileName = (GET_PROCESS_IMAGE_NAME) MmGetSystemRoutineAddress( &sPsGetProcessImageFileName );
  if (!gPsGetProcessImageFileName)
  {
    DbgPrint("PSGetProcessImageFileName not found\n");
    return STATUS_UNSUCCESSFUL;
  }
  
  UNICODE_STRING DeviceName, Win32Device;
  NTSTATUS status;
  PDEVICE_OBJECT DeviceObject = NULL;
  DbgPrint("Driver Entry\n");
  
  RtlInitUnicodeString(&DeviceName,L"\\Device\\"DRIVER_NAME);
  RtlInitUnicodeString(&Win32Device,L"\\DosDevices\\"DRIVER_NAME);
  
  unsigned i;
  for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
    DriverObject->MajorFunction[i] = DriverUnsupportedHandler;

  DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
  DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
  DriverObject->MajorFunction[IRP_MJ_SHUTDOWN] = DriverShutdown;
  DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDispatchIoctl;
  DriverObject->DriverUnload = DriverUnload;

  status = IoCreateDevice(DriverObject,
    sizeof(DriverVariables),
    &DeviceName,
    FILE_DEVICE_UNKNOWN,
    0,
    FALSE,
    &DeviceObject);
  if (!NT_SUCCESS(status))
    return status;
  if (!DeviceObject)
    return STATUS_UNEXPECTED_IO_ERROR;

  DeviceObject->Flags |= DO_DIRECT_IO;
  DeviceObject->AlignmentRequirement = FILE_WORD_ALIGNMENT;
  status = IoCreateSymbolicLink(&Win32Device, &DeviceName);
  DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
  g_pDeviceObject=DeviceObject;
  initializeVariable();
  return STATUS_SUCCESS;
}

VOID UnloadControlProcHandler()
{
  PDriverVariables driverInformation = GetDeviceVariable();
  
  //if register notify callback, mush unregister
  if (driverInformation->activateProc.isActivate)
    CreateProcessNotify(TRUE);

  //clear driver variables
  driverInformation->finalize();
}

void DriverUnload(IN PDRIVER_OBJECT DriverObject)
{
  DbgPrint("Unload driver\n");
  UnloadControlProcHandler();
  UNICODE_STRING Win32Device;
  RtlInitUnicodeString(&Win32Device,L"\\DosDevices\\"DRIVER_NAME);
  IoDeleteSymbolicLink(&Win32Device);
  IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS DriverCreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
  DbgPrint("Create Close Handler\n");
  Irp->IoStatus.Status = STATUS_SUCCESS;
  Irp->IoStatus.Information = 0;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return STATUS_SUCCESS;
}

NTSTATUS DriverUnsupportedHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
  DbgPrint("Not supported Handler\n");
  Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
  Irp->IoStatus.Information = 0;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return Irp->IoStatus.Status;
}

NTSTATUS DriverShutdown(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
  DbgPrint("Shutdown\n");
  UnloadControlProcHandler();
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return STATUS_SUCCESS;
}

NTSTATUS CreateProcessNotify(IN BOOLEAN isDisabled)
{
  DbgPrint("ProcessNotify %s\n",isDisabled ? "disabled" : "enabled");
  return PsSetCreateProcessNotifyRoutine(ProcessCallback, isDisabled);
}

VOID CopyActivateProc(IN PActivateHandlerProc pActivateInfo)
{
  PDriverVariables driverInformation = GetDeviceVariable();
  RtlCopyMemory(&driverInformation->activateProc,pActivateInfo,sizeof(driverInformation->activateProc));
  DbgPrint("Copy procName %s\n",
    (driverInformation->activateProc.procName==NULL ? "Null" : driverInformation->activateProc.procName));
}

NTSTATUS SwitchMonitoringHandler(IN PIRP Irp)
{
  DbgPrint("Call SwitchMonitor\n");
  NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
  PIO_STACK_LOCATION     irpStack  = IoGetCurrentIrpStackLocation(Irp);

  if (irpStack->Parameters.DeviceIoControl.InputBufferLength == sizeof(ActivateHandlerProc))	
  {
		PActivateHandlerProc pActivateInfo = (PActivateHandlerProc)(Irp->AssociatedIrp.SystemBuffer);
    PDriverVariables driverInformation = GetDeviceVariable();

		if (driverInformation->activateProc.isActivate != pActivateInfo->isActivate)
		{
      CopyActivateProc(pActivateInfo);
      ntStatus = CreateProcessNotify(!pActivateInfo->isActivate);
    }
    else
      DbgPrint("Repeat NotifyProc state\n");
	}
  else
    DbgPrint("Activate Handler not equals size\n");
  return ntStatus;
}

NTSTATUS DriverDispatchIoctl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
  DbgPrint("Dispatch IOctl\n");

  NTSTATUS               ntStatus = STATUS_UNSUCCESSFUL;
  PIO_STACK_LOCATION     irpStack  = IoGetCurrentIrpStackLocation(Irp);

  switch(irpStack->Parameters.DeviceIoControl.IoControlCode)
  {
  case IOCTL_DRIVER_CONTROLLER_SWITCH_CONTROL_PROC_STATE:
    {
      ntStatus = SwitchMonitoringHandler(Irp);
      break;
    }
  }

  Irp->IoStatus.Status = ntStatus;

  Irp->IoStatus.Information= (ntStatus == STATUS_SUCCESS)
    ? irpStack->Parameters.DeviceIoControl.OutputBufferLength
    : 0;

  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return ntStatus;
}