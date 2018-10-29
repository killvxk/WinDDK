/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.


Module Name:

    evntdrv.c   

Abstract:

    Sample kernel mode trace provider/driver.


--*/
#include <stdio.h>
#include <ntddk.h>
#include "drvioctl.h"


//
// evntdrvEvents.h is generated by MC.exe with the -km option,
// using the manifest evntdrv.htm.
// The file contains a macro per event, and the required code to raise the 
// event.
#include "evntdrvEvents.h"  


DRIVER_UNLOAD EventDrvDriverUnload;

__drv_dispatchType(IRP_MJ_CREATE)
__drv_dispatchType(IRP_MJ_CLOSE)
DRIVER_DISPATCH EventDrvDispatchOpenClose;

__drv_dispatchType(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH EventDrvDispatchDeviceControl;

#define EventDrv_NT_DEVICE_NAME     L"\\Device\\EventEtw"
#define EventDrv_WIN32_DEVICE_NAME  L"\\DosDevices\\EVENTETW"

DRIVER_INITIALIZE DriverEntry;

NTSTATUS
EventDrvDispatchOpenClose(
    IN PDEVICE_OBJECT pDO,
    IN PIRP Irp
    );

NTSTATUS
EventDrvDispatchDeviceControl(
    IN PDEVICE_OBJECT pDO,
    IN PIRP Irp
    );

VOID
EventDrvDriverUnload(
    IN PDRIVER_OBJECT DriverObject
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text( INIT, DriverEntry )
#pragma alloc_text( PAGE, EventDrvDispatchOpenClose )
#pragma alloc_text( PAGE, EventDrvDispatchDeviceControl )
#pragma alloc_text( PAGE, EventDrvDriverUnload )
#endif // ALLOC_PRAGMA


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O system.

Arguments:

    DriverObject - pointer to the driver object

    RegistryPath - pointer to a unicode string representing the path
        to driver-specific key in the registry

Return Value:

   STATUS_SUCCESS if successful
   STATUS_UNSUCCESSFUL  otherwise

--*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    UNICODE_STRING DeviceName;
    UNICODE_STRING LinkName;
    PDEVICE_OBJECT EventDrvDeviceObject;
    WCHAR DeviceNameString[128];
    ULONG LengthToCopy = 128 * sizeof(WCHAR);
    UNREFERENCED_PARAMETER (RegistryPath);

    KdPrint(("EventDrv: DriverEntry\n"));

    //
    // Create Dispatch Entry Points.  
    //
    DriverObject->DriverUnload = EventDrvDriverUnload;
    DriverObject->MajorFunction[ IRP_MJ_CREATE ] = EventDrvDispatchOpenClose;
    DriverObject->MajorFunction[ IRP_MJ_CLOSE ] = EventDrvDispatchOpenClose;
    DriverObject->MajorFunction[ IRP_MJ_DEVICE_CONTROL ] = EventDrvDispatchDeviceControl;    

    RtlInitUnicodeString( &DeviceName, EventDrv_NT_DEVICE_NAME );

    //
    // Create the Device object
    //
    Status = IoCreateDevice(
                           DriverObject,
                           0,
                           &DeviceName,
                           FILE_DEVICE_UNKNOWN,
                           0,
                           FALSE,
                           &EventDrvDeviceObject);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    RtlInitUnicodeString( &LinkName, EventDrv_WIN32_DEVICE_NAME );
    Status = IoCreateSymbolicLink( &LinkName, &DeviceName );

    if ( !NT_SUCCESS( Status )) {
        IoDeleteDevice( EventDrvDeviceObject );
        return Status;
    }


    //
    // Choose a buffering mechanism
    //
    EventDrvDeviceObject->Flags |= DO_BUFFERED_IO;


    //
    // Register with ETW
    //
    EventRegisterSample_Driver();

    //
    // Log an Event with :  DeviceNameLength
    //                      DeviceName
    //                      Status
    //

    // Copy the device name into the WCHAR local buffer in order
    // to place a NULL character at the end, since this field is
    // defined in the manifest as a NULL-terminated string

    if (DeviceName.Length <= 128 * sizeof(WCHAR)) {

        LengthToCopy = DeviceName.Length;

    }

    RtlCopyMemory(DeviceNameString, 
                  DeviceName.Buffer, 
                  LengthToCopy);

    DeviceNameString[LengthToCopy/sizeof(WCHAR)] = L'\0';

    EventWriteStartEvent(NULL, DeviceName.Length, DeviceNameString, Status);


    return STATUS_SUCCESS;
}

NTSTATUS
EventDrvDispatchOpenClose(
    IN PDEVICE_OBJECT pDO,
    IN PIRP Irp
    )
/*++

Routine Description:

   Dispatch routine to handle Create/Close IRPs. 

Arguments:

   DeviceObject - pointer to a device object.

   Irp - pointer to an I/O Request Packet.

Return Value:

   NT status code

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER (pDO);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest( Irp, IO_NO_INCREMENT );
    return STATUS_SUCCESS;
}


NTSTATUS
EventDrvDispatchDeviceControl(
    IN PDEVICE_OBJECT pDO,
    IN PIRP Irp
    )
/*++

Routine Description:

   Dispatch routine to handle IOCTL IRPs.

Arguments:

   DeviceObject - pointer to a device object.

   Irp - pointer to an I/O Request Packet.

Return Value:

   NT Status code

--*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation( Irp );
    ULONG ControlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;
    
    PAGED_CODE();

    UNREFERENCED_PARAMETER (pDO);

    Irp->IoStatus.Information = 
                            irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    switch ( ControlCode ) {
    case IOCTL_EVNTKMP_TRACE_EVENT_A:
    {
        
        EventWriteSampleEventA(NULL);
        
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;

        break;
    }

    default:
        
        //
        // Not one we recognize. Error.
        //

        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER; 
        Irp->IoStatus.Information = 0;

        break;
    }

    //
    // Get rid of this request
    //
    IoCompleteRequest( Irp, IO_NO_INCREMENT );

    return Status;
}


VOID
EventDrvDriverUnload(
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    Free all the resources allocated in DriverEntry.

Arguments:

    DriverObject - pointer to a driver object.

Return Value:

    VOID.

--*/
{
    PDEVICE_OBJECT DevObj;
    UNICODE_STRING LinkName;

    PAGED_CODE();

    KdPrint(("EventDrv: Unloading \n"));

    //
    // Get pointer to Device object
    //    
    DevObj = DriverObject->DeviceObject;

    EventWriteUnloadEvent(NULL, DevObj);
    
    //
    //  Unregister the driver as an ETW provider
    //
    EventUnregisterSample_Driver();


    //
    // Form the Win32 symbolic link name.
    //
    RtlInitUnicodeString( &LinkName, EventDrv_WIN32_DEVICE_NAME );

    //        
    // Remove symbolic link from Object
    // namespace...
    //
    IoDeleteSymbolicLink( &LinkName );

    //
    // Unload the callbacks from the kernel to this driver
    //
    IoDeleteDevice( DevObj );

}



