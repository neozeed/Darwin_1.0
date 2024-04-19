#ifndef _IOKIT_IOCDAUDIONUBCLIENT_H
#define _IOKIT_IOCDAUDIONUBCLIENT_H

#include <IOKit/IOUserClient.h>

enum {
    kTest = 0,
    kGetNumAudioTracks,
    kGetMSF,
    kPlayTrack,
    kPause,
    kResume,
    kGetVolume,
    kSetVolume,
    kGetAudioStatus,
    kPlayMSF,
    kMethods
};

class IOCDAudioNub;

class IOCDAudioNubClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOCDAudioNubClient)

private:
    task_t		fTask;
    IOExternalMethod    fMethods[ kMethods ];
    IOCDAudioNub *      fOwner;

public:
    static IOCDAudioNubClient *withTask(task_t owningTask);
    virtual IOReturn clientClose( void );
    virtual IOReturn clientDied( void );

    virtual IOReturn registerNotificationPort(
		mach_port_t port, UInt32 type );

    virtual IOReturn connectClient( IOUserClient * client );

    virtual IOReturn clientMemoryForType( UInt32 type,
        UInt32 * flags, IOMemoryDescriptor ** memory );

    virtual IOExternalMethod * getExternalMethodForIndex( UInt32 index );

    virtual bool start( IOService * provider );

    virtual IOReturn CDAudioNubTest(UInt32 opcode,
        UInt32 operand1, UInt32 operand2, UInt32 *status);

    virtual IOReturn playTrack(UInt32 track, UInt32 *status);
    virtual IOReturn playMSF(UInt32 start_msf, UInt32 end_msf, UInt32 *status);
    virtual IOReturn getMSF(UInt32 track, UInt32 *status);
    virtual IOReturn getNumAudioTracks(UInt32 *status);
    virtual IOReturn pause(UInt32 *status);
    virtual IOReturn resume(UInt32 *status);
    virtual IOReturn getVolume(UInt32 *status);
    virtual IOReturn setVolume(UInt8 left, UInt8 right, UInt32 *status);
    virtual IOReturn getAudioStatus(UInt32 *status, UInt32 *absAddress,
                         UInt32 *relAddress, UInt32 *trackType);
    virtual IOReturn readHeader(UInt32 address, UInt32 *status);
};

#endif /* _IOKIT_IOCDAUDIONUBCLIENT_H */
