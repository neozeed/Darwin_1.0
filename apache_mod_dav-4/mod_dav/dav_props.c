/*
** Copyright (C) 1998-2000 Greg Stein. All Rights Reserved.
**
** By using this file, you agree to the terms and conditions set forth in
** the LICENSE.html file which can be found at the top level of the mod_dav
** distribution or at http://www.webdav.org/mod_dav/license-1.html.
**
** Contact information:
**   Greg Stein, PO Box 760, Palo Alto, CA, 94302
**   gstein@lyra.org, http://www.webdav.org/mod_dav/
*/

/*
** DAV extension module for Apache 1.3.*
**  - Property database handling (repository-independent)
**
** Written by Greg Stein, gstein@lyra.org, http://www.lyra.org/
**
** NOTES:
**
**   PROPERTY DATABASE
**
**   This version assumes that there is a per-resource database provider
**   to record properties. The database provider decides how and where to
**   store these databases.
**
**   The DBM keys for the properties have the following form:
**
**     namespace ":" propname
**
**   For example: 5:author
**
**   The namespace provides an integer index into the namespace table
**   (see below). propname is simply the property name, without a namespace
**   prefix.
**
**   A special case exists for properties that had a prefix starting with
**   "xml". The XML Specification reserves these for future use. mod_dav
**   stores and retrieves them unchanged. The keys for these properties
**   have the form:
**
**     ":" propname
**
**   The propname will contain the prefix and the property name. For
**   example, a key might be ":xmlfoo:name"
**
**   The ":name" style will also be used for properties that do not
**   exist within a namespace.
**
**   The DBM values consist of two null-terminated strings, appended
**   together (the null-terms are retained and stored in the database).
**   The first string is the xml:lang value for the property. An empty
**   string signifies that a lang value was not in context for the value.
**   The second string is the property value itself.
**
**
**   NAMESPACE TABLE
**
**   The namespace table is an array that lists each of the namespaces
**   that are in use by the properties in the given propdb. Each entry
**   in the array is a simple URI.
**
**   For example: http://www.foo.bar/standards/props/
**
**   The prefix used for the property is stripped and the URI for it
**   is entered into the namespace table. Also, any namespaces used
**   within the property value will be entered into the table (and
**   stripped from the child elements).
**
**   The namespaces are stored in the DBM database under the "METADATA" key.
**
**
**   STRIPPING NAMESPACES
**
**   Within the property values, the namespace declarations (xmlns...)
**   are stripped. Each element and attribute will have its prefix removed
**   and a new prefix inserted.
**
**   This must be done so that we can return multiple properties in a
**   PROPFIND which may have (originally) used conflicting prefixes. For
**   that case, we must bind all property value elements to new namespace
**   values.
**
**   This implies that clients must NOT be sensitive to the namespace
**   prefix used for their properties. It WILL change when the properties
**   are returned (we return them as "ns<index>", e.g. "ns5"). Also, the
**   property value can contain ONLY XML elements and CDATA. PI and comment
**   elements will be stripped. CDATA whitespace will be preserved, but
**   whitespace within element tags will be altered. Attribute ordering
**   may be altered. Element and CDATA ordering will be preserved.
**
**
**   ATTRIBUTES ON PROPERTY NAME ELEMENTS
**
**   When getting/setting properties, the XML used looks like:
**
**     <prop>
**       <propname1>value</propname1>
**       <propname2>value</propname1>
**     </prop>
**
**   This implementation (mod_dav) DOES NOT save any attributes that are
**   associated with the <propname1> element. The property value is deemed
**   to be only the contents ("value" in the above example).
**
**   We do store the xml:lang value (if any) that applies to the context
**   of the <propname1> element. Whether the xml:lang attribute is on
**   <propname1> itself, or from a higher level element, we will store it
**   with the property value.
**
**
**   VERSIONING
**
**   The DBM db contains a key named "METADATA" that holds database-level
**   information, such as the namespace table. The record also contains the
**   db's version number as the very first 16-bit value. This first number
**   is actually stored as two single bytes: the first byte is a "major"
**   version number. The second byte is a "minor" number.
**
**   If the major number is not what mod_dav expects, then the db is closed
**   immediately and an error is returned. A minor number change is
**   acceptable -- it is presumed that old/new dav_props.c can deal with
**   the database format. For example, a newer dav_props might update the
**   minor value and append information to the end of the metadata record
**   (which would be ignored by previous versions).
**
**
** ISSUES:
**
**   At the moment, for the dav_get_allprops() and dav_get_props() functions,
**   we must return a set of xmlns: declarations for ALL known namespaces
**   in the file. There isn't a way to filter this because we don't know
**   which are going to be used or not. Examining property names is not
**   sufficient because the property values could use entirely different
**   namespaces.
**
**   ==> we must devise a scheme where we can "garbage collect" the namespace
**       entries from the property database.
*/

#include "mod_dav.h"

#include "http_log.h"
#include "http_request.h"

#define DAV_GDBM_NS_KEY		"METADATA"
#define DAV_GDBM_NS_KEY_LEN	8

#define DAV_EMPTY_VALUE		"\0"	/* TWO null terms */

/* the namespace URI was not found; no ID is available */
#define DAV_NS_ERROR_NOT_FOUND	(DAV_NS_ERROR_BASE)

typedef struct {
    unsigned char major;
#define DAV_DBVSN_MAJOR		4
    /*
    ** V4 -- 0.9.9 ..
    **       Prior versions could have keys or values with invalid
    **       namespace prefixes as a result of the xmlns="" form not
    **       resetting the default namespace to be "no namespace". The
    **       namespace would be set to "" which is invalid; it should
    **       be set to "no namespace".
    **
    ** V3 -- 0.9.8
    **       Prior versions could have values with invalid namespace
    **       prefixes due to an incorrect mapping of input to propdb
    **       namespace indices. Version bumped to obsolete the old
    **       values.
    **
    ** V2 -- 0.9.7
    **       This introduced the xml:lang value into the property value's
    **       record in the propdb.
    **
    ** V1 -- .. 0.9.6
    **       Initial version.
    */


    unsigned char minor;
#define DAV_DBVSN_MINOR		0

    short ns_count;

} dav_propdb_metadata;

struct dav_propdb {
    int version;		/* *minor* version of this db */

    pool *p;			/* the pool we should use */
    request_rec *r;		/* the request record */

    const dav_resource *resource;	/* the target resource */

    dav_db *db;			/* underlying database containing props */

    dav_buffer ns_table;	/* table of namespace URIs */
    short ns_count;		/* number of entries in table */
    int ns_table_dirty;		/* ns_table was modified */

    array_header *ns_xlate;	/* translation of an elem->ns to URI */
    int *ns_map;		/* map elem->ns to propdb ns values */
    int incomplete_map;		/* some mappings do not exist */

    dav_lockdb *lockdb;		/* the lock database */

    dav_buffer wb_key;		/* work buffer for dav_gdbm_key */
    dav_buffer wb_lock;		/* work buffer for lockdiscovery property */

    /* if we ever run a GET subreq, it will be stored here */
    request_rec *subreq;

    /* hooks we should use for processing (based on the target resource) */
    const dav_hooks_db *db_hooks;
    const dav_hooks_vsn *vsn_hooks;

    const dav_dyn_hooks *liveprop;	/* head of list */
};

/* ### move these into a "core" liveprop provider? */
static const char *dav_core_props[] =
{
    "getcontenttype",
    "getcontentlanguage",
    "lockdiscovery",
    "resourcetype",
    "supportedlock",

    NULL	/* sentinel */
};
enum {
    DAV_PROPID_CORE_getcontenttype = DAV_PROPID_CORE,
    DAV_PROPID_CORE_getcontentlanguage,
    DAV_PROPID_CORE_lockdiscovery,
    DAV_PROPID_CORE_resourcetype,
    DAV_PROPID_CORE_supportedlock,

    DAV_PROPID_CORE_UNKNOWN
};

/*
** This structure is used to track information needed for a rollback.
** If a SET was performed and no prior value existed, then value.dptr
** will be NULL.
*/
typedef struct dav_rollback_item {
    dav_datum key;		/* key for the item being saved */
    dav_datum value;		/* value before set/replace/delete */
} dav_rollback_item;


static int dav_find_davprop(dav_propdb *propdb, const char *name)
{
    const char **p;
    int anno;

    for (anno = DAV_PROPID_CORE, p = dav_core_props; *p != NULL; ++p, ++anno)
	if (strcmp(name, *p) == 0)
	    return anno;

    /* ### this should scan a list */
    anno = (*DAV_AS_HOOKS_LIVEPROP(propdb->liveprop)->find_prop)(name);
    if (anno != 0)
	return anno;

    return DAV_PROPID_CORE_UNKNOWN;
}

/* is the davprop read/write? */
static int dav_rw_davprop(dav_propdb *propdb, int davprop)
{
    dav_prop_rw rw;

    /* these are defined as read-only */
    if (davprop == DAV_PROPID_CORE_lockdiscovery
	|| davprop == DAV_PROPID_CORE_resourcetype
	|| davprop == DAV_PROPID_CORE_supportedlock) {

	return 0;
    }

    /* these are defined as read/write */
    if (davprop == DAV_PROPID_CORE_getcontenttype
	|| davprop == DAV_PROPID_CORE_getcontentlanguage
	|| davprop == DAV_PROPID_CORE_UNKNOWN) {

	return 1;
    }

    /*
    ** Check the liveprop providers
    */
    /* ### this should be a scan of the providers */
    rw = (*DAV_AS_HOOKS_LIVEPROP(propdb->liveprop)->is_writeable)(propdb->resource, davprop);
    if (rw == DAV_PROP_RW_YES)
	return 1;
    if (rw == DAV_PROP_RW_NO)
	return 0;

    /*
    ** No provider recognized the property, so it must be dead (and writable)
    */
    return 1;
}

/* Note: picked up from ap_gm_timestr_822() */
/* NOTE: buf must be at least DAV_TIMEBUF_SIZE chars in size */
int dav_format_time(int style, time_t sec, char *buf)
{
    struct tm *tms;

    tms = gmtime(&sec);

    if (style == DAV_STYLE_ISO8601) {
	/* ### should we use "-00:00" instead of "Z" ?? */

	/* 20 chars plus null term */
	sprintf(buf, "%.4d-%.2d-%.2dT%.2d:%.2d:%.2dZ",
		tms->tm_year + 1900, tms->tm_mon + 1, tms->tm_mday,
		tms->tm_hour, tms->tm_min, tms->tm_sec);
	return 21;
    }

    /* RFC 822 date format; as strftime '%a, %d %b %Y %T GMT' */

    /* 29 chars plus null term */
    sprintf(buf,
	    "%s, %.2d %s %d %.2d:%.2d:%.2d GMT",
	    ap_day_snames[tms->tm_wday],
	    tms->tm_mday, ap_month_snames[tms->tm_mon],
	    tms->tm_year + 1900,
	    tms->tm_hour, tms->tm_min, tms->tm_sec);
    return 30;
}

/* do a sub-request to fetch properties for the target resource's URI. */
static void dav_do_prop_subreq(dav_propdb *propdb)
{
    const char *uri;

    uri = (*DAV_GET_HOOKS_REPOSITORY(propdb->r)->get_uri)(propdb->resource);

    /* perform a "GET" on the resource's URI (note that the resource
       may not correspond to the current request!). */
    propdb->subreq = ap_sub_req_lookup_uri(uri, propdb->r);
}

static dav_error * dav_insert_davprop(dav_propdb *propdb,
				      int davprop, const char *name,
				      int getvals,
				      dav_text_header *phdr,
				      int *inserted)
{
    const char *value = NULL;
    dav_prop_insert pi;

    if (inserted != NULL)
	*inserted = 0;

    /* if the liveprop hooks can do this, then we're done. */
    /* ### should scan a *list* of providers */
    pi = (*DAV_AS_HOOKS_LIVEPROP(propdb->liveprop)->insert_prop)(propdb->resource, davprop, getvals, phdr);
    if (pi != DAV_PROP_INSERT_NOTME) {
	if (inserted != NULL)
	    *inserted = 1;
	return NULL;
    }

    switch (davprop) {

    case DAV_PROPID_CORE_resourcetype:
        switch (propdb->resource->type) {
        case DAV_RESOURCE_TYPE_REGULAR:
            if (propdb->resource->collection) {
	        value = "<D:collection/>";
            }
	    else {
		/* ### should we denote lock-null resources? */

		value = "";	/* becomes: <D:resourcetype/> */
	    }
            break;
        case DAV_RESOURCE_TYPE_HISTORY:
	    value = "<D:history/>";
            break;
        case DAV_RESOURCE_TYPE_WORKSPACE:
	    value = "<D:workspace/>";
            break;
        case DAV_RESOURCE_TYPE_ACTIVITY:
	    value = "<D:activity/>";
            break;
        case DAV_RESOURCE_TYPE_CONFIGURATION:
	    value = "<D:configuration/>";
            break;
	case DAV_RESOURCE_TYPE_REVISION:
	    value = "<D:revision/>";
	    break;

	default:
	    /* ### bad juju */
	    break;
        }
	break;

    case DAV_PROPID_CORE_lockdiscovery:
        if (propdb->lockdb != NULL) {
	    dav_error *err;
	    dav_lock *locks;

	    if ((err = dav_lock_query(propdb->lockdb, propdb->resource,
				      &locks)) != NULL) {
		/* ### don't log. use err. add higher-level description. */
	        /* ### map result to something nice; log an error */
	        /* ### really... we probably should not log errors here... */
		ap_log_rerror(APLOG_MARK, APLOG_ERR |APLOG_NOERRNO, propdb->r,
			      "%s  (#%d)",
			      err->desc ? err->desc : "There was a problem fetching this resource's locks.",
			      err->error_id);
		return err;
	    }

	    /* this modifies the buffer (and returns buf.pbuf) */
	    (void) dav_lock_get_activelock(propdb->r, locks, &propdb->wb_lock);

	    /* make a copy to isolate it from changes to wb_lock */
	    value = ap_pstrndup(propdb->p, propdb->wb_lock.buf,
				propdb->wb_lock.cur_len);
        }
	break;

    case DAV_PROPID_CORE_supportedlock:
        if (propdb->lockdb != NULL) {
	    value = (*propdb->lockdb->hooks->get_supportedlock)();
        }
	break;

    case DAV_PROPID_CORE_getcontenttype:
	if (propdb->subreq == NULL) {
	    dav_do_prop_subreq(propdb);
	}
	if (propdb->subreq->content_type != NULL) {
	    value = propdb->subreq->content_type;
	}
	break;

    case DAV_PROPID_CORE_getcontentlanguage:
    {
	const char *lang;

	if (propdb->subreq == NULL) {
	    dav_do_prop_subreq(propdb);
	}
	if ((lang = ap_table_get(propdb->subreq->headers_out,
				 "Content-Language")) != NULL) {
	    value = lang;
	}
	break;
    }

    case DAV_PROPID_CORE_UNKNOWN:
    default:
	/* fall through to interpret as a dead property */
	break;
    }

    /* if something was supplied, then insert it */
    if (value != NULL) {
	const char *s;

	if (getvals && *value != '\0') {
	    /* use D: prefix to refer to the DAV: namespace URI */
	    s = ap_psprintf(propdb->p, "<D:%s>%s</D:%s>" DEBUG_CR,
			    name, value, name);
	}
	else {
	    /* use D: prefix to refer to the DAV: namespace URI */
	    s = ap_psprintf(propdb->p, "<D:%s/>" DEBUG_CR, name);
	}
	dav_text_append(propdb->p, phdr, s);

	/* we inserted a name or value (this prop is done) */
	if (inserted != NULL)
	    *inserted = 1;
	return NULL;
    }

    /* nothing was inserted */
    return NULL;
}

static void dav_append_prop(dav_propdb *propdb,
			    const char *name, const char *value,
			    dav_text_header *phdr)
{
    const char *s;
    const char *lang = value;

    /* skip past the xml:lang value */
    value += strlen(lang) + 1;

    if (*value == '\0') {
	/* the property is an empty value */
	if (*name == ':') {
	    /* "no namespace" case */
	    s = ap_psprintf(propdb->p, "<%s/>" DEBUG_CR, name+1);
	}
	else {
	    s = ap_psprintf(propdb->p, "<ns%s/>" DEBUG_CR, name);
	}
    }
    else if (*lang != '\0') {
	if (*name == ':') {
	    /* "no namespace" case */
	    s = ap_psprintf(propdb->p, "<%s xml:lang=\"%s\">%s</%s>" DEBUG_CR,
			    name+1, lang, value, name+1);
	}
	else {
	    s = ap_psprintf(propdb->p, "<ns%s xml:lang=\"%s\">%s</ns%s>" DEBUG_CR,
			    name, lang, value, name);
	}
    }
    else if (*name == ':') {
	/* "no namespace" case */
	s = ap_psprintf(propdb->p, "<%s>%s</%s>" DEBUG_CR, name+1, value, name+1);
    }
    else {
	s = ap_psprintf(propdb->p, "<ns%s>%s</ns%s>" DEBUG_CR, name, value, name);
    }
    dav_text_append(propdb->p, phdr, s);
}

/*
** Prepare the ns_map variable in the propdb structure. This entails copying
** all URIs from the "input" namespace list (in propdb->ns_xlate) into the
** propdb's list of namespaces. As each URI is copied (or pre-existing
** URI looked up), the index mapping is stored into the ns_map variable.
**
** Note: we must copy all declared namespaces because we cannot easily
**   determine which input namespaces were actually used within the property
**   values that are being stored within the propdb. Theoretically, we can
**   determine this at the point where we serialize the property values
**   back into strings. This would require a bit more work, and will be
**   left to future optimizations.
**
** ### we should always initialize the propdb namespace array with "DAV:"
** ### since we know it will be entered anyhow (by virtue of it always
** ### occurring in the ns_xlate array). That will allow us to use
** ### DAV_NS_DAV_ID for propdb ns values, too.
*/
static void dav_prep_ns_map(dav_propdb *propdb, int add_ns)
{
    int i;
    const char **puri;
    const int orig_count = propdb->ns_count;
    int *pmap;
    int updating = 0;	/* are we updating an existing ns_map? */

    if (propdb->ns_map) {
	if (add_ns && propdb->incomplete_map) {
	    /* we must revisit the map and insert new entries */
	    updating = 1;
	    propdb->incomplete_map = 0;
	}
	else {
	    /* nothing to do: we have a proper ns_map */
	    return;
	}
    }
    else {
	propdb->ns_map = ap_palloc(propdb->p, propdb->ns_xlate->nelts * sizeof(*propdb->ns_map));
    }

    pmap = propdb->ns_map;

    /* ### stupid O(n * orig_count) algorithm */
    for (i = propdb->ns_xlate->nelts, puri = (const char **)propdb->ns_xlate->elts;
	 i-- > 0;
	 ++puri, ++pmap) {

	const char *uri = *puri;
	const int uri_len = strlen(uri);

	if (updating) {
	    /* updating an existing mapping... we can skip a lot of stuff */

	    if (*pmap != DAV_NS_ERROR_NOT_FOUND) {
		/* This entry has been filled in, so we can skip it */
		continue;
	    }
	}
	else {
	    int j;
	    int len;
	    const char *p;

	    /*
	    ** GIVEN: uri (a namespace URI from the request input)
	    **
	    ** FIND: an equivalent URI in the propdb namespace table
	    */

	    /* only scan original entries (we may have added some in here) */
	    for (p = propdb->ns_table.buf + sizeof(dav_propdb_metadata),
		     j = 0;
		 j < orig_count;
		 ++j, p += len + 1) {

		len = strlen(p);

		if (uri_len != len)
		    continue;
		if (memcmp(uri, p, len) == 0) {
		    *pmap = j;
		    goto next_input_uri;
		}
	    }

	    if (!add_ns) {
		*pmap = DAV_NS_ERROR_NOT_FOUND;

		/*
		** This flag indicates that we have an ns_map with missing
		** entries. If dav_prep_ns_map() is called with add_ns==1 AND
		** this flag is set, then we zip thru the array and add those
		** URIs (effectively updating the ns_map as if add_ns=1 was
		** passed when the initial prep was called).
		*/
		propdb->incomplete_map = 1;

		continue;
	    }
	}

	/*
	** The input URI was not found in the propdb namespace table, and
	** we are supposed to add it. Append it to the table and store
	** the index into the ns_map.
	*/
	dav_check_bufsize(propdb->p, &propdb->ns_table, uri_len + 1);
	memcpy(propdb->ns_table.buf + propdb->ns_table.cur_len, uri, uri_len + 1);
	propdb->ns_table.cur_len += uri_len + 1;

	propdb->ns_table_dirty = 1;

	*pmap = propdb->ns_count++;

   next_input_uri:
	;
    }
}

#if 0
/* ### not used right now... */
static const char *dav_get_ns_table_uri(dav_propdb *propdb, int ns)
{
    const char *p = propdb->ns_table.buf + sizeof(dav_propdb_metadata);

    while (ns--)
	p += strlen(p) + 1;

    return p;
}
#endif

/* find the "DAV:" namespace in our table and return its ID. */
static int dav_find_dav_id(dav_propdb *propdb)
{
    const char *p = propdb->ns_table.buf + sizeof(dav_propdb_metadata);
    int ns;

    for (ns = 0; ns < propdb->ns_count; ++ns) {
	int len = strlen(p);

	if (len == 4 && memcmp(p, "DAV:", 5) == 0)
	    return ns;
	p += len + 1;
    }

    /* the "DAV:" namespace is not present */
    return -1;
}

/* return all known namespaces (in this propdb) */
static dav_text *dav_get_xmlns(dav_propdb *propdb, dav_text_header *phdr)
{
    dav_text_header hdr = { 0 };
    int i;
    const char *p = propdb->ns_table.buf + sizeof(dav_propdb_metadata);
    int len;

    if (phdr == NULL)
	phdr = &hdr;

    /* note: ns_count == 0 when we have no propdb file */
    for (i = 0; i < propdb->ns_count; ++i, p += len + 1) {
	const char *s;

	len = strlen(p);

	s = ap_psprintf(propdb->p, " xmlns:ns%d=\"%s\"", i, p);
	dav_text_append(propdb->p, phdr, s);
    }

    return phdr->first;
}

/* add a namespace decl from the input namespace table */
static void dav_add_input_xmlns(dav_propdb *propdb, char *marks, int ns, dav_text_header *phdr)
{
    const char *s;

    if (marks[ns])
	return;
    marks[ns] = 1;

    /* use namespaces of the form "i%d" for input namespaces */
    s = ap_psprintf(propdb->p, " xmlns:i%d=\"%s\"", ns, DAV_GET_NS_URI(propdb->ns_xlate, ns));
    dav_text_append(propdb->p, phdr, s);
}

/*
** Internal function to build a key
**
** WARNING: returns a pointer to a "static" buffer holding the key. The
**          value must be copied or no longer used if this function is
**          called again.
*/
static dav_datum dav_gdbm_key(dav_propdb *propdb, const dav_xml_elem *elem)
{
    int ns;
    char nsbuf[20];
    int l_ns;
    int l_name = strlen(elem->name);
    dav_datum key = { 0 };

    /*
     * Convert namespace ID to a string. "no namespace" is an empty string,
     * so the keys will have the form ":name". Otherwise, the keys will
     * have the form "#:name".
     */
    if (elem->ns == DAV_NS_NONE) {
	nsbuf[0] = '\0';
	l_ns = 0;
    }
    else {
	if (propdb->ns_map == NULL) {
	    /*
	     * Note that we prep the map and do NOT add namespaces. If that
	     * is required, then the caller should have called prep
	     * beforehand, passing the correct values.
	     */
	    dav_prep_ns_map(propdb, 0);
	}

	ns = propdb->ns_map[elem->ns];
	if (DAV_NS_IS_ERROR(ns))
	    return key;		/* zeroed */

	l_ns = sprintf(nsbuf, "%d", ns);
    }

    /* assemble: #:name */
    dav_set_bufsize(propdb->p, &propdb->wb_key, l_ns + 1 + l_name + 1);
    memcpy(propdb->wb_key.buf, nsbuf, l_ns);
    propdb->wb_key.buf[l_ns] = ':';
    memcpy(&propdb->wb_key.buf[l_ns + 1], elem->name, l_name + 1);

    /* build the database key */
    key.dsize = l_ns + 1 + l_name + 1;
    key.dptr = propdb->wb_key.buf;

    return key;
}


dav_error *dav_open_propdb(request_rec *r, dav_lockdb *lockdb,
			   const dav_resource *resource,
			   int ro,
			   array_header * ns_xlate,
			   dav_propdb **p_propdb)
{
    const dav_hooks_db *db_hooks = DAV_GET_HOOKS_PROPDB(r);
    dav_propdb *propdb = ap_pcalloc(r->pool, sizeof(*propdb));
    dav_datum key;
    dav_datum value = { 0 };
    dav_error *err = NULL;

    *p_propdb = NULL;

#if 1
    if ((*DAV_GET_HOOKS_REPOSITORY(r)->get_uri)(resource) == NULL) {
	return dav_new_error(r->pool, HTTP_INTERNAL_SERVER_ERROR, 0,
			     "INTERNAL DESIGN ERROR: resource must define "
			     "its URI.");
    }
#endif

    err = (*db_hooks->open)(r->pool, resource, ro, &propdb->db);
    if (err != NULL) {
	return dav_push_error(r->pool, HTTP_INTERNAL_SERVER_ERROR,
			      DAV_ERR_PROP_OPENING,
			      "Could not open the property database.",
			      err);
    }

    /*
    ** NOTE: propdb->db could be NULL if we attempted to open a readonly
    **       database that doesn't exist. If we require read/write
    **       access, then a database was created and opened.
    */

    propdb->version = DAV_DBVSN_MINOR;
    propdb->r = r;
    propdb->p = r->pool; /* ### get rid of this */
    propdb->resource = resource;
    propdb->ns_xlate = ns_xlate;

    propdb->db_hooks = db_hooks;
    propdb->vsn_hooks = DAV_GET_HOOKS_VSN(r);

    propdb->liveprop = dav_get_provider_hooks(r, DAV_DYN_TYPE_LIVEPROP);

    propdb->lockdb = lockdb;

    if (propdb->db != NULL) {
	key.dptr = DAV_GDBM_NS_KEY;
	key.dsize = DAV_GDBM_NS_KEY_LEN;
	(void) (*db_hooks->fetch)(propdb->db, key, &value);
    }
    if (value.dptr == NULL) {
	dav_propdb_metadata m = {
	    DAV_DBVSN_MAJOR, DAV_DBVSN_MINOR, 0
	};

	if (propdb->db != NULL) {
	    /*
	     * If there is no METADATA key, then the database may be
	     * from versions 0.9.0 .. 0.9.4 (which would be incompatible).
	     * These can be identified by the presence of an NS_TABLE entry.
	     */
	    key.dptr = "NS_TABLE";
	    key.dsize = 8;
	    if ((*db_hooks->exists)(propdb->db, key)) {
		(*db_hooks->close)(propdb->db);

		/* call it a major version error */
		return dav_new_error(r->pool, HTTP_INTERNAL_SERVER_ERROR,
				     DAV_ERR_PROP_BAD_MAJOR,
				     "Prop database has the wrong major "
				     "version number and cannot be used.");
	    }
	}

	/* initialize a new metadata structure */
	dav_set_bufsize(r->pool, &propdb->ns_table, sizeof(m));
	memcpy(propdb->ns_table.buf, &m, sizeof(m));
    }
    else {
	dav_propdb_metadata m;

	dav_set_bufsize(r->pool, &propdb->ns_table, value.dsize);
	memcpy(propdb->ns_table.buf, value.dptr, value.dsize);

	memcpy(&m, value.dptr, sizeof(m));
	if (m.major != DAV_DBVSN_MAJOR) {
	    (*db_hooks->close)(propdb->db);

	    return dav_new_error(r->pool, HTTP_INTERNAL_SERVER_ERROR,
				 DAV_ERR_PROP_BAD_MAJOR,
				 "Prop database has the wrong major "
				 "version number and cannot be used.");
	}
	propdb->version = m.minor;
	propdb->ns_count = ntohs(m.ns_count);

	(*db_hooks->freedatum)(propdb->db, value);
    }

    /* ### what to do about closing the propdb on server failure? */

    *p_propdb = propdb;
    return NULL;
}

void dav_close_propdb(dav_propdb *propdb)
{
    if (propdb->db == NULL)
	return;

    if (propdb->ns_table_dirty) {
	dav_propdb_metadata m;
	dav_datum key;
	dav_datum value;
	dav_error *err;

	key.dptr = DAV_GDBM_NS_KEY;
	key.dsize = DAV_GDBM_NS_KEY_LEN;

	value.dptr = propdb->ns_table.buf;
	value.dsize = propdb->ns_table.cur_len;

	/* fill in the metadata that we store into the prop db. */
	m.major = DAV_DBVSN_MAJOR;
	m.minor = propdb->version;	/* ### keep current minor version? */
	m.ns_count = htons(propdb->ns_count);

	memcpy(propdb->ns_table.buf, &m, sizeof(m));

	err = (*propdb->db_hooks->store)(propdb->db, key, value);
	/* ### what to do with the error? */
    }

    (*propdb->db_hooks->close)(propdb->db);
}

dav_get_props_result dav_get_allprops(dav_propdb *propdb, int getvals)
{
    const dav_hooks_db *db_hooks = propdb->db_hooks;
    dav_text_header hdr = { 0 };
    dav_get_props_result result = { 0 };
    int found_resourcetype = 0;
    int found_contenttype = 0;
    int found_contentlang = 0;

    /* initialize the result with some start tags... */
    dav_text_append(propdb->p, &hdr,
		    "<D:propstat>" DEBUG_CR
		    "<D:prop>" DEBUG_CR);

    /* if there ARE properties, then scan them */
    if (propdb->db != NULL) {
	dav_datum key;
	int dav_id = dav_find_dav_id(propdb);

	(void) (*db_hooks->firstkey)(propdb->db, &key);
	while (key.dptr) {
	    dav_datum prevkey;

	    /* any keys with leading capital letters should be skipped
	       (real keys start with a number or a colon) */
	    if (*key.dptr >= 'A' && *key.dptr <= 'Z')
		goto next_key;

	    /*
	    ** See if this is the <DAV:resourcetype> property. We need to
	    ** know whether it was found (and therefore, whether to supply
	    ** a default later).
	    **
	    ** We also look for <DAV:getcontenttype> and
	    ** <DAV:getcontentlanguage>. If they are not stored as dead
	    ** properties, then we need to perform a subrequest to get
	    ** their values (if any).
	    */
	    if (dav_id != -1
		&& *key.dptr != ':'
		&& dav_id == atoi(key.dptr)) {

		const char *colon;

		/* find the colon */
		if ( key.dptr[1] == ':' ) {
		    colon = key.dptr + 1;
		}
		else {
		    colon = strchr(key.dptr + 2, ':');
		}

		if (colon[1] == 'r'
		    && strcmp(colon + 1, "resourcetype") == 0) {

		    found_resourcetype = 1;
		}
		else if (colon[1] == 'g') {
		    if (strcmp(colon + 1, "getcontenttype") == 0) {
			found_contenttype = 1;
		    }
		    else if (strcmp(colon + 1, "getcontentlanguage") == 0) {
			found_contentlang = 1;
		    }
		}
	    }

	    if (getvals) {
		dav_datum value;

		(void) (*db_hooks->fetch)(propdb->db, key, &value);
		if (value.dptr == NULL) {
		    /* ### anything better to do? */
		    /* ### probably should enter a 500 error */
		    goto next_key;
		}

		/* put the prop name and value into the result */
		dav_append_prop(propdb, key.dptr, value.dptr, &hdr);

		(*db_hooks->freedatum)(propdb->db, value);
	    }
	    else {
		/* simple, empty element if a value isn't needed */
		dav_append_prop(propdb, key.dptr, DAV_EMPTY_VALUE, &hdr);
	    }

	  next_key:
	    prevkey = key;
	    (void) (*db_hooks->nextkey)(propdb->db, &key);
	    (*db_hooks->freedatum)(propdb->db, prevkey);
	}
    }

    /* ask the liveprop providers to insert their properties */
    /* ### this should iterate over a list of liveprop providers */
    (*DAV_AS_HOOKS_LIVEPROP(propdb->liveprop)->insert_all)(propdb->resource, getvals, &hdr);

    /* insert the standard properties */
    /* ### should be handling the return errors here */
    (void)dav_insert_davprop(propdb,
			     DAV_PROPID_CORE_supportedlock, "supportedlock",
			     getvals, &hdr, NULL);
    (void)dav_insert_davprop(propdb,
			     DAV_PROPID_CORE_lockdiscovery, "lockdiscovery",
			     getvals, &hdr, NULL);

    /* if the resourcetype wasn't stored, then prepare one */
    if (!found_resourcetype) {
	/* ### should be handling the return error here */
	(void)dav_insert_davprop(propdb,
				 DAV_PROPID_CORE_resourcetype, "resourcetype",
				 getvals, &hdr, NULL);
    }

    /* if we didn't find these, then do the whole subreq thing. */
    if (!found_contenttype) {
	/* ### should be handling the return error here */
	(void)dav_insert_davprop(propdb,
				 DAV_PROPID_CORE_getcontenttype,
				 "getcontenttype",
				 getvals, &hdr, NULL);
    }
    if (!found_contentlang) {
	/* ### should be handling the return error here */
	(void)dav_insert_davprop(propdb,
				 DAV_PROPID_CORE_getcontentlanguage,
				 "getcontentlanguage",
				 getvals, &hdr, NULL);
    }

    /* terminate the result */
    dav_text_append(propdb->p, &hdr,
		    "</D:prop>" DEBUG_CR
		    "<D:status>HTTP/1.1 200 OK</D:status>" DEBUG_CR
		    "</D:propstat>" DEBUG_CR);

    result.propstats = hdr.first;
    result.xmlns = dav_get_xmlns(propdb, NULL);
    return result;
}

dav_get_props_result dav_get_props(dav_propdb *propdb, dav_xml_doc *doc)
{
    const dav_hooks_db *db_hooks = propdb->db_hooks;
    dav_xml_elem *elem = dav_find_child(doc->root, "prop");
    dav_text_header hdr_good = { 0 };
    dav_text_header hdr_bad = { 0 };
    dav_text_header hdr_ns = { 0 };
    int have_good = 0;
    dav_get_props_result result = { 0 };
    char *marks;

    /* ### NOTE: we should pass in TWO buffers -- one for keys, one for
       the marks */

    /* we will ALWAYS provide a "good" result, even if it is EMPTY */
    dav_text_append(propdb->p, &hdr_good,
		    "<D:propstat>" DEBUG_CR
		    "<D:prop>" DEBUG_CR);

    /* generate all the namespaces that are in the propdb */
    dav_get_xmlns(propdb, &hdr_ns);

    /* allocate zeroed-memory for the marks. These marks indicate which
       input namespaces we've generated into the output xmlns buffer */
    marks = ap_pcalloc(propdb->p, propdb->ns_xlate->nelts);

    for (elem = elem->first_child; elem; elem = elem->next) {
	dav_datum key;
	dav_datum value = { 0 };

	/*
	** Note: the key may be NULL if we have no properties that are in
	** a namespace that matches the requested prop's namespace.
	*/
	key = dav_gdbm_key(propdb, elem);

	/* fetch IF we have a db and a key. otherwise, value is NULL */
	if (propdb->db != NULL && key.dptr != NULL) {
	    (void) (*db_hooks->fetch)(propdb->db, key, &value);
	}

	/*
	** If we did not find the property in the database, then it may
	** be a liveprop that we can handle specially.
	*/
	if (value.dptr == NULL && elem->ns == DAV_NS_DAV_ID) {
	    dav_error *err;
	    int inserted;

	    /* cache the propid; dav_get_props() could be called many times */
	    if (elem->annotation == 0)
		elem->annotation = dav_find_davprop(propdb, elem->name);

	    /* insert the property. returns 1 if an insertion was done. */
	    if ((err = dav_insert_davprop(propdb, elem->annotation,
					  elem->name, 1, &hdr_good,
					  &inserted)) != NULL) {
		/* ### need to propagate the error to the caller... */
		/* ### skip it for now, as if nothing was inserted */
	    }
	    if (inserted) {
		have_good = 1;
		continue;
	    }
	}

	if (value.dptr == NULL) {
	    /* not found. add a record to the "bad" propstats */

	    /* make sure we've started our "bad" propstat */
	    if (hdr_bad.first == NULL) {
		dav_text_append(propdb->p, &hdr_bad,
				"<D:propstat>" DEBUG_CR
				"<D:prop>" DEBUG_CR);
	    }

	    /* note: key.dptr may be NULL if the propdb doesn't have an
	       equivalent namespace stored */
	    if (key.dptr == NULL) {
		const char *s;

		if (elem->ns == DAV_NS_NONE) {
		    /*
		     * elem has a prefix already (xml...:name) or the elem
		     * simply has no namespace.
		     */
		    s = ap_psprintf(propdb->p, "<%s/>" DEBUG_CR, elem->name);
		}
		else {
		    /* ensure that an xmlns is generated for the
		       input namespace */
		    dav_add_input_xmlns(propdb, marks, elem->ns, &hdr_ns);
		    s = ap_psprintf(propdb->p, "<i%d:%s/>" DEBUG_CR, elem->ns, elem->name);
		}
		dav_text_append(propdb->p, &hdr_bad, s);
	    }
	    else {
		/* add in the bad prop using our namespace decl */
		dav_append_prop(propdb, key.dptr, DAV_EMPTY_VALUE, &hdr_bad);
	    }
	}
	else {
	    /* found it. put the value into the "good" propstats */

	    have_good = 1;

	    dav_append_prop(propdb, key.dptr, value.dptr, &hdr_good);

	    (*db_hooks->freedatum)(propdb->db, value);
	}
    }

    dav_text_append(propdb->p, &hdr_good,
		    "</D:prop>" DEBUG_CR
		    "<D:status>HTTP/1.1 200 OK</D:status>" DEBUG_CR
		    "</D:propstat>" DEBUG_CR);

    /* default to start with the good */
    result.propstats = hdr_good.first;

    /* we may not have any "bad" results */
    if (hdr_bad.first != NULL) {
	dav_text_append(propdb->p, &hdr_bad,
			"</D:prop>" DEBUG_CR
			"<D:status>HTTP/1.1 404 Not Found</D:status>" DEBUG_CR
			"</D:propstat>" DEBUG_CR);

	/* if there are no good props, then just return the bad */
	if (!have_good) {
	    result.propstats = hdr_bad.first;
	}
	else {
	    /* hook the bad propstat to the end of the good one */
	    hdr_good.last->next = hdr_bad.first;
	}
    }

    result.xmlns = hdr_ns.first;
    return result;
}

void dav_prop_validate(dav_prop_ctx *ctx)
{
    dav_xml_elem *prop = ctx->prop;

    /*
    ** If we have a DAV property, then annotate the element with our
    ** internal identifier for it.
    **
    ** Verify that the property is read/write. If not, then it cannot
    ** be SET or DELETEd.
    */
    if (prop->ns == DAV_NS_DAV_ID) {
	if (prop->annotation == 0)
	    prop->annotation = dav_find_davprop(ctx->propdb, prop->name);

	if (!dav_rw_davprop(ctx->propdb, prop->annotation)) {
	    ctx->err = dav_new_error(ctx->propdb->p, HTTP_CONFLICT,
				     DAV_ERR_PROP_READONLY,
				     "Property is read-only.");
	    return;
	}
    }

    /*
    ** There should be an open, writable database in here!
    **
    ** Note: the database would be NULL if it was opened readonly and it
    **       did not exist.
    */
    if (ctx->propdb->db == NULL) {
	ctx->err = dav_new_error(ctx->propdb->p, HTTP_INTERNAL_SERVER_ERROR,
				 DAV_ERR_PROP_NO_DATABASE,
				 "Attempted to set/remove a property "
				 "without a valid, open, read/write "
				 "property database.");
	return;
    }

    if (ctx->operation == DAV_PROP_OP_SET) {
	/*
	** Prep the element->propdb namespace index mapping, inserting
	** namespace URIs into the propdb that don't exist.
	*/
	dav_prep_ns_map(ctx->propdb, 1);
    }
    else if (ctx->operation == DAV_PROP_OP_DELETE) {
	/*
	** There are no checks to perform here. If a property exists, then
	** we will delete it. If it does not exist, then it does not matter
	** (see S12.13.1).
	**
	** Note that if a property does not exist, that does not rule out
	** that a SET will occur during this PROPPATCH (thusly creating it).
	*/
    }
}

void dav_prop_exec(dav_prop_ctx *ctx)
{
    dav_propdb *propdb = ctx->propdb;
    dav_datum key;
    dav_error *err = NULL;
    dav_rollback_item *rollback;

    /* we're going to need the key for all operations */
    key = dav_gdbm_key(propdb, ctx->prop);

    /* save the old value so that we can do a rollback. */
    rollback = ap_palloc(propdb->p, sizeof(*rollback));
    rollback->key = key;
    (void)(*propdb->db_hooks->fetch)(propdb->db, key, &rollback->value);
    ctx->rollback = rollback;

    if (ctx->operation == DAV_PROP_OP_SET) {

	dav_datum value;

	/* Note: propdb->ns_map was set in dav_prop_validate() */

	/* quote all the values in the element */
	dav_quote_xml_elem(propdb->p, ctx->prop);

	/* generate a text blob for the xml:lang plus the contents */
	dav_xml2text(propdb->p, ctx->prop, DAV_X2T_LANG_INNER, NULL,
		     propdb->ns_map,
		     (const char **)&value.dptr, &value.dsize);

	err = (*propdb->db_hooks->store)(propdb->db, key, value);

	/*
	** If an error occurred, then assume that we didn't change the
	** value. Remove the rollback item so that we don't try to set
	** its value during the rollback.
	*/
    }
    else if (ctx->operation == DAV_PROP_OP_DELETE) {

	/*
	** Delete the property. Ignore errors -- the property is there, or
	** we are deleting it for a second time.
	*/
	(void) (*propdb->db_hooks->remove)(propdb->db, key);
    }

    /* push a more specific error here */
    if (err != NULL) {
	/*
	** Use HTTP_INTERNAL_SERVER_ERROR because we shouldn't have seen
	** any errors at this point.
	*/
	ctx->err = dav_push_error(propdb->p, HTTP_INTERNAL_SERVER_ERROR,
				  DAV_ERR_PROP_EXEC,
				  "Could not execute PROPPATCH.", err);
    }
}

void dav_prop_commit(dav_prop_ctx *ctx)
{
    /*
    ** Note that a commit implies ctx->err is NULL. The caller should assume
    ** a status of HTTP_OK for this case.
    */
}

void dav_prop_rollback(dav_prop_ctx *ctx)
{
    /* do nothing if there is no rollback information. */
    if (ctx->rollback == NULL)
	return;

    /*
    ** ### if we have an
    ** ### error, and a rollback occurs, then the namespace mods should
    ** ### not happen at all. Basically, the namespace management is
    ** ### simply a bitch.
    */

    if (ctx->rollback->value.dptr == NULL) {
	(void) (*ctx->propdb->db_hooks->remove)(ctx->propdb->db,
						ctx->rollback->key);
    }
    else {
	/* ### what to do with errors? */
	(void) (*ctx->propdb->db_hooks->store)(ctx->propdb->db,
					       ctx->rollback->key,
					       ctx->rollback->value);
    }
}
