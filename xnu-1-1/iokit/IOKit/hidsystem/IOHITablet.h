#ifndef _IOHITABLET_H
#define _IOHITABLET_H

#include <IOKit/hidsystem/IOHIPointing.h>
#include <IOKit/hidsystem/IOLLEvent.h>

class IOHITabletPointer;

#define kIOHIVendorID		"VendorID"
#define kIOHISystemTabletID	"SystemTabletID"
#define kIOHIVendorTabletID	"VendorTabletID"

typedef void (*TabletEventAction)(OSObject		*target,
                                  NXEventData	*tabletData,	// Do we want to parameterize this?
                                  AbsoluteTime ts);

typedef void (*ProximityEventAction)(OSObject		*target,
                                     NXEventData	*proximityData,	// or this?
                                     AbsoluteTime ts);
                                  
class IOHITablet : public IOHIPointing
{
    OSDeclareDefaultStructors(IOHITablet);

public:
    UInt16		_systemTabletID;

private:
    OSObject *				_tabletEventTarget;
    TabletEventAction		_tabletEventAction;
    OSObject *				_proximityEventTarget;
    ProximityEventAction	_proximityEventAction;

protected:
    virtual void dispatchTabletEvent(NXEventData *tabletEvent,
                                     AbsoluteTime ts);

    virtual void dispatchProximityEvent(NXEventData *proximityEvent,
                                        AbsoluteTime ts);

    virtual bool startTabletPointer(IOHITabletPointer *pointer, OSDictionary *properties);

public:
    static UInt16 generateTabletID();

    virtual bool init(OSDictionary * propTable);
    virtual bool open(IOService *	client,
                      IOOptionBits	options,
                      RelativePointerEventAction	rpeAction,
                      AbsolutePointerEventAction	apeAction,
                      ScrollWheelEventAction		sweAction,
                      TabletEventAction			tabletAction,
                      ProximityEventAction		proximityAction);

};

#endif /* !_IOHITABLET_H */
