#include <IOKit/IOUserClient.h>
#include <IOKit/assert.h>
#include "IOCDAudioNub.h"
#include "IOCDAudioNubClient.h"

#define super IOUserClient

OSDefineMetaClassAndStructors(IOCDAudioNubClient, IOUserClient)

IOCDAudioNubClient *IOCDAudioNubClient::withTask(task_t owningTask)
{
    IOCDAudioNubClient *me;

    me = new IOCDAudioNubClient;
    if(me) {
        if(!me->init()) {
            me->release();
            return NULL;
        }
        me->fTask = owningTask;
    }
    return me;
}

bool IOCDAudioNubClient::start( IOService * provider )
{
    kprintf("IOCDAudioNubClient::start\n");

    assert(OSDynamicCast(IOCDAudioNub, provider));
    if(!super::start(provider))
        return false;
    fOwner = (IOCDAudioNub *)provider;

    fMethods[kTest].object = this;
    fMethods[kTest].func =
        (IOMethod)&IOCDAudioNubClient::CDAudioNubTest;
    fMethods[kTest].count0 = 3;
    fMethods[kTest].count1 = 1;
    fMethods[kTest].flags = kIOUCScalarIScalarO;

    fMethods[kGetNumAudioTracks].object = this;
    fMethods[kGetNumAudioTracks].func =
        (IOMethod)&IOCDAudioNubClient::getNumAudioTracks;
    fMethods[kGetNumAudioTracks].count0 = 0;
    fMethods[kGetNumAudioTracks].count1 = 1;
    fMethods[kGetNumAudioTracks].flags = kIOUCScalarIScalarO;

    fMethods[kGetMSF].object = this;
    fMethods[kGetMSF].func =
        (IOMethod)&IOCDAudioNubClient::getMSF;
    fMethods[kGetMSF].count0 = 1;
    fMethods[kGetMSF].count1 = 1;
    fMethods[kGetMSF].flags = kIOUCScalarIScalarO;

    fMethods[kPlayTrack].object = this;
    fMethods[kPlayTrack].func =
        (IOMethod)&IOCDAudioNubClient::playTrack;
    fMethods[kPlayTrack].count0 = 1;
    fMethods[kPlayTrack].count1 = 1;
    fMethods[kPlayTrack].flags = kIOUCScalarIScalarO;

    fMethods[kPause].object = this;
    fMethods[kPause].func =
        (IOMethod)&IOCDAudioNubClient::pause;
    fMethods[kPause].count0 = 0;
    fMethods[kPause].count1 = 1;
    fMethods[kPause].flags = kIOUCScalarIScalarO;

    fMethods[kResume].object = this;
    fMethods[kResume].func =
        (IOMethod)&IOCDAudioNubClient::resume;
    fMethods[kResume].count0 = 0;
    fMethods[kResume].count1 = 1;
    fMethods[kResume].flags = kIOUCScalarIScalarO;

    fMethods[kGetVolume].object = this;
    fMethods[kGetVolume].func =
        (IOMethod)&IOCDAudioNubClient::getVolume;
    fMethods[kGetVolume].count0 = 0;
    fMethods[kGetVolume].count1 = 1;
    fMethods[kGetVolume].flags = kIOUCScalarIScalarO;

    fMethods[kSetVolume].object = this;
    fMethods[kSetVolume].func =
        (IOMethod)&IOCDAudioNubClient::setVolume;
    fMethods[kSetVolume].count0 = 2;
    fMethods[kSetVolume].count1 = 1;
    fMethods[kSetVolume].flags = kIOUCScalarIScalarO;

    fMethods[kGetAudioStatus].object = this;
    fMethods[kGetAudioStatus].func =
        (IOMethod)&IOCDAudioNubClient::getAudioStatus;
    fMethods[kGetAudioStatus].count0 = 0;
    fMethods[kGetAudioStatus].count1 = 4;
    fMethods[kGetAudioStatus].flags = kIOUCScalarIScalarO;

    fMethods[kPlayMSF].object = this;
    fMethods[kPlayMSF].func =
        (IOMethod)&IOCDAudioNubClient::playMSF;
    fMethods[kPlayMSF].count0 = 2;
    fMethods[kPlayMSF].count1 = 1;
    fMethods[kPlayMSF].flags = kIOUCScalarIScalarO;

    return true;
}

IOReturn IOCDAudioNubClient::clientClose( void )
{
    kprintf("IOCDAudioNubClient::clientClose\n");
    if (fOwner) {
        fOwner->close(this);
        detach( fOwner);
    }
    return kIOReturnSuccess;
}

IOReturn IOCDAudioNubClient::clientDied( void )
{
    return( clientClose());
}

IOReturn IOCDAudioNubClient::connectClient( IOUserClient * client )
{
    return kIOReturnSuccess;
}

IOExternalMethod * IOCDAudioNubClient::getExternalMethodForIndex( UInt32 index )
{
    kprintf("IOCDAudioNubClient::getExternalMethodForIndex: %d\n",index);
    if(index >= kMethods) {
        kprintf("IOCDAudioNubClient::getExternalMethodForIndex: bad index\n");
    	return NULL;
    } else {
        return &fMethods[index];
    }
}

IOReturn IOCDAudioNubClient::registerNotificationPort(
            mach_port_t port, UInt32 type )
{
    return kIOReturnUnsupported;
}

IOReturn IOCDAudioNubClient::clientMemoryForType( UInt32 type,
    UInt32 * flags, IOMemoryDescriptor ** memory )
{
    return 1;
}
   
IOReturn IOCDAudioNubClient::CDAudioNubTest(UInt32 opcode, UInt32 operand1, UInt32 operand2, UInt32 *status)
{
    // IOLog("IOCDAudioNubClient::CDAudioNubTest %d %d %d\n", opcode,operand1,operand2);

    if (!fOwner) return -2;

    switch(opcode) {
        case 0:
            return getNumAudioTracks(status);
        case 1:
            return getMSF(operand1-1,status);
        case 2:
            return playTrack(operand1-1,status);
        case 3:
            return pause(status);
        case 4:
            return resume(status);
        case 5:
            return getVolume(status);
        case 6:
            return setVolume((UInt8)operand1,(UInt8)operand2,status);
        case 7:
            return getAudioStatus(status,status,status,status);
        case 8:
            return playMSF(operand1,operand2,status);
        case 9:
            return readHeader(operand1,status);
    }
    
    return -1;
}

int subMSF(int m1, int m2)
{   
    int f1,f2;
    int min,sec,frm;

    // convert to total frames
    f1 = (m1 >> 16 & 0xff) * 75 * 60 +
         (m1 >> 8 & 0xff) * 75 +
         (m1  & 0xff);
    f2 = (m2 >> 16 & 0xff) * 75 * 60 +
         (m2 >> 8 & 0xff) * 75 +
         (m2  & 0xff);

    // get the total difference in frames
    m1 = f1 - f2;

    // convert make to MSF
    min = (int)(m1 / (75 * 60));
    m1 = m1 - min * 75 * 60;
    sec = (int)(m1 / 75);
    frm = m1 - sec * 75;
    return (min << 16) | (sec << 8) | frm;
}

IOReturn IOCDAudioNubClient::playTrack(UInt32 track,UInt32 *status)
{
    struct macEntireToc toc;
    UInt32 starting_msf, ending_msf;
    UInt32 starting_track, ending_track;

    if (!fOwner) return -2;
    if (track > 101) return -1;

    // read the TOC
    fOwner->readEntireTOC(&toc);

    // get the starting and ending MSF
    starting_msf = (toc.entries[track].pMin << 16) |
        (toc.entries[track].pSec << 8)  |
        (toc.entries[track].pFrame);

    ending_msf = (toc.entries[track+1].pMin << 16) |
        (toc.entries[track+1].pSec << 8)  |
        (toc.entries[track+1].pFrame);

    if (ending_msf == 0) return -3;

    // subtrace one frame from the ending MSF
    ending_msf = subMSF(ending_msf,1);

    // status returns the time
    *status = subMSF(ending_msf,starting_msf);

    // IOLog("IOCDAudioNubClient::playTrack %d %x %x\n", track,starting_msf,ending_msf);

    // set the audio stop address
    fOwner->setAudioStopAddress(kAbsoluteTime,(cdAddress)ending_msf);

   // send the play command and return immediately
    return fOwner->audioPlay(kAbsoluteTime,(cdAddress)starting_msf,kNormalMode);
}

IOReturn IOCDAudioNubClient::playMSF(
             UInt32 starting_msf, UInt32 ending_msf, UInt32 *status)
{
    // status returns the time
    *status = subMSF(ending_msf,starting_msf);

    // set the audio stop address
    fOwner->setAudioStopAddress(kAbsoluteTime,(cdAddress)ending_msf);

   // send the play command and return immediately
    return fOwner->audioPlay(kAbsoluteTime,(cdAddress)starting_msf,kNormalMode);
}

IOReturn IOCDAudioNubClient::readHeader(UInt32 address, UInt32 *status)
{
    struct headerInfo buffer;
    IOReturn ret;

    ret = fOwner->readHeader(address,&buffer);
    *status = buffer.address;
    return ret;
}
IOReturn IOCDAudioNubClient::getMSF(UInt32 track, UInt32 *status)
{
    struct macEntireToc toc;
    UInt32 msf;

    if (!fOwner) return -2;

    // get the TOC
    fOwner->readEntireTOC(&toc);

    // get the MSF
    msf = (toc.entries[track].pMin << 16) |
        (toc.entries[track].pSec << 8)  |
        (toc.entries[track].pFrame);

   // status is the MSF
   *status = msf;

   // IOLog("IOCDAudioNubClient::getMSF %d %x\n",track,msf);
   return 0;
}

IOReturn IOCDAudioNubClient::getNumAudioTracks(UInt32 *status)
{
    struct macEntireToc toc;
    UInt32 track = 0;

    if (!fOwner) return -2;

    // get the TOC
    fOwner->readEntireTOC(&toc);

    // count the number of tracks
    for(;;) {
         if (!toc.entries[track].trackPoint) break;
         track++;
    }
    if (track) track--;

    // status returns the number of tracks
    *status = track;

    // IOLog("IOCDAudioNubClient::getNumAudioTracks %d\n",track);
    return 0;
}

IOReturn IOCDAudioNubClient::pause(UInt32 *status)
{
    if (!fOwner) return -2;
    *status = 0;
    return fOwner->audioPause(true);
}

IOReturn IOCDAudioNubClient::resume(UInt32 *status)
{
    if (!fOwner) return -2;
    *status = 0;
    return fOwner->audioPause(false);
}

IOReturn IOCDAudioNubClient::getVolume(UInt32 *status)
{
    UInt8 right, left;
    IOReturn ret;

    if (!fOwner) return -2;
    ret = fOwner->readAudioVolume(&left,&right);
    *status = (left << 8 & 0xff00) | (right & 0xff);
    return ret;
}

IOReturn IOCDAudioNubClient::setVolume(UInt8 left, UInt8 right, UInt32 *status)
{
    IOReturn ret;

    // IOLog("IOCDAudioNubClient::setVolume %d %d\n",left,right);

    if (!fOwner) return -2;
    ret = fOwner->setVolume(left,right);
    *status = (left << 8 & 0xff00) | (right & 0xff);
    return ret;
}

IOReturn IOCDAudioNubClient::getAudioStatus(UInt32 *status,
             UInt32 *absAddress, UInt32 *relAddress, UInt32 *trackType)
{
    IOReturn ret;
    struct audioStatus stat;
    struct qSubcode buffer;
    bool found;
    UInt8 buf[15];

    ret = fOwner->getAudioStatus(&stat);
    *status = stat.status;
    *trackType = stat.type;

    ret = fOwner->readTheQSubcode(&buffer);
    *relAddress = buffer.relAddress;
    *absAddress = buffer.absAddress;

    // IOLog("IOCDAudioNubClient::getAudioStatus abs=0x%x rel=0x%x status=%d type=%x\n", *absAddress, *relAddress, *status, *trackType);

    return ret;
}
