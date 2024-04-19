#include <IOKit/audio/IOAudioManager.h>

#include <libkern/c++/OSCollectionIterator.h>

#define DEFAULT_INITIAL_VOLUME 		MASTER_VOLUME_MAX
#define DEFAULT_INITIAL_MUTE		false
#define DEFAULT_INITIAL_INCREMENT	4096

static IOAudioManager *amInstance = NULL;

OSDefineMetaClassAndStructors(IOAudioManager, IOService);

IOAudioManager *IOAudioManager::sharedInstance()
{
    return amInstance;
}

bool IOAudioManager::init(OSDictionary *properties)
{
    if (!IOService::init(properties)) {
        return false;
    }

    masterVolumeLeft = DEFAULT_INITIAL_VOLUME;
    masterVolumeRight = DEFAULT_INITIAL_VOLUME;
    masterMute = DEFAULT_INITIAL_MUTE;
    masterVolumeIncrement = DEFAULT_INITIAL_INCREMENT;

    setProperty(kMasterVolumeLeft, (unsigned long long)masterVolumeLeft, sizeof(masterVolumeLeft)*8);
    setProperty(kMasterVolumeRight, (unsigned long long)masterVolumeRight, sizeof(masterVolumeRight)*8);
    setProperty(kMasterMute, (unsigned long long)masterMute, sizeof(masterMute)*8);
    setProperty(kMasterVolumeIncrement, (unsigned long long)masterVolumeIncrement, sizeof(masterVolumeIncrement)*8);

    audioControllers = NULL;
    publishNotify = NULL;
    driverLock = NULL;

    return true;
}

void IOAudioManager::free()
{
    if (audioControllers) {
        audioControllers->release();
    }

    if (publishNotify) {
        publishNotify->release();
    }

    if (driverLock) {
        IOLockFree(driverLock);
    }
    
    IOService::free();
}

bool IOAudioManager::start(IOService *provider)
{
    bool success = false;

    amInstance = this;

    do {
        if (!IOService::start(provider)) {
            break;
        }

        audioControllers = OSArray::withCapacity(1);

        driverLock = IOLockAlloc();

        publishNotify = addNotification(gIOPublishNotification,
                                        serviceMatching("IOAudioController"),
                                        (IOServiceNotificationHandler) &audioPublishNotificationHandler,
                                        this,
                                        0);

        if (!publishNotify || !driverLock) {
            break;
        }

        IOLockInit(driverLock);

        success = true;
    } while (false);

    if (!success) {
        amInstance = NULL;
    }

    return success;
}

IOReturn IOAudioManager::setProperties( OSObject * properties )
{
    OSDictionary *props;
    IOReturn result = kIOReturnSuccess;

    if (properties && (props = OSDynamicCast(OSDictionary, properties))) {
        OSCollectionIterator *iterator;
        OSObject *iteratorKey;

        iterator = OSCollectionIterator::withCollection(props);
        while ((iteratorKey = iterator->getNextObject())) {
            OSSymbol *key;

            key = OSDynamicCast(OSSymbol, iteratorKey);
            if (key) {
                OSNumber *value;
                value = OSDynamicCast(OSNumber, props->getObject(key));

                if (value) {
                    if (key->isEqualTo(kMasterVolumeLeft)) {
                        setMasterVolumeLeft(value->unsigned16BitValue());
                    } else if (key->isEqualTo(kMasterVolumeRight)) {
                        setMasterVolumeRight(value->unsigned16BitValue());
                    } else if (key->isEqualTo(kMasterMute)) {
                        setMasterMute(value->unsigned8BitValue());
                    } else if (key->isEqualTo(kMasterVolumeIncrement)) {
                        setMasterVolumeIncrement(value->unsigned16BitValue());
                    }
                } else {
                    result = kIOReturnBadArgument;
                    break;
                }
            } else {
                result = kIOReturnBadArgument;
                break;
            }
        }
    } else {
        result = kIOReturnBadArgument;
    }

    return result;
}

bool IOAudioManager::audioPublishNotificationHandler(IOAudioManager *self,
                                                     void *ref,
                                                     IOService *newService)
{
    self->attach(newService);

    if (OSDynamicCast(IOAudioController, newService)) {
        self->registerAudioController((IOAudioController *)newService);
    }

    return true;
}

bool IOAudioManager::registerAudioController(IOAudioController *controller)
{
    IOTakeLock(driverLock);
    
    audioControllers->setObject(controller);

    controller->setMasterMute(masterMute);
    controller->setMasterVolumeLeft(masterVolumeLeft);
    controller->setMasterVolumeRight(masterVolumeRight);

    IOUnlock(driverLock);
    
    return true;
}

UInt16 IOAudioManager::getMasterVolumeLeft()
{
    return masterVolumeLeft;
}

UInt16 IOAudioManager::getMasterVolumeRight()
{
    return masterVolumeRight;
}

UInt16 IOAudioManager::setMasterVolumeLeft(UInt16 newMasterVolumeLeft)
{
    UInt16 oldMasterVolumeLeft;

    oldMasterVolumeLeft = masterVolumeLeft;
    masterVolumeLeft = newMasterVolumeLeft;

    if (masterVolumeLeft != oldMasterVolumeLeft) {
        OSCollectionIterator *iter;

        setProperty(kMasterVolumeLeft, (unsigned long long)masterVolumeLeft, sizeof(masterVolumeLeft)*8);

        IOTakeLock(driverLock);
        
        iter = OSCollectionIterator::withCollection(audioControllers);
        if (iter) {
            IOAudioController *controller;

            while ((controller = (IOAudioController *)iter->getNextObject()) != NULL) {
                controller->setMasterVolumeLeft(masterVolumeLeft);
            }

            iter->release();
        }

        IOUnlock(driverLock);
    }

    return oldMasterVolumeLeft;
}

UInt16 IOAudioManager::setMasterVolumeRight(UInt16 newMasterVolumeRight)
{
    UInt16 oldMasterVolumeRight;

    oldMasterVolumeRight = masterVolumeRight;
    masterVolumeRight = newMasterVolumeRight;

    if (masterVolumeRight != oldMasterVolumeRight) {
        OSCollectionIterator *iter;

        setProperty(kMasterVolumeRight, (unsigned long long)masterVolumeRight, sizeof(masterVolumeRight)*8);

        IOTakeLock(driverLock);

        iter = OSCollectionIterator::withCollection(audioControllers);
        if (iter) {
            IOAudioController *controller;

            while ((controller = (IOAudioController *)iter->getNextObject()) != NULL) {
                controller->setMasterVolumeRight(masterVolumeRight);
            }

            iter->release();
        }

        IOUnlock(driverLock);
    }

    return oldMasterVolumeRight;
}

bool IOAudioManager::getMasterMute()
{
    return masterMute;
}

bool IOAudioManager::setMasterMute(bool newMasterMute)
{
    bool oldMasterMute;

    oldMasterMute = masterMute;
    masterMute = newMasterMute;

    if (masterMute != oldMasterMute) {
        OSCollectionIterator *iter;

        setProperty(kMasterMute, (unsigned long long)masterMute, sizeof(masterMute)*8);

        IOTakeLock(driverLock);

        iter = OSCollectionIterator::withCollection(audioControllers);
        if (iter) {
            IOAudioController *controller;

            while ((controller = (IOAudioController *)iter->getNextObject()) != NULL) {
                controller->setMasterMute(masterMute);
            }

            iter->release();
        }

        IOUnlock(driverLock);
    }

    return oldMasterMute;
}

UInt16 IOAudioManager::getMasterVolumeIncrement()
{
    return masterVolumeIncrement;
}

UInt16 IOAudioManager::setMasterVolumeIncrement(UInt16 newMasterVolumeIncrement)
{
    UInt16 oldMasterVolumeIncrement;

    oldMasterVolumeIncrement = masterVolumeIncrement;
    masterVolumeIncrement = newMasterVolumeIncrement;

    setProperty(kMasterVolumeIncrement, (unsigned long long)masterVolumeIncrement, sizeof(masterVolumeIncrement)*8);

    return oldMasterVolumeIncrement;
}

UInt16 IOAudioManager::incrementMasterVolume()
{
    SInt32 newMasterVolumeLeft;
    SInt32 newMasterVolumeRight;

    newMasterVolumeLeft = masterVolumeLeft + masterVolumeIncrement;
    newMasterVolumeRight = masterVolumeRight + masterVolumeIncrement;
    
    if (newMasterVolumeLeft > MASTER_VOLUME_MAX) {
        newMasterVolumeRight = newMasterVolumeRight - (newMasterVolumeLeft - MASTER_VOLUME_MAX);
        newMasterVolumeLeft = MASTER_VOLUME_MAX;
    }

    if (newMasterVolumeRight > MASTER_VOLUME_MAX) {
        newMasterVolumeLeft = newMasterVolumeLeft - (newMasterVolumeRight - MASTER_VOLUME_MAX);
        newMasterVolumeRight = MASTER_VOLUME_MAX;
    }

    setMasterVolumeLeft(newMasterVolumeLeft);
    setMasterVolumeRight(newMasterVolumeRight);

    return masterVolumeLeft > masterVolumeRight ? masterVolumeLeft : masterVolumeRight;
}

UInt16 IOAudioManager::decrementMasterVolume()
{
    SInt32 newMasterVolumeLeft;
    SInt32 newMasterVolumeRight;

    newMasterVolumeLeft = masterVolumeLeft - masterVolumeIncrement;
    newMasterVolumeRight = masterVolumeRight - masterVolumeIncrement;
    
    if (newMasterVolumeLeft < MASTER_VOLUME_MIN) {
        newMasterVolumeRight = newMasterVolumeRight + (MASTER_VOLUME_MIN - newMasterVolumeLeft);
        newMasterVolumeLeft = MASTER_VOLUME_MIN;
    }

    if (newMasterVolumeRight < MASTER_VOLUME_MIN) {
        newMasterVolumeLeft = newMasterVolumeLeft + (MASTER_VOLUME_MIN - newMasterVolumeRight);
        newMasterVolumeRight = MASTER_VOLUME_MIN;
    }

    setMasterVolumeLeft(newMasterVolumeLeft);
    setMasterVolumeRight(newMasterVolumeRight);

    return masterVolumeLeft > masterVolumeRight ? masterVolumeLeft : masterVolumeRight;
}

bool IOAudioManager::toggleMasterMute()
{
    setMasterMute(!masterMute);
    return masterMute;
}
