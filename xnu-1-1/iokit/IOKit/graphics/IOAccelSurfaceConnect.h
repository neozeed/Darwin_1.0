
#ifndef _IOACCEL_SURFACE_CONNECT_H
#define _IOACCEL_SURFACE_CONNECT_H

#include <IOKit/graphics/IOAccelTypes.h>
#include <IOKit/graphics/IOAccelClientConnect.h>


/*
** Surface visible region in device coordinates.
**
** num_rects:	The number of rectangles in the rect array.  If num_rects
**		is zero the bounds rectangle is used for the visible rectangle.
**		If num_rects is zero the surface must be completely contained
**		by the device.
**
** bounds:	The unclipped surface rectangle in device coords.  Extends
**		beyond the device bounds if the surface is not totally on
**		the device.
**
** rect[]:	An array of visible rectangles in device coords.  If num_rects
**		is non-zero only the region described by these rectangles is
**		copied to the frame buffer during a flush operation.
*/
typedef struct
{
        UInt32        num_rects;
        IOAccelBounds bounds;
        IOAccelBounds rect[0];
} IOAccelDeviceRegion;


/*
** Determine the size of a region.
*/
#define IOACCEL_SIZEOF_DEVICE_REGION(_rgn_) (sizeof(IOAccelDeviceRegion) + (_rgn_)->num_rects * sizeof(IOAccelBounds))


/*
** Surface client public memory types.  Private memory types start with
** kIOAccelNumSurfaceMemoryTypes.
*/
enum eIOAccelSurfaceMemoryTypes {
	kIOAccelNumSurfaceMemoryTypes,
};


/*
** Surface client public methods.  Private methods start with
** kIOAccelNumSurfaceMethods.
*/
enum eIOAccelSurfaceMethods {
	kIOAccelSurfaceSetIDMode,
	kIOAccelSurfaceSetShape,
	kIOAccelSurfaceGetState,
	kIOAccelSurfaceLock,
	kIOAccelSurfaceUnlock,
	kIOAccelNumSurfaceMethods,
};


/*
** Option bits for IOAccelCreateSurface and the kIOAccelSurfaceSetIDMode method.
** The color depth field can take any value of the _CGSDepth enumeration.
*/
typedef enum {
        kIOAccelSurfaceModeColorDepthBits = 0x0000000F,
} eIOAccelSurfaceModeBits;


/*
** Options bits for IOAccelSetSurfaceShape and the kIOAccelSurfaceSetShape method.
*/
typedef enum {
        kIOAccelSurfaceShapeNone         = 0x00000000,
        kIOAccelSurfaceShapeBlockingBit  = 0x00000001,
        kIOAccelSurfaceShapeNonSimpleBit = 0x00000002,
} eIOAccelSurfaceShapeBits;


/*
** Return bits for the kIOAccelSurfaceGetState method.
*/
typedef enum {
	kIOAccelSurfaceStateNone    = 0x00000000,
	kIOAccelSurfaceStateIdleBit = 0x00000001,
} eIOAccelSurfaceStateBits;


#endif /* _IOACCEL_SURFACE_CONNECT_H */

