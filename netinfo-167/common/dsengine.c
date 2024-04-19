/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "dsengine.h"
#include "dsutil.h"
#include "dsindex.h"

/*
 * Creates and opens a new data store with the given pathname.
 */
dsstatus
dsengine_new(dsengine **s, char *name, u_int32_t flags)
{
	dsstatus status;
	dsstore *x;
	dsrecord *r;

	if (name == NULL) return DSStatusInvalidStore;

	status = dsstore_new(&x, name, flags);
	if (status != DSStatusOK) return status;

	*s = (dsengine *)malloc(sizeof(dsengine));
	(*s)->store = x;

	/* create root if necessary */
	r = dsstore_fetch(x, 0);
	if (r == NULL)
	{
		r = dsrecord_new();
		r->super = 0;

		status = dsstore_save((*s)->store, r);
	}
	
	dsrecord_release(r);
	return status;
}

/*
 * Opens a data store with the given pathname.
 */
dsstatus
dsengine_open(dsengine **s, char *name, u_int32_t flags)
{
	dsstatus status;
	dsstore *x;

	if (name == NULL) return DSStatusInvalidStore;

	status = dsstore_open(&x, name, flags);
	if (status != DSStatusOK) return status;

	*s = (dsengine *)malloc(sizeof(dsengine));
	(*s)->store = x;

	return status;
}

/*
 * Closes the data store.
 */
dsstatus
dsengine_close(dsengine *s)
{
	if (s == NULL) return DSStatusOK;
	if (s->store == NULL) return DSStatusInvalidStore;

	dsstore_close(s->store);
	free(s);

	return DSStatusOK;
}

/*
 * Autheticate.
 */
dsstatus
dsengine_authenticate(dsengine *s, dsdata *user, dsdata *password)
{
	if (s == NULL) return DSStatusOK;
	if (s->store == NULL) return DSStatusInvalidStore;
	return dsstore_authenticate(s->store, user, password);
}

/*
 * Detach a child (specified by its ID) from a record.
 * This just edits the record's list of children, and does not remove the
 * child record from the data store.  You should never do this!  This
 * routine is provided to allow emergency repairs to a corrupt data store.
 */
dsstatus
dsengine_detach(dsengine *s, dsrecord *r, u_int32_t dsid)
{
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;
	if (r == NULL) return DSStatusInvalidRecord;

	/* Remove child's dsid */
	dsrecord_remove_sub(r, dsid);

	/* Reset parent's index */
	dsindex_free(r->index);
	r->index = NULL;

	status = dsstore_save(s->store, r);

	return status;
}

/*
 * Attach a child (specified by its ID) to a record.
 * This just edits the record's list of children, and does not create the
 * child record in the data store.  You should never do this!  This
 * routine is provided to allow emergency repairs to a corrupt data store.
 */
dsstatus
dsengine_attach(dsengine *s, dsrecord *r, u_int32_t dsid)
{
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;
	if (r == NULL) return DSStatusInvalidRecord;

	/* Add child's dsid */
	dsrecord_append_sub(r, dsid);

	/* Reset parent's index */
	dsindex_free(r->index);
	r->index = NULL;

	status = dsstore_save(s->store, r);

	return status;
}

/*
 * Sets a record's parent.  This is for emergency repairs.
 */
dsstatus
dsengine_set_parent(dsengine *s, dsrecord *r, u_int32_t dsid)
{
	dsstatus status;
	dsrecord *parent;

	if (s == NULL) return DSStatusInvalidStore;
	if (r == NULL) return DSStatusInvalidRecord;

	parent = dsstore_fetch(s->store, dsid);
	if (parent == NULL) return DSStatusInvalidRecord;

	/* Reset parent's index */
	dsindex_free(parent->index);
	parent->index = NULL;

	dsrecord_release(parent);

	r->super = dsid;
	status = dsstore_save(s->store, r);

	return status;
}

/*
 * Adds a new record as a child of another record (specified by its ID).
 */
dsstatus
dsengine_create(dsengine *s, dsrecord *r, u_int32_t dsid)
{
	dsrecord *parent;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;
	if (r == NULL) return DSStatusInvalidRecord;

	parent = dsstore_fetch(s->store, dsid);
	if (parent == NULL) return DSStatusInvalidPath;

	r->super = dsid;

	status = dsstore_save(s->store, r);
	if (status != DSStatusOK)
	{
		dsrecord_release(parent);
		return status;
	}

	dsrecord_append_sub(parent, r->dsid);
	dsindex_insert_record(parent->index, r);

	status = dsstore_save(s->store, parent);
	dsrecord_release(parent);

	return status;
}

/*
 * Fetch a record specified by its ID.
 */
dsstatus
dsengine_fetch(dsengine *s, u_int32_t dsid, dsrecord **r)
{
	if (s == NULL) return DSStatusInvalidStore;

	*r = dsstore_fetch(s->store, dsid);
	if (*r == NULL) return DSStatusInvalidPath;
	return DSStatusOK;
}

/*
 * Fetch a list of records specified by ID.
 */
dsstatus
dsengine_fetch_list(dsengine *s, u_int32_t count, u_int32_t *dsid,
	dsrecord ***l)
{
	dsstatus status;
	u_int32_t i;
	dsrecord **list;

	if (s == NULL) return DSStatusInvalidStore;

	if ((count == 0) || (dsid == NULL))
	{
		*l = NULL;
		return DSStatusOK;
	}

	list = (dsrecord **)malloc(count * sizeof(dsrecord *));
	*l = list;

	for (i = 0; i < count; i++)
	{
		status = dsengine_fetch(s, dsid[i], &(list[i]));
		if (status != DSStatusOK)
		{
			free(list);
			*l = NULL;
			return status;
		}
	}

	return DSStatusOK;
}

/*
 * Save a record.
 */
dsstatus
dsengine_save(dsengine *s, dsrecord *r)
{
	u_int32_t i;
	dsrecord *parent;

	if (s == NULL) return DSStatusInvalidStore;

	i = dsstore_record_super(s->store, r->dsid);
	if (i == IndexNull) return DSStatusInvalidPath;

	parent = dsstore_fetch(s->store, i);
	if (parent == NULL)
	{
		/* XXX Parent doesn't exist! */
		return DSStatusInvalidPath;
	}

	/* Update child in parent's index */
	dsindex_delete_dsid(parent->index, r->dsid);
	dsindex_insert_record(parent->index, r);

	dsrecord_release(parent);
	return dsstore_save(s->store, r);
}

/*
 * Modify a record attribute. 
 */
dsstatus
dsengine_save_attribute(dsengine *s, dsrecord *r, dsattribute *a, u_int32_t asel)
{
	u_int32_t i;
	dsrecord *parent;

	if (s == NULL) return DSStatusInvalidStore;

	i = dsstore_record_super(s->store, r->dsid);
	if (i == IndexNull) return DSStatusInvalidPath;

	parent = dsstore_fetch(s->store, i);
	if (parent == NULL)
	{
		/* XXX Parent doesn't exist! */
		return DSStatusInvalidPath;
	}

	/* Update child in parent's index */
	dsindex_delete_dsid(parent->index, r->dsid);
	dsindex_insert_record(parent->index, r);

	dsrecord_release(parent);
	return dsstore_save_attribute(s->store, r, a, asel);
}

/*
 * Remove a record from the data store.
 */
dsstatus
dsengine_remove(dsengine *s, u_int32_t dsid)
{
	u_int32_t i;
	dsrecord *parent;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;

	i = dsstore_record_super(s->store, dsid);
	if (i == IndexNull) return DSStatusInvalidPath;

	parent = dsstore_fetch(s->store, i);
	if (parent == NULL)
	{
		/* XXX Parent doesn't exist! */
		return DSStatusInvalidPath;
	}

	/* Remove the record from the parent's sub list */
	for (i = 0; i < parent->sub_count; i++)
		if (parent->sub[i] == dsid) break;
	
	if (i >= parent->sub_count)
	{
		/* XXX Parent doesn't know child! */
	}

	i++;
	for (; i < parent->sub_count; i++)
		parent->sub[i-1] = parent->sub[i];
		
	parent->sub_count--;
	parent->sub = (u_int32_t *)realloc(parent->sub, parent->sub_count * sizeof(u_int32_t));

	/* Remove child from parent's index */
	dsindex_delete_dsid(parent->index, dsid);

	status = dsstore_save(s->store, parent);
	dsrecord_release(parent);
	if (status != DSStatusOK)
	{
		/* XXX can't save changes to parent! */
		return status;
	}

	status = dsstore_remove(s->store, dsid);
	return status;
}

/*
 * Move a record to a new parent.
 */
dsstatus
dsengine_move(dsengine *s, u_int32_t dsid, u_int32_t pdsid)
{
	u_int32_t old;
	dsrecord *r, *child;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;
	if (dsid == 0) return DSStatusInvalidRecordID;
	
	child = dsstore_fetch(s->store, dsid);
	if (child == NULL) return DSStatusInvalidPath;
	if (child->super == pdsid)
	{
		dsrecord_release(child);
		return DSStatusOK;
	}

	old = child->super;
	child->super = pdsid;
	status = dsstore_save(s->store, child);
	if (status != DSStatusOK)
	{
		/* XXX can't save changes to record! */
		dsrecord_release(child);
		return status;
	}

	r = dsstore_fetch(s->store, old);
	if (r == NULL)
	{
		dsrecord_release(child);
		return DSStatusInvalidPath;
	}

	dsrecord_remove_sub(r, dsid);

	/* Remove child from original parent's index */
	dsindex_delete_dsid(r->index, dsid);

	status = dsstore_save(s->store, r);
	dsrecord_release(r);
	if (status != DSStatusOK)
	{
		/* XXX can't save changes to original parent! */
		dsrecord_release(child);
		return status;
	}

	r = dsstore_fetch(s->store, pdsid);
	if (r == NULL) return DSStatusInvalidPath;

	dsrecord_append_sub(r, dsid);

	/* Add child to new parent's index */
	dsindex_insert_record(r->index, child);
	dsrecord_release(child);

	status = dsstore_save(s->store, r);
	dsrecord_release(r);
	if (status != DSStatusOK)
	{
		/* XXX can't save changes to original parent! */
		return status;
	}

	return DSStatusOK;
}
	
/*
 * Copy a record to a new parent.
 */
dsstatus
dsengine_copy(dsengine *s, u_int32_t dsid, u_int32_t pdsid)
{
	dsrecord *r, *x;
	u_int32_t *kids, i, count;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;
	
	r = dsstore_fetch(s->store, dsid);
	if (r == NULL) return DSStatusInvalidPath;

	x = dsrecord_copy(r);
	dsrecord_release(r);

	count = x->sub_count;
	kids = x->sub;
	x->sub_count = 0;
	x->sub = NULL;

	status = dsengine_create(s, x, pdsid);
	if (status != DSStatusOK)
	{
		/* XXX Can't add new record! */
		dsrecord_release(x);
		return status;
	}
		
	for (i = 0; i < count; i++)
	{
		status = dsengine_copy(s, kids[i], x->dsid);
		if (status != DSStatusOK)
		{
			/* XXX Can't add child! */
			dsrecord_release(x);
			return status;
		}
	}

	return DSStatusOK;
}

/*
 * Warning! count is overloaded - if we are searching for the
 * first match (dsengine_find_pattern()) then count is the output parameter
 * returning the dsid of the matching record.
 */
static dsstatus
_pattern_searcher(dsengine *s, u_int32_t dsid, dsrecord *pattern, u_int32_t scopemin, u_int32_t scopemax, u_int32_t **match, u_int32_t *count, u_int32_t findall)
{
	dsrecord *r;
	u_int32_t i, x;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;

	r = dsstore_fetch(s->store, dsid);
	if (r == NULL) return DSStatusInvalidPath;
	
	if (findall == 0) *count = (u_int32_t)-1;

	if (scopemin == 0)
	{
		if (dsrecord_match(r, pattern) == 1)
		{
			if (findall == 0)
			{
				*count = dsid;
				dsrecord_release(r);
				return DSStatusOK;
			}

			if (*count == 0) *match = (u_int32_t *)malloc(sizeof(u_int32_t));
			else *match = (u_int32_t *)realloc(*match, (1 + *count) * sizeof(u_int32_t));
			(*match)[*count] = dsid;
			*count = *count + 1;
		}
	}
	else scopemin--;

	if (scopemax == 0)
	{
		dsrecord_release(r);
		return DSStatusOK;
	}

	x = scopemax - 1;
	if (scopemax == (u_int32_t)-1) x = scopemax;

	for (i = 0; i < r->sub_count; i++)
	{
		status = _pattern_searcher(s, r->sub[i], pattern, scopemin, x, match, count, findall);
		if (status  != DSStatusOK)
		{
			dsrecord_release(r);
			return status;
		}

		if ((findall == 0) && (*count != (u_int32_t)-1)) break;
	}

	dsrecord_release(r);
	return DSStatusOK;
}

/*
 * Search starting at a given record (specified by ID dsid) for all
 * records matching the attributes of the "pattern" record.
 * Scope is n for n-level deep search. Use (u-int32_t)-1 for unlimited
 * depth.  Scope 0 tests the given node against the pattern.
 * Search with null pattern or null attributes matchs all.
 * Search with null pattern or attributes and scope 1 returns all child IDs.
 */
dsstatus
dsengine_search_pattern(dsengine *s, u_int32_t dsid, dsrecord *pattern, u_int32_t scopemin, u_int32_t scopemax, u_int32_t **match, u_int32_t *count)
{
	*count = 0;	
	return _pattern_searcher(s, dsid, pattern, scopemin, scopemax, match, count, 1);
}

/*
 * Find first match.
 */
dsstatus
dsengine_find_pattern(dsengine *s, u_int32_t dsid, dsrecord *pattern, u_int32_t scopemin, u_int32_t scopemax, u_int32_t *match)
{
	return _pattern_searcher(s, dsid, pattern, scopemin, scopemax, NULL, match, 0);
}

/*
 * Warning! count is overloaded - if we are searching for the
 * first match (dsengine_find_pattern()) then count is the output parameter
 * returning the dsid of the matching record.
 */
static dsstatus
_filter_searcher(dsengine *s, u_int32_t dsid, dsfilter *f, u_int32_t scopemin, u_int32_t scopemax, u_int32_t **match, u_int32_t *count, u_int32_t findall)
{
	dsrecord *r;
	u_int32_t i, x;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;

	r = dsstore_fetch(s->store, dsid);
	if (r == NULL) return DSStatusInvalidPath;
	
	if (findall == 0) *count = (u_int32_t)-1;

	if (scopemin == 0)
	{
		if (dsfilter_test(f, r) == L3True)
		{
			if (findall == 0)
			{
				*count = dsid;
				dsrecord_release(r);
				return DSStatusOK;
			}

			if (*count == 0) *match = (u_int32_t *)malloc(sizeof(u_int32_t));
			else *match = (u_int32_t *)realloc(*match, (1 + *count) * sizeof(u_int32_t));
			(*match)[*count] = dsid;
			*count = *count + 1;
		}
	}
	else scopemin--;

	if (scopemax == 0)
	{
		dsrecord_release(r);
		return DSStatusOK;
	}

	x = scopemax - 1;
	if (scopemax == (u_int32_t)-1) x = scopemax;

	for (i = 0; i < r->sub_count; i++)
	{
		status = _filter_searcher(s, r->sub[i], f, scopemin, x, match, count, findall);
		if (status  != DSStatusOK)
		{
			dsrecord_release(r);
			return status;
		}

		if ((findall == 0) && (*count != (u_int32_t)-1)) break;
	}

	dsrecord_release(r);
	return DSStatusOK;
}

dsstatus
dsengine_search_filter(dsengine *s, u_int32_t dsid, dsfilter *f, u_int32_t scopemin, u_int32_t scopemax, u_int32_t **match, u_int32_t *count)
{
	*count = 0;	
	return _filter_searcher(s, dsid, f, scopemin, scopemax, match, count, 1);
}

/*
 * Find first match.
 */
dsstatus
dsengine_find_filter(dsengine *s, u_int32_t dsid, dsfilter *f, u_int32_t scopemin, u_int32_t scopemax, u_int32_t *match)
{
	return _filter_searcher(s, dsid, f, scopemin, scopemax, NULL, match, 0);
}

/*
 * Find the first child record of a given record that has an attribute with "key" and "val".
 */
dsstatus
dsengine_match(dsengine *s, u_int32_t dsid, dsdata *key, dsdata *val, u_int32_t *match)
{
	return dsstore_match(s->store, dsid, key, val, SELECT_ATTRIBUTE, match);
}

static dsstatus
dsengine_pathutil(dsengine *s, u_int32_t dsid, dsrecord *path, u_int32_t *match, u_int32_t create)
{
	u_int32_t i, n, c;
	dsstatus status;
	dsattribute *a;
	dsdata *k, *v;
	dsrecord *r;
	dsdata *keyname, *dot, *dotdot;

	*match = (u_int32_t)-1;

	if (s == NULL) return DSStatusInvalidStore;

	if (path == NULL)
	{
		*match = dsid;
		return DSStatusOK;
	}
	
	/* Special cases: */
	/* key="name" val="."  matches this  record. */
	/* key="name" val=".." matches super record. */
	keyname = cstring_to_dsdata("name");
	dot = cstring_to_dsdata(".");
	dotdot = cstring_to_dsdata("..");

	n = dsid;
	for (i = 0; i < path->count; i++)
	{
		a = path->attribute[i];
		k = a->key;
		v = NULL;

		if (a->count > 0) v = a->value[0];

		if (dsdata_equal(k, keyname))
		{
			if (dsdata_equal(v, dot)) continue;
			if (dsdata_equal(v, dotdot))
			{
				r = dsstore_fetch(s->store, n);
				if (r == NULL)
				{
					dsdata_release(keyname);
					dsdata_release(dot);
					dsdata_release(dotdot);
					return DSStatusInvalidPath;
				}
				n = r->super;
				dsrecord_release(r);
				continue;
			}
		}

		status = dsengine_match(s, n, k, v, &c);
		if (status != DSStatusOK)
		{
			dsdata_release(keyname);
			dsdata_release(dot);
			dsdata_release(dotdot);
			return status;
		}

		if (c == (u_int32_t)-1)
		{
			if (create == 0)
			{
				dsdata_release(keyname);
				dsdata_release(dot);
				dsdata_release(dotdot);
				return DSStatusInvalidPath;
			}
			else
			{
				/* Create the path component */
				r = dsrecord_new();
				dsrecord_append_attribute(r, a, SELECT_ATTRIBUTE);
				status = dsengine_create(s, r, n);
				c = r->dsid;
				dsrecord_release(r);
				if (status != DSStatusOK)
				{
					dsdata_release(keyname);
					dsdata_release(dot);
					dsdata_release(dotdot);
					return status;
				}
			}
		}
		n = c;
	}

	*match = n;

	dsdata_release(keyname);
	dsdata_release(dot);
	dsdata_release(dotdot);

	return DSStatusOK;
}

/*
 * Returns a list of dsids, representing the path the given record to root.
 * The final dsid in the list will always be 0 (root).
 */
dsstatus
dsengine_path(dsengine *s, u_int32_t dsid, u_int32_t **list)
{
	u_int32_t i, n;

	i = dsid;

	*list = (u_int32_t *)malloc(sizeof(u_int32_t));
	(*list)[0] = i;
	n = 1;

	while (i != 0)
	{
		i = dsstore_record_super(s->store, i);
		if (i == IndexNull) return DSStatusReadFailed;
	
		*list = (u_int32_t *)realloc(*list, (n + 1) * sizeof(u_int32_t));
		(*list)[n] = i;
		n++;
	}

	return DSStatusOK;
}

/*
 * Find a record following a list of key=value pairs, which are given as
 * the attributes of a "path" record.
 */
dsstatus
dsengine_pathmatch(dsengine *s, u_int32_t dsid, dsrecord *path, u_int32_t *match)
{
	return dsengine_pathutil(s, dsid, path, match, 0);
}

/*
 * Create a path following a list of key=value pairs, which are given as
 * the attributes of a "path" record.  Returns dsid of last directory in the
 * chain of created directories.  Follows existing directories if they exist.
 */
dsstatus
dsengine_pathcreate(dsengine *s, u_int32_t dsid, dsrecord *path, u_int32_t *match)
{
	return dsengine_pathutil(s, dsid, path, match, 1);
}

/*
 * Returns a list of dsids and values for a given attribute key.
 * Results are returned in a dsrecord.  Keys are dsids encoded using
 * int32_to_dsdata().  Values are attribute values for the
 * corresponding record.
 */
dsstatus
dsengine_list(dsengine *s, u_int32_t dsid, dsdata *key, u_int32_t scopemin, u_int32_t scopemax, dsrecord **list)
{
	dsrecord *r;
	dsattribute *a;
	u_int32_t i, j, *matches, len, x;
	dsdata *n;
	dsstatus status;
	
	if (s == NULL) return DSStatusInvalidStore;
	if (key == NULL) return DSStatusInvalidKey;

	r = dsrecord_new();
	a = dsattribute_new(key);
	dsrecord_append_attribute(r, a, SELECT_ATTRIBUTE);
	dsattribute_release(a);

	status = dsengine_search_pattern(s, dsid, r, scopemin, scopemax, &matches, &len);
	dsrecord_release(r);
	if (status != DSStatusOK) return status;

	*list = dsrecord_new();
	n = cstring_to_dsdata("key");
	a = dsattribute_new(n);
	dsattribute_append(a, key);
	dsrecord_append_attribute(*list, a, SELECT_META_ATTRIBUTE);
	dsdata_release(n);
	dsattribute_release(a);

	for (i = 0; i < len; i++)
	{
		status = dsengine_fetch(s, matches[i], &r);
		if (status != DSStatusOK)
		{
			dsrecord_release(*list);
			return status;
		}

		x = dsrecord_attribute_index(r, key, SELECT_ATTRIBUTE);
		if (x == IndexNull)
		{
			dsrecord_release(r);
			continue;
		}

		n = int32_to_dsdata(r->dsid);
		a = dsattribute_new(n);
		dsdata_release(n);

		a->count = r->attribute[x]->count;
		a->value = (dsdata **)malloc(a->count * sizeof(dsdata *));
		for (j = 0; j < a->count; j++)
			a->value[j] = dsdata_retain(r->attribute[x]->value[j]);

		dsrecord_append_attribute(*list, a, SELECT_ATTRIBUTE);
		dsrecord_release(r);
	}

	if (len > 0) free(matches);
	
	return DSStatusOK;
}

dsstatus
dsengine_netinfo_string_pathmatch(dsengine *s, u_int32_t dsid, char *path, u_int32_t *match)
{
	dsrecord *p;
	u_int32_t i;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;

	*match = dsid;

	if (path == NULL) return DSStatusOK;
	
	if ((path[0] >= '0') && (path[0] <= '9'))
	{
		i = atoi(path);
		if ((i == 0) && (strcmp(path, "0")))
		{
			*match = (u_int32_t)-1;
			return DSStatusInvalidRecordID;
		}
		*match = i;
		return DSStatusOK;
	}

	p = dsutil_parse_netinfo_string_path(path);
	if (p == NULL) return DSStatusInvalidPath;

	if (path[0] == '/') *match = 0;
	status = dsengine_pathmatch(s, *match, p, match);
	dsrecord_release(p);

	return status;
}

char *
dsengine_netinfo_string_path(dsengine *s, u_int32_t dsid)
{
	char *p, *path, str[32], *x;
	u_int32_t i, len, plen, dirno;
	dsrecord *r;
	dsattribute *name;
	dsstatus status;
	dsdata *d;

	if (s == NULL) return NULL;

	if (dsid == 0)
	{
		path = malloc(2);
		path[0] = '/';
		path[1] = '\0';
		return path;
	}

	path = NULL;
	plen = 0;
	d = cstring_to_dsdata("name");

	i = dsid;
	while (i != 0)
	{
		dirno = 0;
		r = NULL;
		name = NULL;

		status = dsengine_fetch(s, i, &r);
		if (status != DSStatusOK)
		{
			dirno = 1;
			i = 0;
		}
		else
		{
			i = r->super;
			name = dsrecord_attribute(r, d, SELECT_ATTRIBUTE);
			if (name == NULL) dirno = 1;
			else if (name->count == 0) dirno = 1;
		}

		if (dirno == 1)
		{
			if (r == NULL) sprintf(str, "dir:?");
			else sprintf(str, "dir:%d", r->dsid);
			x = str;
		}
		else
		{
			x = dsdata_to_cstring(name->value[0]);
		}

		len = strlen(x);
		p = malloc(plen + len + 1);
		if (path == NULL)
		{
			sprintf(p, "/%s", x);
		}
		else
		{
			sprintf(p, "/%s%s", x, path);
			free(path);
		}
		
		path = p;
		plen = strlen(path);

		dsattribute_release(name);
		dsrecord_release(r);
	}
	
	dsdata_release(d);

	return path;
}

char *
dsengine_x500_string_path(dsengine *s, u_int32_t dsid)
{
	char *p, *path, str[32], *x;
	u_int32_t i, plen;
	dsrecord *r, *parent;
	dsattribute *rdn_attr, *name;
	dsstatus status;
	dsdata *default_rdn_key, *rdn_key, *name_rdn;

	if (s == NULL) return NULL;
	if (dsid == 0) return NULL;

	path = NULL;
	plen = 0;
	default_rdn_key = cstring_to_dsdata("cn");
	name_rdn = cstring_to_dsdata("rdn");

	i = dsid;
	status = dsengine_fetch(s, i, &r);
	if (status != DSStatusOK) return NULL;

	while (i != 0)
	{
		i = r->super;
		status = dsengine_fetch(s, i, &parent);
		if (status != DSStatusOK)
		{
			dsrecord_release(r);
			return NULL;
		}

		rdn_attr = dsrecord_attribute(parent, name_rdn, SELECT_ATTRIBUTE);
		if (rdn_attr == NULL) rdn_key = dsdata_retain(default_rdn_key);
		else if (rdn_attr->count == 0) rdn_key = dsdata_retain(default_rdn_key);
		else rdn_key = dsdata_retain(rdn_attr->value[0]);
		
		dsattribute_release(rdn_attr);

		x = dsdata_to_cstring(rdn_key);
		if (path == NULL)
		{
			plen = strlen(x) + 1;
			path = malloc(plen + 1);
			sprintf(path, "%s=", x);
		}
		else
		{
			plen = plen + strlen(x) + 3;
			p = malloc(plen + 1);
			sprintf(p, "%s, %s=", path, x);
			free(path);
			path = p;
		}

		name = dsrecord_attribute(r, rdn_key, SELECT_ATTRIBUTE);
		dsrecord_release(r);
		if (name == NULL)
		{
			sprintf(str, "dir:%d", r->dsid);
			x = str;
		}
		else if (name->count == 0)
		{
			sprintf(str, "dir:%d", r->dsid);
			x = str;
		}
		else
		{
			x = dsdata_to_cstring(name->value[0]);
		}

		dsdata_release(rdn_key);
		dsattribute_release(name);
		
		plen = plen + strlen(x);
		p = malloc(plen + 1);
		sprintf(p, "%s%s", path, x);
		free(path);
		path = p;

		r = parent;
	}
	
	dsdata_release(default_rdn_key);
	dsdata_release(name_rdn);

	return path;
}


/*
 * Get a record's parent dsid.
 */
dsstatus
dsengine_record_super(dsengine *s, u_int32_t dsid, u_int32_t *super)
{
	if (s == NULL) return DSStatusInvalidStore;

	*super = dsstore_record_super(s->store, dsid);
	if (*super == IndexNull) return DSStatusInvalidRecordID;
	return DSStatusOK;
}

/*
 * Get a record's version number.
 */
dsstatus dsengine_record_version(dsengine *s, u_int32_t dsid, u_int32_t *version)
{
	if (s == NULL) return DSStatusInvalidStore;

	*version = dsstore_record_version(s->store, dsid);
	if (*version == IndexNull) return DSStatusInvalidRecordID;
	return DSStatusOK;
}

/*
 * Get a record's serial number.
 */
dsstatus dsengine_record_serial(dsengine *s, u_int32_t dsid, u_int32_t *serial)
{
	if (s == NULL) return DSStatusInvalidStore;

	*serial = dsstore_record_serial(s->store, dsid);
	if (*serial == IndexNull) return DSStatusInvalidRecordID;
	return DSStatusOK;
}

/*
 * Get the dsid of the record with a given version number.
 */
dsstatus dsengine_version_record(dsengine *s, u_int32_t version, u_int32_t *dsid)
{
	if (s == NULL) return DSStatusInvalidStore;

	*dsid = dsstore_version_record(s->store, version);
	if (*dsid == IndexNull) return DSStatusInvalidRecordID;
	return DSStatusOK;
}

/*
 * Get data store version number.
 */
dsstatus dsengine_version(dsengine *s, u_int32_t *version)
{
	if (s == NULL) return DSStatusInvalidStore;

	*version = dsstore_version(s->store);
	if (*version == IndexNull) return DSStatusInvalidStore;
	return DSStatusOK;
}

/*
 * Get a record's version number, serial number, and parent's dsid.
 */
dsstatus dsengine_vital_statistics(dsengine *s, u_int32_t dsid, u_int32_t *version, u_int32_t *serial, u_int32_t *super)
{
	if (s == NULL) return DSStatusInvalidStore;
	return dsstore_vital_statistics(s->store, dsid, version, serial, super);
}

void
dsengine_flush_cache(dsengine *s)
{
	if (s == NULL) return;
	dsstore_flush_cache(s->store);
}
