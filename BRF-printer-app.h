
#ifndef BRF_H
#  define BRF_H

//
// Include necessary headers...
//

#  include "config.h"
#  include <pappl/pappl.h>
#  include <pappl-retrofit/base.h>




//
// Constants...
//

#  define BRF_TESTPAGE_MIMETYPE	"application/vnd.cups-paged-brf"


//
// Functions...
//

extern bool	brf_gen(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata);


#endif // !BRF_H
