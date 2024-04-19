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
**  - handle dynamic DAV extensions
**
** Written by Greg Stein, gstein@lyra.org, http://www.lyra.org/
*/

#include "mod_dav.h"


/*
** Hold the runtime information associated with a module.
**
** One of these will exist per loaded module.
*/
typedef struct dav_dyn_runtime
{
    void *handle;		/* DSO handle */
    int index;			/* unique index for module */

    const dav_dyn_module *module;	/* dynamic module info */
    int num_providers;		/* number of providers in module */
    void *m_context;		/* module-level ctx (i.e. managed globals) */

    struct dav_dyn_runtime *next;
} dav_dyn_runtime;

/*
** Hold the runtime information associated with a provider.
*/
typedef struct dav_dyn_prov_ctx
{
    int active;

    const dav_dyn_provider *provider;

} dav_dyn_prov_ctx;

/*
** Hold the runtime information associated with a directory/location and
** a provider.
*/
typedef struct dav_dyn_mod_ctx
{
    dav_dyn_runtime *runtime;

    int state;
#define DAV_DYN_STATE_UNUSED	0
#define DAV_DYN_STATE_POTENTIAL	1	/* module's params seen */
#define DAV_DYN_STATE_HAS_CTX	2	/* per-dir context exists */
#define DAV_DYN_STATE_ACTIVE	3	/* a provider is active */

    void *d_context;			/* per-directory context */

    dav_dyn_prov_ctx *pc;

    struct dav_dyn_mod_ctx *next;
} dav_dyn_mod_ctx;

/* ### needs thread protection */
static dav_dyn_runtime *dav_loaded_modules = NULL;

/* builtin, default module for filesystem-based access */
extern const dav_dyn_module dav_dyn_module_default;


int dav_load_module(const char *name, const char *module_sym,
		    const char *filename)
{
    return 0;
}

const dav_dyn_module *dav_find_module(const char *name)
{
    /* name == NULL means the default. */

    /* ### look through dav_loaded_modules for the right module */

    return &dav_dyn_module_default;
}
