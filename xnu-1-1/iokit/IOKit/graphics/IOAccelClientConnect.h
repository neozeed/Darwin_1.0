
#ifndef _IOACCEL_CLIENT_CONNECT_H
#define _IOACCEL_CLIENT_CONNECT_H


/*
** The IOAccelerator service name
*/
#define kIOAcceleratorClassName "IOAccelerator"


/*
** IOAccelerator public client types.  Private client types start with
** kIOAccelNumClientTypes.
*/
enum eIOAcceleratorClientTypes {
	kIOAccelSurfaceClientType,
	kIOAccelNumClientTypes,
};


#endif /* _IOACCEL_CLIENT_CONNECT_H */

