/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * Implementation of the interface for the keylargo I2C interface
 *
 * HISTORY
 *
 */

#include "PPCI2CInterface.h"

// Uncomment the following define if you wish ro see more logging
// on the i2c interface:
// #define DEBUGMODE 

// Private Methods:
// ===============

// FIXME: change the if 0 into if 1 in the following line for final
// builds so all the simple private functions can be inlined.
#if 1
#define INLINE inline
#else
#define INLINE
#endif

// --------------------------------------------------------------------------
// Method: dumpI2CRegisters
//
// Purpose:
//        Given the base of the i2c registers inits all the registers
INLINE void
PPCI2CInterface::dumpI2CRegisters()
{
#ifdef DEBUGMODE
    IOLog("mode    0x%08lx -> 0x%02x\n", (UInt32)mode,*mode);
    IOLog("control 0x%08lx -> 0x%02x\n", (UInt32)control,*control);
    IOLog("status  0x%08lx -> 0x%02x\n", (UInt32)status,*status);
    IOLog("ISR     0x%08lx -> 0x%02x\n", (UInt32)ISR,*ISR);
    IOLog("IER     0x%08lx -> 0x%02x\n", (UInt32)IER,*IER);
    IOLog("address 0x%08lx -> 0x%02x\n", (UInt32)address,*address);
    IOLog("subAddr 0x%08lx -> 0x%02x\n", (UInt32)subAddr,*subAddr);
    //IOLog("data    0x%08lx -> 0x%02x\n", (UInt32)data,*data);
#endif //DEBUGMODE
}

// --------------------------------------------------------------------------
// Method: SetI2CBase
//
// Purpose:
//        Given the base of the i2c registers inits all the registers
INLINE void
PPCI2CInterface::SetI2CBase(UInt8 *baseAddress, UInt8 steps)
{
    I2CRegister base = (I2CRegister)baseAddress;

    if (base != NULL) {
        mode = base;						     				// Configure the transmission mode of the i2c cell and the databit rate.
        control = mode + steps;			 // Holds the 4 bits used to start the operations on the i2c interface.
        status = control + steps;		// Status bits for the i2 cell and the i2c interface.
        ISR = status + steps;						// Holds the status bits for the interrupt conditions.
        IER = ISR + steps;					 	  // Eneables the bits that allow the four interrupt status conditions.
        address = IER + steps;			  // Holds the 7 bits address and the R/W bit.
        subAddr = address + steps; // the 8bit subaddress..
        data = subAddr + steps; 			// from where we read and write data

        // Clears interrupt and status register:
        *status = 0x00;
        *ISR = 0x00;

        // And remembers the current mode:
        getMode();

#ifdef DEBUGMODE
        IOLog("PPCI2CInterface::SetI2CBase(0x%08lx,0x%02x)\n",(UInt32)baseAddress, steps);
        dumpI2CRegisters();
#endif // DEBUGMODE
    }
}

// --------------------------------------------------------------------------
// Method: shiftedMask
//
// Purpose:
//        Returns the mask to use with the register:
INLINE UInt8
PPCI2CInterface::shiftedMask(UInt8 mask, UInt8 shift)
{
    return (mask << shift);
}

// --------------------------------------------------------------------------
// Method: shiftedCompMask
//
// Purpose:
//        Returns the complement of the mask
INLINE UInt8
PPCI2CInterface::shiftedCompMask(UInt8 mask, UInt8 shift)
{
    return ~shiftedMask(mask, shift);
}

// --------------------------------------------------------------------------
// Method: writeRegisterField
//
// Purpose:
//        writes a field of a regiter with the given data. The arguments are
//        the register, the mask for the field, shift to get to the right bits
//        and the new data.
INLINE void
PPCI2CInterface::writeRegisterField(I2CRegister reg, UInt8 mask, UInt8 shift, UInt8 newData)
{
    UInt8 registerValue = *reg;
    UInt8 data = (newData & mask);
    UInt8 nMask = shiftedCompMask(mask, shift);

    *reg = (registerValue & nMask) | (data << shift);
    eieio();
 
#ifdef DEBUGMODE
/*    IOLog("PPCI2CInterface::writeRegisterField(0x%08lx, 0x%02x, 0x%02x, 0x%02x)\n",(UInt32)reg, mask, shift, newData);
    IOLog("PPCI2CInterface::writeRegisterField() registerValue=0x%02x\n", registerValue);
    IOLog("PPCI2CInterface::writeRegisterField() data=0x%02x\n", data);
    IOLog("PPCI2CInterface::writeRegisterField() nMask=0x%02x\n", nMask);
    IOLog("PPCI2CInterface::writeRegisterField() *reg=0x%02x\n", *reg);*/
#endif // DEBUGMODE
}

// --------------------------------------------------------------------------
// Method: readRegisterField
//
// Purpose:
//        writes a field of a regiter with the given data. The arguments are
//        the register, the mask for the field and shift to get to the right
//        bits.
INLINE UInt8
PPCI2CInterface::readRegisterField(I2CRegister reg, UInt8 mask, UInt8 shift)
{
    UInt8 registerValue = *reg;
    UInt8 returnValue = ((registerValue) >> shift) & mask;

#ifdef DEBUGMODE
/*    IOLog("PPCI2CInterface::readRegisterField(0x%08lx, 0x%02x, 0x%02x)\n",(UInt32)reg, mask, shift);
    IOLog("PPCI2CInterface::readRegisterField() *reg=0x%02x\n", *reg);
    IOLog("PPCI2CInterface::readRegisterField() returnValue=0x%02x\n", returnValue);*/
#endif // DEBUGMODE

    return returnValue;
}

// Intermediate methods to access to each field of all the registers:
// ------------------------------------------------------------------

// Mode register
INLINE void
PPCI2CInterface::setMode(I2CMode newMode)
{
    // remoeber the last mode:
    lastMode = newMode;
    writeRegisterField(mode, (UInt8)kModeMask, (UInt8)I2CModeShift, (UInt8)newMode);
}

INLINE PPCI2CInterface::I2CMode
PPCI2CInterface::getMode()
{
    lastMode = (I2CMode)readRegisterField(mode, (UInt8)kModeMask, (UInt8)I2CModeShift);
    return lastMode;
}

INLINE void
PPCI2CInterface::setSpeed(I2CSpeed newSpeed)
{
    writeRegisterField(mode, (UInt8)kSpeedMask, (UInt8)I2CSpeedShift, (UInt8)newSpeed);
}

INLINE PPCI2CInterface::I2CSpeed
PPCI2CInterface::getSpeed()
{
    return (I2CSpeed)readRegisterField(mode, (UInt8)kSpeedMask, (UInt8)I2CSpeedShift);
}

// Control register
INLINE void
PPCI2CInterface::setControl(I2CControl newControlValue)
{
    *control = (UInt8)newControlValue;
    eieio();
}

INLINE PPCI2CInterface::I2CControl
PPCI2CInterface::getControl()
{
    return (I2CControl)(*control);
}

// Status register
INLINE void
PPCI2CInterface::setStatus(I2CStatus newStatusValue)
{
    *status = (UInt8)newStatusValue;
    eieio();
}

INLINE PPCI2CInterface::I2CStatus
PPCI2CInterface::getStatus()
{
    return (I2CStatus)(*status);
}

// Interrupt status
INLINE void
PPCI2CInterface::setInterruptStatus(I2CInterruptStatus newStatusValue)
{
    *ISR = (UInt8)newStatusValue;
    eieio();
}

INLINE PPCI2CInterface::I2CInterruptStatus
PPCI2CInterface::getInterruptStatus()
{
    return (I2CInterruptStatus)(*ISR);
}

// Interrupt enable
INLINE void
PPCI2CInterface::setInterruptEnable(I2CInterruptEnable newInterruptEnable)
{
    *IER = (UInt8)newInterruptEnable;
    eieio();
}

INLINE PPCI2CInterface::I2CInterruptEnable
PPCI2CInterface::setInterruptEnable()
{
    return (I2CInterruptEnable)(*IER);
}

// Address Register:
INLINE void
PPCI2CInterface::setAddress(UInt8 newAddress)
{
    writeRegisterField(address, (UInt8)kADDRMask, (UInt8)I2CAddressShift, (UInt8)newAddress);
    eieio();
}

INLINE void
PPCI2CInterface::setAddressRegister(UInt8 newAddress, I2CRWMode readMode)
{
    newAddress &= kADDRMask;
    *address = (newAddress << I2CAddressShift) | readMode;
    eieio();
}

INLINE UInt8
PPCI2CInterface::getAddress()
{
    return readRegisterField(address, (UInt8)kADDRMask, (UInt8)I2CAddressShift);
}

INLINE void
PPCI2CInterface::setReadWrite(I2CRWMode readMode)
{
    writeRegisterField(address, (UInt8)kRWMask, (UInt8)I2CRWShift, (UInt8)readMode);
    eieio();
}

INLINE PPCI2CInterface::I2CRWMode
PPCI2CInterface::getReadWrite()
{
    return (I2CRWMode)readRegisterField(address, (UInt8)kRWMask, (UInt8)I2CRWShift);
}

// SubAddress register
INLINE void
PPCI2CInterface::setSubAddress(UInt8 newSubAddress)
{
    *subAddr = newSubAddress;
    eieio();
}

INLINE UInt8
PPCI2CInterface::getSubAddress()
{
    return (*subAddr);
}

// Data register
INLINE void
PPCI2CInterface::setData(UInt8 newByte)
{
    *data = newByte;
    eieio();
}

INLINE UInt8
PPCI2CInterface::getData()
{
    return (*data);
}

// --------------------------------------------------------------------------
// Method: setAddressAndDirection
//
// Purpose:
//        sets the control register with the correct address and data direction.
INLINE bool
PPCI2CInterface::setAddressAndDirection()
{
    // sets the address register:
    if (isReading) {
        setAddressRegister(currentAddress, kReadADDR);        
    }
    else {
        setAddressRegister(currentAddress, kWriteADDR);       
    }

    // sets the subAddress:
    setSubAddress(currentSubaddress);

    currentState = ki2cStateWaitingForIADDR;
    setControl((I2CControl)(getControl() | kXAddrCNTRL));

#ifdef DEBUGMODE
    IOLog("PPCI2CInterface::setAddressAndDirection()\n");
#endif // DEBUGMODE

    return(true);
}

// --------------------------------------------------------------------------
// Method: i2cStandardSubModeInterrupts
//
// Purpose:
//        handles the interrupts and the state machine for the Standard + SubAddress mode:
bool
PPCI2CInterface::i2cStandardSubModeInterrupts(UInt8 interruptStatus)
{
    bool success = true;

    switch (currentState) {
        case ki2cStateWaitingForIADDR:
#ifdef DEBUGMODE            
            IOLog("ki2cStateWaitingForIADDR: ");
#endif // DEBUGMODE            

            // verify that correct interrupt bit set
            if ((interruptStatus & kIAddrISR) == 0) {
#ifdef DEBUGMODE            
                IOLog("PPCI2CInterface::i2cStandardSubModeInterrupts (ki2cStateWaitingForIADDR): bad state ");
#endif
                success = false;
            }

            if (success) {
                if (getStatus() & kLastAakSTATUS) {
                        if (isReading) {
                            // if multiple bytes are to be read, set AAK bit
                            if (nBytes > 1)
                                setControl((I2CControl)(getControl() | kAakCNTRL));
                        }
                        else {
                            // write operation
                            setData(*dataBuffer++);
                            nBytes--;
                        }

                        currentState = ki2cStateWaitingForIDATA;
                    }
                    else {
                        // no slave responded, so wait for STOP (control bit does not need to be set,
                        // since NAAK sets the control bit automatically)
#ifdef DEBUGMODE            
                        IOLog("...no slave...");
#endif
                        currentState = ki2cStateWaitingForISTOP;
                        success = false;
                    }
                }
                else {
                    // bad state, so abort command
                    abortTransfer();
                }

                // clear IADDR bit and go
                setInterruptStatus(kIAddrISR);
            break;

        case ki2cStateWaitingForIDATA:
#ifdef DEBUGMODE            
            IOLog("ki2cStateWaitingForIDATA: ");
#endif
            // verify that correct interrupt bit set
            if ((interruptStatus & kIDataISR) == 0) {
#ifdef DEBUGMODE            
                IOLog("PPCI2CInterface::i2cStandardSubModeInterrupts (ki2cStateWaitingForIDATA): bad state ");
#endif
                success = false;
            }

                if (success) {
                    if (isReading) {
                        *dataBuffer++ = getData();
                        nBytes--;

                        if (nBytes == 0)			// read operation completed
                            currentState = ki2cStateWaitingForISTOP;
                        else 					// if next byte is last byte, clear AAK bit, and state remains unchanged
                            setControl(kClrCNTRL);
                    }
                    else {
                        // write operation
                        if (getStatus() & kLastAakSTATUS) {
                            if (nBytes == 0) {
                                // write operation completed
                                currentState = ki2cStateWaitingForISTOP;
                                setControl((I2CControl)(getControl() | kStopCNTRL));
                            }
                            else {
                                // send an other byte
                                setData(*dataBuffer++);
                                nBytes--;
                            }
                        }
                       else {
                           // no slave responded, so wait for STOP (control bit does not need to be set,
                           // since NAAK sets the control bit automatically)
#ifdef DEBUGMODE            
                           IOLog("...no slave...");
#endif
                           currentState = ki2cStateWaitingForISTOP;
                           success = false;
                        }
                    }
                }
                else {
                    // bad state, so abort command
                    abortTransfer();
                }

                // clear IDATA bit and go
                setInterruptStatus(kIDataISR);
            break;

        case ki2cStateWaitingForISTOP:
#ifdef DEBUGMODE
            IOLog("ki2cStateWaitingForISTOP: ");
#endif
            // verify that correct interrupt bit set
            if ((interruptStatus & kIStopISR) == 0) {
#ifdef DEBUGMODE
                IOLog("PPCI2CInterface::i2cStandardSubModeInterrupts (ki2cStateWaitingForISTOP): bad state ");
#endif
                success = false;
            }

            currentState = ki2cStateIdle;
            setInterruptStatus(kIStopISR);
           break;

        case ki2cStateWaitingForISTART:
#ifdef DEBUGMODE
            IOLog("ki2cStateWaitingForISTART: is not supposed to happen in this state machine ");
#endif
            break;

        case ki2cStateIdle:
#ifdef DEBUGMODE
            IOLog("ki2cStateIdle: ");
#endif
            break;

        default:
             break;
    }
#ifdef DEBUGMODE
    IOLog("\n");
#endif

    // tell the system that this interrupt has been serviced
    return success;
}

// --------------------------------------------------------------------------
// Method: abortTransfer
//
// Purpose:
//        ends the data transfer.
INLINE bool
PPCI2CInterface::abortTransfer()
{
    currentState = ki2cStateWaitingForISTOP;
    setControl((I2CControl)(getControl() | kStopCNTRL));
    return false;
}

// --------------------------------------------------------------------------
// Method: waitForCompletion
//
// Purpose:
//        waits until the last command was executed correctly and when it
//        happens it calls the interrupt handler. This is useful for when
//        we use this object without an interrupt handler.
bool
PPCI2CInterface::waitForCompletion()
{
    UInt16 loop = 50 + nBytes * 10;
    UInt8 intStat = getInterruptStatus();
    
    // First call to the interrupt handler in case the last command
    // is already completed
    
    //IOLog("1] Interrupt Status = 0x%02x\n", intStat);
    if (intStat & kISRMask)
        handleI2CInterrupt();

    // makes 10 loops for each expected interrupt with 1 millisecond delay for each loop:
    while((loop--) && (currentState != ki2cStateIdle)) {
        IOSleep(1);

        // If an interrupt occured handle it:
        intStat = getInterruptStatus();
        
        //IOLog("%d] Interrupt Status = 0x%02x\n", intStat);
        if (intStat & kISRMask)
            handleI2CInterrupt();
    }

    if (currentState != ki2cStateIdle) {
#ifdef DEBUGMODE
        IOLog("PPCI2CInterface::waitForCompletion error loop is incomplete\n");
#endif
        return false;
    }
    else {
#ifdef DEBUGMODE
        IOLog("PPCI2CInterface::waitForCompletion loop is complete\n");
#endif
        return true;        
    }
}

// Public Methods:
// ===============

// Initialize the address of the registers and the registers themselfs:
bool
PPCI2CInterface::initI2CBus(UInt8 *baseAddress, UInt8 steps)
{
    pollingMode = true;
    currentState = ki2cStateIdle;
    SetI2CBase(baseAddress, steps);

    return true;
}

// The default behavior of this class is to work in "polling mode"
// where the interrupt handler is called by a polling routine.
// However it is always possible to attahc the interrupt handler
// to an hardware interrupt and handle the transfer in the proper
// way. (the polling mode was created for the sole pourpose to
// debug the state machine).
void
PPCI2CInterface::setPollingMode(bool mode)
{
    pollingMode = mode; 
}

// These are to setup the mode for the I2C bus:
void
PPCI2CInterface::setDumbMode()
{
    setMode(kDumbMode);
}

void
PPCI2CInterface::setStandardMode()
{
    setMode(kStandardMode);
}

void
PPCI2CInterface::setStandardSubMode()
{
    setMode(kStandardSubMode);
}

void
PPCI2CInterface::setCombinedMode()
{
    setMode(kCombinedMode);
}

// These instead set the speed:
bool
PPCI2CInterface::setKhzSpeed(UInt speed)
{
    switch (speed)
    {
        case 100:
            setSpeed(k100KhzMode);
            break;

        case 50:
            setSpeed(k50KhzMode);
            break;

        case 25:
            setSpeed(k25KhzMode);
            break;

        default:
#ifdef DEBUGMODE
            IOLog("PPCI2CInterface::setKhzSpeed Can not set bus speed %d is not allowed\n", speed);
#endif
            return false;
            break;
    }

    return true;
}

// Test to read the values set by the funtions above:
bool
PPCI2CInterface::isInDumbMode()
{
    return (getMode() == kDumbMode);
}

bool
PPCI2CInterface::isInStandardMode()
{
    return (getMode() == kStandardMode);
}

bool
PPCI2CInterface::isInStandardSubMode()
{
    return (getMode() == kStandardSubMode);
}

bool
PPCI2CInterface::isInCombinedMode()
{
    return (getMode() == kCombinedMode);
}

// These instead returns the speed:
UInt
PPCI2CInterface::getKhzSpeed()
{
    switch((int)getSpeed())
    {
        case k100KhzMode:
            return (100);
            break;

        case k50KhzMode:
            return (50);
            break;

        case k25KhzMode:
            return (25);
            break;
    }

    // Returns 0 since the speed was not set to a real value
    return (0);
}

// Starts the use of the interface:
bool
PPCI2CInterface::openI2CBus(UInt8 port)
{
    portSelect = port;

    // Enable all the interrupts:
    setInterruptEnable((I2CInterruptEnable)(kEDataIER | kEAddrIER | kEStopIER | kEStartIER));

    return true;
}

// Writes a block of data at a given address:
bool
PPCI2CInterface::writeI2CBus(UInt8 address, UInt8 subAddress, UInt8 *newData, UInt16 len)
{
    bool success = true;

    // waits for the completion of the previous command
    waitForCompletion();

    // pointer to the data to be transfered
    dataBuffer = newData;

    // and the number of bytes still to transfer
    nBytes = len;

    // the current transfer address:
    currentAddress = address;

    // the current transfer subAddress:
    currentSubaddress = subAddress;

    // the data direction:
    isReading = false;

    if ((success = setAddressAndDirection()) == true ) {
        if (pollingMode)
            success = waitForCompletion();
    }

    return success;
}

// Reads a block of data at a given address:
bool
PPCI2CInterface::readI2CBus(UInt8 address, UInt8 subAddress, UInt8 *newData, UInt16 len)
{
    bool success = false;

    // waits for the completion of the previous command
    waitForCompletion();

    // pointer to the data to be received
    dataBuffer = data;

    // and the number of bytes still to receive
    nBytes = len;

    // the current transfer address:
    currentAddress = address;

    // the current transfer subAddress:
    currentSubaddress = subAddress;

    // the data direction:
    isReading = true;

    if ((success = setAddressAndDirection()) == true ) {
        if (pollingMode)
            success = waitForCompletion();
    }
 
    return success;
}

// End using the interface:
bool
PPCI2CInterface::closei2CBus()
{
    // For now empty, we may add here some deinitialization parameters.
    return true;
}

// The interrupt handler:
// dispatches to the correct interrupt handler according to the current mode set:
bool
PPCI2CInterface::handleI2CInterrupt()
{
    switch(lastMode)
    {
        case kDumbMode:
#ifdef DEBUGMODE
            IOLog("PPCI2CInterface::i2cStandardSubModeInterrupts kDumbMode is an unsupported mode (for now)\n");
#endif
            break;

        case kStandardMode:
#ifdef DEBUGMODE
            IOLog("PPCI2CInterface::i2cStandardSubModeInterrupts kStandardMode is an unsupported mode (for now)\n");
#endif
            break;

        case kStandardSubMode:
            return (i2cStandardSubModeInterrupts(getInterruptStatus()));
            break;

        case kCombinedMode:
#ifdef DEBUGMODE
            IOLog("PPCI2CInterface::i2cStandardSubModeInterrupts kCombinedMode is an unsupported mode (for now)\n");
#endif
            break;
    }

    // By default we fail ...
    return false;
}
