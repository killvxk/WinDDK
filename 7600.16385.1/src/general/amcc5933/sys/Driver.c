/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Driver.c - Driver for the AMCC S5933 PCI chipset reference kit & & S5933DK1 ISA adapter

Abstract:

    Purpose of the sample to demonstrate:

    1) How one driver can be used for two different type of devices.
    2) How to use a PNP driver for a legacy non PNP ISA device by specifying
        hardware resources in the INF file.
    3) How to do packet based non scatter-gather DMA for a PCI device.

Environment:

    Kernel mode

--*/

#include "common.h"

#include "driver.tmh" // auto-generated by WPP

//
// Make sure the initialization code is removed from memory after use.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, CommonEvtDeviceAdd)
#pragma alloc_text(PAGE, CommonEvtDriverContextCleanup)
#endif


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O system.

Arguments:

    DriverObject - Pointer to the driver object created by the I/O manager.

    RegistryPath - Pointer to the driver specific key
                   \Registry
                      \Machine
                         \System
                            \CurrentControlSet
                               \Services
                                  \<DriverName>

Return Value:

    NTSTATUS

--*/
{

    NTSTATUS               status = STATUS_SUCCESS;
    WDF_DRIVER_CONFIG      config;
    WDFDRIVER              driver;
    WDF_OBJECT_ATTRIBUTES  attributes;

    //
    // Initialize WPP Tracing
    //
    WPP_INIT_TRACING( DriverObject, RegistryPath );

    TraceEvents(TRACE_LEVEL_INFORMATION, AMCC_TRACE_INIT,
                        "DriverObject 0x%p", DriverObject);

    TraceEvents(TRACE_LEVEL_INFORMATION, AMCC_TRACE_INIT,
                        "Built %s %s", __DATE__, __TIME__);
    //
    // Initialize the Driver Config structure..
    //
    WDF_DRIVER_CONFIG_INIT( &config, CommonEvtDeviceAdd );

    //
    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    attributes.EvtCleanupCallback = CommonEvtDriverContextCleanup;

    status = WdfDriverCreate( DriverObject,
                              RegistryPath,
                              &attributes,
                              &config,
                              &driver );

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, AMCC_TRACE_INIT,
                    "WdfDriverCreate failed with status 0x%X\n", status);
        //
        // Cleanup tracing here because DriverContextCleanup will not be called
        // as we have failed to create WDFDRIVER object itself.
        // Please note that if your return failure from DriverEntry after the
        // WDFDRIVER object is created successfully, you don't have to
        // call WPP cleanup because in those cases DriverContextCleanup
        // will be executed when the framework deletes the DriverObject.
        //
        WPP_CLEANUP(DriverObject);
    }

    return status;
}



NTSTATUS
CommonEvtDeviceAdd(
    IN WDFDRIVER        Driver,
    IN PWDFDEVICE_INIT  DeviceInit
    )
/*++

Routine Description:

    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager.  It is responsible for initializing and
    creating a WDFDEVICE object.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS               status = STATUS_SUCCESS;
    WCHAR                  enumeratorName[64] = {0};
    ULONG                  returnSize;
    UNICODE_STRING         unicodeEnumName, temp;
    WDF_OBJECT_ATTRIBUTES  attributesForRequests;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, AMCC_TRACE_INIT,
                "CommonEvtDeviceAdd: Init 0x%p", DeviceInit);

    //
    // Get the device enumerator name to detect whether the device being
    // added is a PCI device or root-enumerated non pnp ISA device.
    // It's okay to WDM functions when there is no appropriate WDF
    // interface is available.
    //
    status = WdfFdoInitQueryProperty(DeviceInit,
                                     DevicePropertyEnumeratorName,
                                     sizeof(enumeratorName),
                                     enumeratorName,
                                     &returnSize);
    if(!NT_SUCCESS(status)){
        return status;
    }

    //
    // Context is used only for the PCI device.
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributesForRequests, REQUEST_CONTEXT);

    WdfDeviceInitSetRequestAttributes(DeviceInit, &attributesForRequests);

    //
    // WdfFdoInitQueryProperty returns a NULL-terminated WCHAR string.
    // So don't worry about buffer overrun.
    //
    RtlInitUnicodeString(&unicodeEnumName, enumeratorName);
    RtlInitUnicodeString(&temp, L"PCI");
    if(RtlCompareUnicodeString(&unicodeEnumName, &temp, TRUE) == 0) {
        //
        // It's a PCI device.
        //
        return AmccPciAddDevice(Driver, DeviceInit);

    }

    //
    // We root-enumerated the non pnp ISA device by manualy installing the
    // driver. So the enumerator should be Root.
    //
    RtlInitUnicodeString(&temp, L"Root");

    if(RtlCompareUnicodeString(&unicodeEnumName, &temp, TRUE) == 0) {
        //
        // It's an non pnp ISA device.
        //
        return AmccIsaEvtDeviceAdd(Driver, DeviceInit);
    }

    ASSERTMSG("Unknown device", FALSE);

    return STATUS_DEVICE_CONFIGURATION_ERROR;
}

VOID
CommonEvtDriverContextCleanup(
    IN WDFDRIVER Driver
    )
/*++
Routine Description:

    Free all the resources allocated in DriverEntry.

Arguments:

    Driver - handle to a WDF Driver object.

Return Value:

    VOID.

--*/
{
    PAGED_CODE ();

    TraceEvents(TRACE_LEVEL_INFORMATION, AMCC_TRACE_SHUTDOWN,
                    "CommonEvtDriverContextCleanup Entered");

    //
    // Stop WPP Tracing
    //
    WPP_CLEANUP( WdfDriverWdmGetDriverObject( Driver ) );

}



