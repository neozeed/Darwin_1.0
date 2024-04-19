#ifndef _IOHITABLETPOINTER_H
#define _IOHITABLETPOINTER_H

#include <IOKit/hidsystem/IOHIDevice.h>
#include <IOKit/hidsystem/IOHITablet.h>
#include <IOKit/hidsystem/IOLLEvent.h>

#define kIOHITabletPointerID			"PointerID"
#define kIOHITabletPointerDeviceID		"DeviceID"
#define kIOHITabletPointerVendorType	"VendorPointerType"
#define kIOHITabletPointerType			"PointerType"
#define kIOHITabletPointerSerialNumber	"SerialNumber"
#define kIOHITabletPointerUniqueID		"UniqueID"

class IOHITabletPointer : public IOHIDevice
{
    OSDeclareDefaultStructors(IOHITabletPointer);

public:
    IOHITablet	*_tablet;
    UInt16		_deviceID;
    
    static UInt16 generateDeviceID();

    virtual bool init(OSDictionary *propTable);
    virtual bool attach(IOService *provider);

    virtual void dispatchTabletEvent(NXEventData *tabletEvent,
                                     AbsoluteTime ts);
    virtual void dispatchProximityEvent(NXEventData *proximityEvent,
                                        AbsoluteTime ts);
};

#endif /* !_IOHITABLETPOINTER_H */
