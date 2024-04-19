#include <IOKit/hidsystem/IOHITabletPointer.h>

OSDefineMetaClassAndStructors(IOHITabletPointer, IOHIDevice)

UInt16 IOHITabletPointer::generateDeviceID()
{
    static _nextDeviceID = 0;
    return _nextDeviceID++;
}

bool IOHITabletPointer::init( OSDictionary *propTable )
{
    if (!IOHIDevice::init(propTable)) {
        return false;
    }

    _deviceID = generateDeviceID();
    setProperty(kIOHITabletPointerDeviceID, (unsigned long long)_deviceID, 16);

    return true;
}

bool IOService::attach( IOService * provider )
{
    if (!IOHIDevice::attach(provider)) {
        return false;
    }

    _tablet = OSDynamicCast(IOHITablet, provider);

    return true;
}

void IOHITabletPointer::dispatchTabletEvent(NXEventData *tabletEvent,
                                            AbsoluteTime ts)
{
    if (_tablet) {
        _tablet->dispatchTabletEvent(tabletEvent, ts);
    }
}

void IOHITabletPointer::dispatchProximityEvent(NXEventData *proximityEvent,
                                               AbsoluteTime ts)
{
    if (_tablet) {
        _tablet->dispatchProximityEvent(proximityEvent, ts);
    }
}
