/*
 * Copyright (c) 2000 Apple Computer, Inc.  All rights reserved. 
 *
 * AppleATAPIIX.h - ATA controller driver for Intel PIIX/PIIX3/PIIX4.
 *
 * HISTORY
 *
 */

#ifndef _APPLEATAPIIX_H
#define _APPLEATAPIIX_H

#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOMemoryCursor.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/OSByteOrder.h>

#include "AppleATA.h"
#include "AppleATAPIIXRegs.h"
#include "AppleATAPIIXTiming.h"

class AppleATAPIIX : public AppleATA
{
    OSDeclareDefaultStructors( AppleATAPIIX )
    
protected:
    IOPCIDevice *            provider;     // our provider
    UInt32                   pciCFID;      // our PCI vendor/device ID
    UInt32                   channel;      // IDE channel
    UInt16                   ioCmdRange;   // command block
    UInt16                   ioCtlRange;   // control block
    UInt16                   ioBMRange;    // bus-master register block
    UInt32                   dmaReqLength; // transaction state
    bool                     dmaIsWrite;   // transaction state
    prdEntry_t *             prdTable;     // physical region descriptor table
    IOPhysicalAddress        prdTablePhys; // physical address of prdTable
    PIIXSelectedTimings      timings[2];   // drive0 and drive1 timings
    IOLittleMemoryCursor *   prdCursor;    // request -> scatter-gather list
    IOTimerEventSource *     timerEventSource;
    IOInterruptEventSource * interruptEventSource;

    /*
     * Internal (private) functions.
     */
    bool _getIDERanges(IOPCIDevice * provider);
    bool _getBMRange(IOPCIDevice * provider);
    bool _allocatePRDTable();
    void _deallocatePRDTable();
    bool _resetTimings();
    bool _readPCIConfigSpace(UInt8 * configSpace);
    bool _writePCIConfigSpace(UInt8 * configSpace);
    bool _selectTiming(UInt32        unit,
                       PIIXProtocol  timingProtocol,
                       UInt8 *       pciConfig);

public:
    /*
     * Class initializer.
     */
    static void initialize();

    /*
     * Returns the IDE channel for the current driver instance.
     */
    static int PIIXGetChannel(IOPCIDevice * provider);

    /*
     * Functions defined by our superclass that we must override.
     */
    void writeATAReg(UInt32 regIndex, UInt32 regValue);
    
    UInt32 readATAReg(UInt32 regIndex);

    void free();

    bool configure(IOService * forProvider,
                   UInt32 *    controllerDataSize);

    bool createWorkLoop(IOWorkLoop ** workLoop);

    bool provideProtocols(enum ATAProtocol * protocolsSupported);

    bool provideTimings(UInt32 *    numTimings,
                        ATATiming * timingsSupported);
    
    bool calculateTiming(UInt32 deviceNum, ATATiming * pTiming);

    bool selectTiming(UInt32            unitNum,
                      ATATimingProtocol timingProtocol);

    void disableControllerInterrupts();
    void enableControllerInterrupts();

    void ataTimer(IOTimerEventSource * sender);

    /*
     * Functions that must be implemented by a bus-master controller.
     */
    bool programDma(IOATACommand * cmd);
    bool startDma(IOATACommand * cmd);
    bool stopDma(IOATACommand * cmd, UInt32 * transferCount);

    /*
     * Miscellaneous functions.
     */
    void interruptOccurred();
};

#endif /* !_APPLEATAPIIX_H */
