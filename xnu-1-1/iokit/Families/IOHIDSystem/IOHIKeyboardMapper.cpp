/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* 	Copyright (c) 1992 NeXT Computer, Inc.  All rights reserved. 
 *
 * KeyMap.m - Generic keymap string parser and keycode translator.
 *
 * HISTORY
 * 19 June 1992    Mike Paquette at NeXT
 *      Created. 
 * 5  Aug 1993	  Erik Kay at NeXT
 *	minor API cleanup
 * 11 Nov 1993	  Erik Kay at NeXT
 *	fix to allow prevent long sequences from overflowing the event queue
 * 12 Nov 1998    Dan Markarian at Apple
 *      major cleanup of public API's; converted to C++
 */

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/hidsystem/IOLLEvent.h>
#include <IOKit/hidsystem/IOHIKeyboard.h>
#include <IOKit/hidsystem/IOHIKeyboardMapper.h>

#define super OSObject
OSDefineMetaClassAndStructors(IOHIKeyboardMapper, OSObject);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOHIKeyboardMapper * IOHIKeyboardMapper::keyboardMapper(
                                        IOHIKeyboard * delegate,
                                        const UInt8 *  mapping,
                                        UInt32         mappingLength,
                                        bool           mappingShouldBeFreed )
{
  IOHIKeyboardMapper * me = new IOHIKeyboardMapper;

  if (me && !me->init(delegate, mapping, mappingLength, mappingShouldBeFreed))
  {
    me->free();
    return 0;
  }

  return me;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/*
 * Common KeyMap initialization
 */
bool IOHIKeyboardMapper::init( IOHIKeyboard * delegate,
                               const UInt8 *  mapping,
                               UInt32         mappingLength,
                               bool           mappingShouldBeFreed )
{
  if (!super::init())  return false;

  if (!parseKeyMapping(mapping, mappingLength, &_parsedMapping))  return false;

  _delegate                 = delegate;
  _mappingShouldBeFreed     = mappingShouldBeFreed;
  _parsedMapping.mapping    = mapping;
  _parsedMapping.mappingLen = mappingLength;

  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void IOHIKeyboardMapper::free()
{
  if (_mappingShouldBeFreed && _parsedMapping.mapping)
    IOFree((void *)_parsedMapping.mapping, _parsedMapping.mappingLen);

  super::free();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

const UInt8 * IOHIKeyboardMapper::mapping()
{
  return (const UInt8 *)_parsedMapping.mapping;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

UInt32 IOHIKeyboardMapper::mappingLength()
{
  return _parsedMapping.mappingLen;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool IOHIKeyboardMapper::serialize(OSSerialize *s) const
{
    OSData * data;
    bool ok;

    if (s->previouslySerialized(this)) return true;

    data = OSData::withBytesNoCopy( (void *) _parsedMapping.mapping, 								_parsedMapping.mappingLen );
    if (data) {
	ok = data->serialize(s);
	data->release();
    } else
	ok = false;

    return( ok );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

//
// Perform the mapping of 'key' moving in the specified direction
// into events.
//

void IOHIKeyboardMapper::translateKeyCode(UInt8        key,
                                          bool         keyDown,
                                          kbdBitVector keyBits)
{
  unsigned char thisBits = _parsedMapping.keyBits[key];

  /* do mod bit update and char generation in useful order */

  if (keyDown)
  {
    EVK_KEYDOWN(key, keyBits);

    if (thisBits & NX_MODMASK)     doModCalc(key, keyBits);
    if (thisBits & NX_CHARGENMASK) doCharGen(key, keyDown);
  }
  else
  {
    EVK_KEYUP(key, keyBits);
    if (thisBits & NX_CHARGENMASK) doCharGen(key, keyDown);
    if (thisBits & NX_MODMASK)     doModCalc(key, keyBits);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

//
// Support goop for parseKeyMapping.  These routines are
// used to walk through the keymapping string.  The string
// may be composed of bytes or shorts.  If using shorts, it
// MUST always be aligned to use short boundries.
//
typedef struct {
    unsigned const char *bp;
    unsigned const char *endPtr;
    int shorts;
} NewMappingData;

static inline unsigned int NextNum(NewMappingData *nmd)
{
    if (nmd->bp >= nmd->endPtr)
	return(0);
    if (nmd->shorts)
	return(*((unsigned short *)nmd->bp)++);
    else
	return(*((unsigned char *)nmd->bp)++);
}

//
// Perform the actual parsing operation on a keymap.  Returns false on failure.
//

bool IOHIKeyboardMapper::parseKeyMapping(const UInt8 *        mapping,
                                         UInt32               mappingLength,
	                                 NXParsedKeyMapping * parsedMapping) const
{
	NewMappingData nmd;
	int i, j, k, l, n;
	unsigned int m;
	int keyMask, numMods;
	int maxSeqNum = -1;

	/* Initialize the new map. */
	bzero( parsedMapping, sizeof (NXParsedKeyMapping) );
	parsedMapping->maxMod = -1;
	parsedMapping->numDefs = -1;
	parsedMapping->numSeqs = -1;

	nmd.endPtr = mapping + mappingLength;
	nmd.bp = mapping;
	nmd.shorts = 1;		// First value, the size, is always a short

	/* Start filling it in with the new data */
	parsedMapping->mapping = (unsigned char *)mapping;
	parsedMapping->mappingLen = mappingLength;
	parsedMapping->shorts = nmd.shorts = NextNum(&nmd);

	/* Walk through the modifier definitions */
	numMods = NextNum(&nmd);
	for(i=0; i<numMods; i++)
	{
	    /* Get bit number */
	    if ((j = NextNum(&nmd)) >= NX_NUMMODIFIERS)
		return false;

	    /* Check maxMod */
	    if (j > parsedMapping->maxMod)
		parsedMapping->maxMod = j;

	    /* record position of this def */
	    parsedMapping->modDefs[j] = (unsigned char *)nmd.bp;

	    /* Loop through each key assigned to this bit */
	    for(k=0,n = NextNum(&nmd);k<n;k++)
	    {
		/* Check that key code is valid */
		if ((l = NextNum(&nmd)) >= NX_NUMKEYCODES)
		    return false;
		/* Make sure the key's not already assigned */
		if (parsedMapping->keyBits[l] & NX_MODMASK)
			return false;
		/* Set bit for modifier and which one */
		parsedMapping->keyBits[l] |=NX_MODMASK | (j & NX_WHICHMODMASK);
	    }
	}

	/* Walk through each key definition */
	parsedMapping->numDefs = NextNum(&nmd);
	n = parsedMapping->numDefs;
	for( i=0; i < NX_NUMKEYCODES; i++)
	{
	    if (i < n)
	    {
		parsedMapping->keyDefs[i] = (unsigned char *)nmd.bp;
		if ((keyMask = NextNum(&nmd)) != (nmd.shorts ? 0xFFFF: 0x00FF))
		{
		    /* Set char gen bit for this guy: not a no-op */
		    parsedMapping->keyBits[i] |= NX_CHARGENMASK;
		    /* Check key defs to find max sequence number */
		    for(j=0, k=1; j<=parsedMapping->maxMod; j++, keyMask>>=1)
		    {
			    if (keyMask & 0x01)
				k*= 2;
		    }
		    for(j=0; j<k; j++)
		    {
			m = NextNum(&nmd);
			l = NextNum(&nmd);
			if (m == (unsigned)(nmd.shorts ? 0xFFFF: 0x00FF))
			    if (((int)l) > maxSeqNum)
				maxSeqNum = l;	/* Update expected # of seqs */
		    }
		}
		else /* unused code within active range */
		    parsedMapping->keyDefs[i] = NULL;
	    }
	    else /* Unused code past active range */
	    {
		parsedMapping->keyDefs[i] = NULL;
	    }
	}
	/* Walk through sequence defs */
	parsedMapping->numSeqs = NextNum(&nmd);
       	/* If the map calls more sequences than are declared, bail out */
	if (parsedMapping->numSeqs <= maxSeqNum)
	    return false;

	/* Walk past all sequences */
	for(i = 0; i < parsedMapping->numSeqs; i++)
	{
	    parsedMapping->seqDefs[i] = (unsigned char *)nmd.bp;
	    /* Walk thru entries in a seq. */
	    for(j=0, l=NextNum(&nmd); j<l; j++)
	    {
		NextNum(&nmd);
		NextNum(&nmd);
	    }
	}
	/* Install Special device keys.  These override default values. */
	numMods = NextNum(&nmd);	/* Zero on old style keymaps */
	if ( numMods > NX_NUMSPECIALKEYS )
	    return false;
	if ( numMods )
	{
	    for ( i = 0; i < NX_NUMSPECIALKEYS; ++i )
		parsedMapping->specialKeys[i] = NX_NOSPECIALKEY;
	    for ( i = 0; i < numMods; ++i )
	    {
		j = NextNum(&nmd);	/* Which modifier key? */
		l = NextNum(&nmd);	/* Scancode for modifier key */
		if ( j >= NX_NUMSPECIALKEYS )
		    return false;
		parsedMapping->specialKeys[j] = l;
	    }
	}
	else  /* No special keys defs implies an old style keymap */
	{
		return false;	/* Old style keymaps are guaranteed to do */
				/* the wrong thing on ADB keyboards */
	}
	/* Install bits for Special device keys */
	for(i=0; i<NX_NUM_SCANNED_SPECIALKEYS; i++)
	{
	    if ( parsedMapping->specialKeys[i] != NX_NOSPECIALKEY )
	    {
		parsedMapping->keyBits[parsedMapping->specialKeys[i]] |=
		    (NX_CHARGENMASK | NX_SPECIALKEYMASK);
	    }
	}
    
	return true;
}

static inline int NEXTNUM(unsigned char ** mapping, short shorts)
{
  int returnValue;

  if (shorts)
  {
    returnValue = *((unsigned short *)*mapping);
    *mapping += sizeof(unsigned short);
  }
  else
  {
    returnValue = **((unsigned char  **)mapping);
    *mapping += sizeof(unsigned char);
  }

  return returnValue;
}

//
// Look up in the keymapping each key associated with the modifier bit.
// Look in the device state to see if that key is down.
// Return 1 if a key for modifier 'bit' is down.  Return 0 if none is down
//
static inline int IsModifierDown(NXParsedKeyMapping *parsedMapping,
			 	 kbdBitVector keyBits,
				 int bit )
{
    int i, n;
    unsigned char *mapping;
    unsigned key;
    short shorts = parsedMapping->shorts;

    if ( (mapping = parsedMapping->modDefs[bit]) != 0 ) {
	for(i=0, n=NEXTNUM(&mapping, shorts); i<n; i++)
	{
	    key = NEXTNUM(&mapping, shorts);
	    if ( EVK_IS_KEYDOWN(key, keyBits) )
		return 1;
	}
    }
    return 0;
}

void IOHIKeyboardMapper::calcModBit(int bit, kbdBitVector keyBits)
{
	int		bitMask;
	unsigned	myFlags;

	bitMask = 1<<(bit+16);

	/* Initially clear bit, as if key-up */
	myFlags = _delegate->deviceFlags() & (~bitMask);
	/* Set bit if any associated keys are down */
	if ( IsModifierDown( &_parsedMapping, keyBits, bit ) )
		myFlags |= bitMask;

	if ( bit == NX_MODIFIERKEY_ALPHALOCK ) /* Caps Lock key */
	    _delegate->setAlphaLock((myFlags & NX_ALPHASHIFTMASK) ? true : false);

	_delegate->setDeviceFlags(myFlags);
}


//
// Perform flag state update and generate flags changed events for this key.
//
void IOHIKeyboardMapper::doModCalc(int key, kbdBitVector keyBits)
{
    int thisBits;

    thisBits = _parsedMapping.keyBits[key];

    if (thisBits & NX_MODMASK)
    {
	calcModBit((thisBits & NX_WHICHMODMASK), keyBits);
	/* The driver generates flags-changed events only when there is
	   no key-down or key-up event generated */
	if (!(thisBits & NX_CHARGENMASK))
	{
		/* Post the flags-changed event */
		_delegate->keyboardEvent(NX_FLAGSCHANGED,
		 /* flags */            _delegate->eventFlags(),
		 /* keyCode */          key,
		 /* charCode */         0,
		 /* charSet */          0,
		 /* originalCharCode */ 0,
		 /* originalCharSet */  0);
	}
	else	/* Update, but don't generate an event */
		_delegate->updateEventFlags(_delegate->eventFlags());
    }
}

//
// Perform character event generation for this key
//
void IOHIKeyboardMapper::doCharGen(int keyCode, bool down)
{
    int	i, n, eventType, adjust, thisMask, modifiers, saveModifiers;
    short shorts;
    unsigned charSet, origCharSet;
    unsigned charCode, origCharCode;
    unsigned char *mapping;
    unsigned eventFlags, origflags;

    _delegate->setCharKeyActive(true);	// a character generating key is active

    eventType = (down == true) ? NX_KEYDOWN : NX_KEYUP;
    eventFlags = _delegate->eventFlags();
    saveModifiers = eventFlags >> 16;	// machine independent mod bits
    /* Set NX_ALPHASHIFTMASK based on alphaLock OR shift active */
    if( saveModifiers & (NX_SHIFTMASK >> 16))
	saveModifiers |= (NX_ALPHASHIFTMASK >> 16);

    /* Get this key's key mapping */
    shorts = _parsedMapping.shorts;
    mapping = _parsedMapping.keyDefs[keyCode];
    modifiers = saveModifiers;
    if ( mapping )
    {
	/* Build offset for this key */
	thisMask = NEXTNUM(&mapping, shorts);
	if (thisMask && modifiers)
	{
	    adjust = (shorts ? sizeof(short) : sizeof(char))*2;
	    for( i = 0; i <= _parsedMapping.maxMod; ++i)
	    {
		if (thisMask & 0x01)
		{
		    if (modifiers & 0x01)
			mapping += adjust;
		    adjust *= 2;
		}
		thisMask >>= 1;
		modifiers >>= 1;
	    }
	}
	charSet = NEXTNUM(&mapping, shorts);
	charCode = NEXTNUM(&mapping, shorts);
	
	/* construct "unmodified" character */
	mapping = _parsedMapping.keyDefs[keyCode];
        modifiers = saveModifiers & ((NX_ALPHASHIFTMASK | NX_SHIFTMASK) >> 16);

	thisMask = NEXTNUM(&mapping, shorts);
	if (thisMask && modifiers)
	{
	    adjust = (shorts ? sizeof(short) : sizeof(char)) * 2;
	    for ( i = 0; i <= _parsedMapping.maxMod; ++i)
	    {
		if (thisMask & 0x01)
		{
		    if (modifiers & 0x01)
			mapping += adjust;
		    adjust *= 2;
		}
		thisMask >>= 1;
		modifiers >>= 1;
	    }
	}
	origCharSet = NEXTNUM(&mapping, shorts);
	origCharCode = NEXTNUM(&mapping, shorts);
	
	if (charSet == (unsigned)(shorts ? 0xFFFF : 0x00FF))
	{
	    // Process as a character sequence
	    // charCode holds the sequence number
	    mapping = _parsedMapping.seqDefs[charCode];
	    
	    origflags = eventFlags;
	    for(i=0,n=NEXTNUM(&mapping, shorts);i<n;i++)
	    {
#if 0
		// every 10 characters, give the windowserver a chance to
		// actually read the events out of the event queue
		if ((i % 10) == 9)
		    (void) thread_block();
#endif
		if ( (charSet = NEXTNUM(&mapping, shorts)) == 0xFF ) /* metakey */
		{
		    if ( down == true )	/* down or repeat */
		    {
			eventFlags |= (1 << (NEXTNUM(&mapping, shorts) + 16));
			_delegate->keyboardEvent(NX_FLAGSCHANGED,
			 /* flags */            _delegate->deviceFlags(),
			 /* keyCode */          keyCode,
			 /* charCode */         0,
			 /* charSet */          0,
			 /* originalCharCode */ 0,
			 /* originalCharSet */  0);
		    }
		    else
			NEXTNUM(&mapping, shorts);	/* Skip over value */
		}
		else
		{
		    charCode = NEXTNUM(&mapping, shorts);
		    _delegate->keyboardEvent(eventType,
		     /* flags */            eventFlags,
		     /* keyCode */          keyCode,
		     /* charCode */         charCode,
		     /* charSet */          charSet,
		     /* originalCharCode */ charCode,
		     /* originalCharSet */  charSet);
		}
	    }
	    /* Done with macro.  Restore the flags if needed. */
	    if ( eventFlags != origflags )
	    {
		_delegate->keyboardEvent(NX_FLAGSCHANGED,
		 /* flags */            _delegate->deviceFlags(),
		 /* keyCode */          keyCode,
		 /* charCode */         0,
		 /* charSet */          0,
		 /* originalCharCode */ 0,
		 /* originalCharSet */  0);
		eventFlags = origflags;
	    }
	}
	else	/* A simple character generating key */
	{
	    _delegate->keyboardEvent(eventType,
	     /* flags */            eventFlags,
	     /* keyCode */          keyCode,
	     /* charCode */         charCode,
	     /* charSet */          charSet,
	     /* originalCharCode */ origCharCode,
	     /* originalCharSet */  origCharSet);
	}
    } /* if (mapping) */
    
    /*
     * Check for a device control key: note that they always have CHARGEN
     * bit set
     */
    if (_parsedMapping.keyBits[keyCode] & NX_SPECIALKEYMASK)
    {
	for(i=0; i<NX_NUM_SCANNED_SPECIALKEYS; i++)
	{
	    if ( keyCode == _parsedMapping.specialKeys[i] )
	    {
		_delegate->keyboardSpecialEvent(eventType,
		 	        /* flags */     eventFlags,
			        /* keyCode */   keyCode,
			        /* specialty */ i);
		/*
		 * Special keys hack for letting an arbitrary (non-locking)
		 * key act as a CAPS-LOCK key.  If a special CAPS LOCK key
		 * is designated, and there is no key designated for the 
		 * AlphaLock function, then we'll let the special key toggle
		 * the AlphaLock state.
		 */
		if (i == NX_KEYTYPE_CAPS_LOCK
		    && down == true
		    && !_parsedMapping.modDefs[NX_MODIFIERKEY_ALPHALOCK] )
		{
		    unsigned myFlags = _delegate->deviceFlags();
		    bool alphaLock = (_delegate->alphaLock() == false);
//		    bool alphaLock = down;

		    // Set delegate's alphaLock state
		    _delegate->setAlphaLock(alphaLock);
		    // Update the delegate's flags
		    if ( alphaLock )
		    	myFlags |= NX_ALPHASHIFTMASK;
		    else
		        myFlags &= ~NX_ALPHASHIFTMASK;
		    _delegate->setDeviceFlags(myFlags);
		    _delegate->keyboardEvent(NX_FLAGSCHANGED,
		     /* flags */            myFlags,
		     /* keyCode */          keyCode,
		     /* charCode */         0,
		     /* charSet */          0,
		     /* originalCharCode */ 0,
		     /* originalCharSet */  0);
		} 
		break;
	    }
	}
    }
}
