
#ifndef _IOACCEL_TYPES_H
#define _IOACCEL_TYPES_H

#include <IOKit/IOTypes.h>


/* Integer rectangle in device coordinates */
typedef struct
{
        SInt16 x;
        SInt16 y;
        SInt16 w;
        SInt16 h;
} IOAccelBounds;


#endif /* _IOACCEL_TYPES_H */

