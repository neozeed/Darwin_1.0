#include <IOKit/hidsystem/IOHITablet.h>
#include <IOKit/hidsystem/IOHITabletPointer.h>

OSDefineMetaClassAndStructors(IOHITablet, IOHIPointing);

UInt16 IOHITablet::generateTabletID()
{
    static UInt16 _nextTabletID = 0;
    return _nextTabletID++;
}

bool IOHITablet::init(OSDictionary *propTable)
{
    if (!IOHIPointing::init(propTable)) {
        return false;
    }

    _systemTabletID = generateTabletID();
    setProperty(kIOHISystemTabletID, (unsigned long long)_systemTabletID, 16);

    return true;
}

bool IOHITablet::open(IOService *client,
                      IOOptionBits options,
                      RelativePointerEventAction	rpeAction,
                      AbsolutePointerEventAction	apeAction,
                      ScrollWheelEventAction		sweAction,
                      TabletEventAction				tabletAction,
                      ProximityEventAction			proximityAction)
{
    if (!IOHIPointing::open(client, options, rpeAction, apeAction, sweAction)) {
        return false;
    }

    _tabletEventTarget = client;
    _tabletEventAction = tabletAction;
    _proximityEventTarget = client;
    _proximityEventAction = proximityAction;

    return true;
}

void IOHITablet::dispatchTabletEvent(NXEventData *tabletEvent,
                                     AbsoluteTime ts)
{
    if (_tabletEventAction) {
        (*_tabletEventAction)(_tabletEventTarget,
                            tabletEvent,
                            ts);
    }
}

void IOHITablet::dispatchProximityEvent(NXEventData *proximityEvent,
                                        AbsoluteTime ts)
{
    if (_proximityEventAction) {
        (*_proximityEventAction)(_proximityEventTarget,
                               proximityEvent,
                               ts);
    }
}

bool IOHITablet::startTabletPointer(IOHITabletPointer *pointer, OSDictionary *properties)
{
    bool result = false;

    do {
        if (!pointer)
            break;

        if (!pointer->init(properties))
            break;

        if (!pointer->attach(this))
            break;

        if (!pointer->start(this))
            break;

        result = true;
    } while (false);

    return result;
}

