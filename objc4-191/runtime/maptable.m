/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*	maptable.m
  	Copyright 1990-1996 NeXT Software, Inc.
	Created by Bertrand Serlet, August 1990
 */

#if defined(WIN32)
    #include <winnt-pdo.h>
#endif

#import "objc-private.h"
#import "maptable.h"

#import <string.h>
#import <stdlib.h>
#import <stdio.h>
#import <objc/Object.h>
#import <objc/hashtable2.h>

#if defined(NeXT_PDO)
    #import <pdo.h>
#endif

/******		Macros and utilities	****************************/

#if defined(DEBUG)
    #define INLINE	
#else
    #define INLINE inline
#endif

typedef struct _MapPair {
    const void	*key;
    const void	*value;
} MapPair;

static unsigned log2(unsigned x) { return (x<2) ? 0 : log2(x>>1)+1; };

static INLINE unsigned exp2m1(unsigned x) { return (1 << x) - 1; };

/* iff necessary this modulo can be optimized since the nbBuckets is of the form 2**n-1 */
static INLINE unsigned bucketOf(NXMapTable *table, const void *key) {
    unsigned	hash = (table->prototype->hash)(table, key);
    unsigned	xored = (hash & 0xffff) ^ (hash >> 16);
    return ((xored * 65521) + hash) % table->nbBuckets;
}

static INLINE int isEqual(NXMapTable *table, const void *key1, const void *key2) {
    return (key1 == key2) ? 1 : (table->prototype->isEqual)(table, key1, key2);
}

static INLINE unsigned nextIndex(NXMapTable *table, unsigned index) {
    return (index+1 >= table->nbBuckets) ? 0 : index+1;
}

static INLINE void *allocBuckets(NXZone *zone, unsigned nb) {
    MapPair	*pairs = NXZoneMalloc(zone, (nb * sizeof(MapPair)));
    MapPair	*pair = pairs;
    while (nb--) { pair->key = NX_MAPNOTAKEY; pair->value = NULL; pair++; }
    return pairs;
}

/*****		Global data and bootstrap	**********************/

static int isEqualPrototype (const void *info, const void *data1, const void *data2) {
    NXHashTablePrototype        *proto1 = (NXHashTablePrototype *) data1;
    NXHashTablePrototype        *proto2 = (NXHashTablePrototype *) data2;

    return (proto1->hash == proto2->hash) && (proto1->isEqual == proto2->isEqual) && (proto1->free == proto2->free) && (proto1->style == proto2->style);
    };

static uarith_t hashPrototype (const void *info, const void *data) {
    NXHashTablePrototype        *proto = (NXHashTablePrototype *) data;

    return NXPtrHash(info, proto->hash) ^ NXPtrHash(info, proto->isEqual) ^ NXPtrHash(info, proto->free) ^ (uarith_t) proto->style;
    };

static NXHashTablePrototype protoPrototype = {
    hashPrototype, isEqualPrototype, NXNoEffectFree, 0
};

static NXHashTable *prototypes = NULL;
	/* table of all prototypes */

/****		Fundamentals Operations			**************/

NXMapTable *NXCreateMapTableFromZone(NXMapTablePrototype prototype, unsigned capacity, NXZone *zone) {
    NXMapTable			*table = NXZoneMalloc(zone, sizeof(NXMapTable));
    NXMapTablePrototype		*proto;
    if (! prototypes) prototypes = NXCreateHashTable(protoPrototype, 0, NULL);
    if (! prototype.hash || ! prototype.isEqual || ! prototype.free || prototype.style) {
	_NXLogError("*** NXCreateMapTable: invalid creation parameters\n");
	return NULL;
    }
    proto = NXHashGet(prototypes, &prototype); 
    if (! proto) {
	proto = malloc(sizeof(NXMapTablePrototype));
	*proto = prototype;
    	(void)NXHashInsert(prototypes, proto);
    }
    table->prototype = proto; table->count = 0;
    table->nbBuckets = exp2m1(log2(capacity)+1);
    table->buckets = allocBuckets(zone, table->nbBuckets);
    return table;
}

NXMapTable *NXCreateMapTable(NXMapTablePrototype prototype, unsigned capacity) {
    return NXCreateMapTableFromZone(prototype, capacity, NXDefaultMallocZone());
}

void NXFreeMapTable(NXMapTable *table) {
    NXResetMapTable(table);
    free(table->buckets);
    free(table);
}

void NXResetMapTable(NXMapTable *table) {
    MapPair	*pairs = table->buckets;
    void	(*freeProc)(struct _NXMapTable *, void *, void *) = table->prototype->free;
    unsigned	index = table->nbBuckets;
    while (index--) {
	if (pairs->key != NX_MAPNOTAKEY) {
	    freeProc(table, (void *)pairs->key, (void *)pairs->value);
	    pairs->key = NX_MAPNOTAKEY; pairs->value = NULL;
	}
	pairs++;
    }
    table->count = 0;
}

BOOL NXCompareMapTables(NXMapTable *table1, NXMapTable *table2) {
    if (table1 == table2) return YES;
    if (table1->count != table2->count) return NO;
    else {
	const void *key;
	const void *value;
	NXMapState	state = NXInitMapState(table1);
	while (NXNextMapState(table1, &state, &key, &value)) {
	    if (NXMapMember(table2, key, (void**)&value) == NX_MAPNOTAKEY) return NO;
	}
	return YES;
    }
}

unsigned NXCountMapTable(NXMapTable *table) { return table->count; }

static int mapSearch = 0;
static int mapSearchHit = 0;
static int mapSearchLoop = 0;

static INLINE void *_NXMapMember(NXMapTable *table, const void *key, void **value) {
    MapPair	*pairs = table->buckets;
    unsigned	index = bucketOf(table, key);
    MapPair	*pair = pairs + index;
    if (pair->key == NX_MAPNOTAKEY) return NX_MAPNOTAKEY;
    mapSearch ++;
    if (isEqual(table, pair->key, key)) {
	*value = (void *)pair->value;
	mapSearchHit ++;
	return (void *)pair->key;
    } else {
	unsigned	index2 = index;
	while ((index2 = nextIndex(table, index2)) != index) {
	    mapSearchLoop ++;
	    pair = pairs + index2;
	    if (pair->key == NX_MAPNOTAKEY) return NX_MAPNOTAKEY;
	    if (isEqual(table, pair->key, key)) {
	    	*value = (void *)pair->value;
		return (void *)pair->key;
	    }
	}
	return NX_MAPNOTAKEY;
    }
}

void *NXMapMember(NXMapTable *table, const void *key, void **value) {
    return _NXMapMember(table, key, value);
}

void *NXMapGet(NXMapTable *table, const void *key) {
    void	*value;
    return (_NXMapMember(table, key, &value) != NX_MAPNOTAKEY) ? value : NULL;
}

static int mapRehash = 0;
static int mapRehashSum = 0;

static void _NXMapRehash(NXMapTable *table) {
    MapPair	*pairs = table->buckets;
    MapPair	*pair = pairs;
    unsigned	index = table->nbBuckets;
    unsigned	oldCount = table->count;
    table->nbBuckets += table->nbBuckets + 1; /* 2 times + 1 */
    table->count = 0; 
    table->buckets = allocBuckets(NXZoneFromPtr(table), table->nbBuckets);
    mapRehash ++;
    mapRehashSum += table->count;
    while (index--) {
	if (pair->key != NX_MAPNOTAKEY) {
	    (void)NXMapInsert(table, pair->key, pair->value);
	}
	pair++;
    }
    if (oldCount != table->count)
	_NXLogError("*** maptable: count differs after rehashing; probably indicates a broken invariant: there are x and y such as isEqual(x, y) is TRUE but hash(x) != hash (y)\n");
    free(pairs); 
}

static int mapInsert = 0;
static int mapInsertHit = 0;
static int mapInsertLoop = 0;

void *NXMapInsert(NXMapTable *table, const void *key, const void *value) {
    MapPair	*pairs = table->buckets;
    unsigned	index = bucketOf(table, key);
    MapPair	*pair = pairs + index;
    if (key == NX_MAPNOTAKEY) {
	_NXLogError("*** NXMapInsert: invalid key: -1\n");
	return NULL;
    }
    mapInsert ++;
    if (pair->key == NX_MAPNOTAKEY) {
	mapInsertHit ++;
	pair->key = key; pair->value = value;
	table->count++;
	return NULL;
    }
    if (isEqual(table, pair->key, key)) {
	const void	*old = pair->value;
	mapInsertHit ++;
	if (old != value) pair->value = value;/* avoid writing unless needed! */
	return (void *)old;
    } else if (table->count == table->nbBuckets) {
	/* no room: rehash and retry */
	_NXMapRehash(table);
	return NXMapInsert(table, key, value);
    } else {
	unsigned	index2 = index;
	while ((index2 = nextIndex(table, index2)) != index) {
	    mapInsertLoop ++;
	    pair = pairs + index2;
	    if (pair->key == NX_MAPNOTAKEY) {
    #if INSERT_TAIL
              pair->key = key; pair->value = value;
    #else
              MapPair         current = {key, value};
              index2 = index;
              while (current.key != NX_MAPNOTAKEY) {
                  MapPair             temp;
                  pair = pairs + index2;
                  temp = *pair;
                  *pair = current;
                  current = temp;
                  index2 = nextIndex(table, index2);
              }
    #endif
		table->count++;
		if (table->count * 4 > table->nbBuckets * 3) _NXMapRehash(table);
		return NULL;
	    }
	    if (isEqual(table, pair->key, key)) {
		const void	*old = pair->value;
		if (old != value) pair->value = value;/* avoid writing unless needed! */
		return (void *)old;
	    }
	}
	/* no room: can't happen! */
	_NXLogError("**** NXMapInsert: bug\n");
	return NULL;
    }
}

static int mapRemove = 0;

void *NXMapRemove(NXMapTable *table, const void *key) {
    MapPair	*pairs = table->buckets;
    unsigned	index = bucketOf(table, key);
    MapPair	*pair = pairs + index;
    unsigned	chain = 1; /* number of non-nil pairs in a row */
    int		found = 0;
    const void	*old = NULL;
    if (pair->key == NX_MAPNOTAKEY) return NULL;
    mapRemove ++;
    /* compute chain */
    {
	unsigned	index2 = index;
	if (isEqual(table, pair->key, key)) {found ++; old = pair->value; }
	while ((index2 = nextIndex(table, index2)) != index) {
	    pair = pairs + index2;
	    if (pair->key == NX_MAPNOTAKEY) break;
	    if (isEqual(table, pair->key, key)) {found ++; old = pair->value; }
	    chain++;
	}
    }
    if (! found) return NULL;
    if (found != 1) _NXLogError("**** NXMapRemove: incorrect table\n");
    /* remove then reinsert */
    {
	MapPair	buffer[16];
	MapPair	*aux = (chain > 16) ? malloc(sizeof(MapPair)*(chain-1)) : buffer;
	int	auxnb = 0;
	int	nb = chain;
	unsigned	index2 = index;
	while (nb--) {
	    pair = pairs + index2;
	    if (! isEqual(table, pair->key, key)) aux[auxnb++] = *pair;
	    pair->key = NX_MAPNOTAKEY; pair->value = NULL;
	    index2 = nextIndex(table, index2);
	}
	table->count -= chain;
	if (auxnb != chain-1) _NXLogError("**** NXMapRemove: bug\n");
	while (auxnb--) NXMapInsert(table, aux[auxnb].key, aux[auxnb].value);
	if (chain > 16) free(aux);
    }
    return (void *)old;
}

NXMapState NXInitMapState(NXMapTable *table) {
    NXMapState	state;
    state.index = table->nbBuckets;
    return state;
}
    
int NXNextMapState(NXMapTable *table, NXMapState *state, const void **key, const void **value) {
    MapPair	*pairs = table->buckets;
    while (state->index--) {
	MapPair	*pair = pairs + state->index;
	if (pair->key != NX_MAPNOTAKEY) {
	    *key = pair->key; *value = pair->value;
	    return YES;
	}
    }
    return NO;
}

/****		Conveniences		*************************************/

static unsigned _mapPtrHash(NXMapTable *table, const void *key) {
    return (((uarith_t) key) >> ARITH_SHIFT) ^ ((uarith_t) key);
}
    
static unsigned _mapStrHash(NXMapTable *table, const void *key) {
    unsigned		hash = 0;
    unsigned char	*s = (unsigned char *)key;
    /* unsigned to avoid a sign-extend */
    /* unroll the loop */
    if (s) for (; ; ) { 
	if (*s == '\0') break;
	hash ^= *s++;
	if (*s == '\0') break;
	hash ^= *s++ << 8;
	if (*s == '\0') break;
	hash ^= *s++ << 16;
	if (*s == '\0') break;
	hash ^= *s++ << 24;
    }
    return hash;
}
    
static unsigned _mapObjectHash(NXMapTable *table, const void *key) {
    return [(id)key hash];
}
    
static int _mapPtrIsEqual(NXMapTable *table, const void *key1, const void *key2) {
    return key1 == key2;
}

static int _mapStrIsEqual(NXMapTable *table, const void *key1, const void *key2) {
    if (key1 == key2) return YES;
    if (! key1) return ! strlen ((char *) key2);
    if (! key2) return ! strlen ((char *) key1);
    if (((char *) key1)[0] != ((char *) key2)[0]) return NO;
    return (strcmp((char *) key1, (char *) key2)) ? NO : YES;
}
    
static int _mapObjectIsEqual(NXMapTable *table, const void *key1, const void *key2) {
    return [(id)key1 isEqual:(id)key2];
}
    
static void _mapNoFree(NXMapTable *table, void *key, void *value) {}

static void _mapObjectFree(NXMapTable *table, void *key, void *value) {
    [(id)key free];
}

const NXMapTablePrototype NXPtrValueMapPrototype = {
    _mapPtrHash, _mapPtrIsEqual, _mapNoFree, 0
};

const NXMapTablePrototype NXStrValueMapPrototype = {
    _mapStrHash, _mapStrIsEqual, _mapNoFree, 0
};

const NXMapTablePrototype NXObjectMapPrototype = {
    _mapObjectHash, _mapObjectIsEqual, _mapObjectFree, 0
};

