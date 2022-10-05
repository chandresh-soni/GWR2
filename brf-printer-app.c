

//
// Include necessary headers...
#include <strings.h>

#include <pappl/pappl.h>



# define brf_TESTPAGE_HEADER	"T*E*S*T*P*A*G*E*"
#  define brf_TESTPAGE_MIMETYPE	"application/vnd.cups-paged-brf"


extern bool	brf_gen(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata);

//
// Local functions...
//

static const char *autoadd_cb(const char *device_info, const char *device_uri, const char *device_id, void *cbdata);
static bool	driver_cb(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata);
static int	match_id(int num_did, cups_option_t *did, const char *match_id);
static const char *mime_cb(const unsigned char *header, size_t headersize, void *data);
static bool	printer_cb(const char *device_info, const char *device_uri, const char *device_id, pappl_system_t *system);
static pappl_system_t *system_cb(int num_options, cups_option_t *options, void *data);


//
// Local globals...
//

static pappl_pr_driver_t	brf_drivers[] =
{					// Driver list
{ "gen_brf",  "Generic",
  NULL, NULL },

};
static char			brf_statefile[1024];
					// State file


//
// 'main()' - Main entry for brf.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  return (papplMainloop(argc, argv,
                        VERSION,
                        NULL,
                        (int)(sizeof(brf_drivers) / sizeof(brf_drivers[0])),
                        brf_drivers, autoadd_cb, driver_cb,
                        /*subcmd_name*/NULL, /*subcmd_cb*/NULL,
                        system_cb,
                        /*usage_cb*/NULL,
                        /*data*/NULL));
}


//
// 'autoadd_cb()' - Determine the proper driver for a given printer.
//

static const char *			// O - Driver name or `NULL` for none
autoadd_cb(const char *device_info,	// I - Device information/name (not used)
           const char *device_uri,	// I - Device URI
           const char *device_id,	// I - IEEE-1284 device ID
           void       *cbdata)		// I - Callback data (System)
{
  int		i,			// Looping var
		score,			// Current driver match score
	    	best_score = 0,		// Best score
		num_did;		// Number of device ID key/value pairs
  cups_option_t	*did;			// Device ID key/value pairs
  const char	*make,			// Manufacturer name
		*best_name = NULL;	// Best driver
 


  (void)device_info;

  // First parse the device ID and get any potential driver name to match...
  num_did = papplDeviceParseID(device_id, &did);

  if ((make = cupsGetOption("MANUFACTURER", num_did, did)) == NULL)
    if ((make = cupsGetOption("MANU", num_did, did)) == NULL)
      make = cupsGetOption("MFG", num_did, did);

  
  // // Then loop through the driver list to find the best match...
  // for (i = 0; i < (int)(sizeof(brf_drivers) / sizeof(brf_drivers[0])); i ++)
  {
 

    if (brf_drivers[i].device_id)
    {
      // See if we have a matching device ID...
      score = match_id(num_did, did, brf_drivers[i].device_id);
      if (score > best_score)
      {
        best_score = score;
        best_name  = brf_drivers[i].name;
      }
    }
   }

  // Clean up and return...
  cupsFreeOptions(num_did, did);

  return (best_name);
}



//
// 'match_id()' - Compare two IEEE-1284 device IDs and return a score.
//
// The score is 2 for each exact match and 1 for a partial match in a comma-
// delimited field.  Any non-match results in a score of 0.
//

static int				// O - Score
match_id(int           num_did,		// I - Number of device ID key/value pairs
         cups_option_t *did,		// I - Device ID key/value pairs
         const char    *match_id)	// I - Driver's device ID match string
{
  int		i,			// Looping var
		score = 0,		// Score
		num_mid;		// Number of match ID key/value pairs
  cups_option_t	*mid,			// Match ID key/value pairs
		*current;		// Current key/value pair
  const char	*value,			// Device ID value
		*valptr;		// Pointer into value


  // Parse the matching device ID into key/value pairs...
  if ((num_mid = papplDeviceParseID(match_id, &mid)) == 0)
    return (0);

  // Loop through the match pairs to find matches (or not)
  for (i = num_mid, current = mid; i > 0; i --, current ++)
  {
    if ((value = cupsGetOption(current->name, num_did, did)) == NULL)
    {
      // No match
      score = 0;
      break;
    }

    if (!strcasecmp(current->value, value))
    {
      // Full match!
      score += 2;
    }
    else if ((valptr = strstr(value, current->value)) != NULL)
    {
      // Possible substring match, check
      size_t mlen = strlen(current->value);
					// Length of match value
      if ((valptr == value || valptr[-1] == ',') && (!valptr[mlen] || valptr[mlen] == ','))
      {
        // Partial match!
        score ++;
      }
      else
      {
        // No match
        score = 0;
        break;
      }
    }
    else
    {
      // No match
      score = 0;
      break;
    }
  }
}

//
// 'driver_cb()' - Main driver callback.
//

static bool				// O - `true` on success, `false` on error
driver_cb(
    pappl_system_t         *system,	// I - System
    const char             *driver_name,// I - Driver name
    const char             *device_uri,	// I - Device URI
    const char             *device_id,	// I - 1284 device ID
    pappl_pr_driver_data_t *data,	// I - Pointer to driver data
    ipp_t                  **attrs,	// O - Pointer to driver attributes
    void                   *cbdata)	// I - Callback data (not used)
{
  int	i;				// Looping var


  // Copy make/model info...
  for (i = 0; i < (int)(sizeof(brf_drivers) / sizeof(brf_drivers[0])); i ++)
  {
    if (!strcmp(driver_name, brf_drivers[i].name))
    {
      papplCopyString(data->make_and_model, brf_drivers[i].description, sizeof(data->make_and_model));
      break;
    }
  }

  // Pages per minute 
  data->ppm = 60;

 
  // Color values...
  data->color_supported   = PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME;
  data->color_default   = PAPPL_COLOR_MODE_MONOCHROME;
  data->raster_types  = PAPPL_PWG_RASTER_TYPE_BLACK_8; // to be done just guess

  // "print-quality-default" value...
  data->quality_default = IPP_QUALITY_NORMAL;
  data->orient_default  = IPP_ORIENT_NONE;

  // "sides" values...
  data->sides_supported = PAPPL_SIDES_ONE_SIDED;
  data->sides_default   = PAPPL_SIDES_ONE_SIDED;

  // "orientation-requested-default" value...
  data->orient_default = IPP_ORIENT_NONE;


  // Test page callback...
 // data->testpage_cb = lprintTestPageCB;

  // Use the corresponding sub-driver callback to set things up...
  if (!strncmp(driver_name, "gen_", 4))
    return (brf_gen(system, driver_name, device_uri, device_id, data, attrs, cbdata));
 
  else
    return (false);
}

//
// 'mime_cb()' - MIME typing callback...
//

static const char *			// O - MIME media type or `NULL` if none
mime_cb(const unsigned char *header,	// I - Header data
        size_t              headersize,	// I - Size of header data
        void                *cbdata)	// I - Callback data (not used)
{




    return (brf_TESTPAGE_MIMETYPE);
  
}


//
// 'printer_cb()' - Try auto-adding printers.
//

static bool				// O - `false` to continue
printer_cb(const char     *device_info,	// I - Device information
	   const char     *device_uri,	// I - Device URI
	   const char     *device_id,	// I - IEEE-1284 device ID
	   pappl_system_t *system)	// I - System
{
  const char *driver_name = autoadd_cb(device_info, device_uri, device_id, system);
					// Driver name, if any

  if (driver_name)
  {
    char	name[128],		// Printer name
		*nameptr;		// Pointer in name
    papplCopyString(name, device_info, sizeof(name));

    if ((nameptr = strstr(name, " (")) != NULL)
      *nameptr = '\0';

    if (!papplPrinterCreate(system, 0, name, driver_name, device_id, device_uri))
    {
      // Printer already exists with this name, so try adding a number to the
      // name...
      int	i;			// Looping var
      char	newname[128],		// New name
		number[4];		// Number string
      size_t	namelen = strlen(name),	// Length of original name string
		numberlen;		// Length of number string

      for (i = 2; i < 100; i ++)
      {
        // Append " NNN" to the name, truncating the existing name as needed to
        // include the number at the end...
        snprintf(number, sizeof(number), " %d", i);
        numberlen = strlen(number);

        papplCopyString(newname, name, sizeof(newname));
        if ((namelen + numberlen) < sizeof(newname))
          memcpy(newname + namelen, number, numberlen + 1);
        else
          memcpy(newname + sizeof(newname) - numberlen - 1, number, numberlen + 1);

        // Try creating with this name...
        if (papplPrinterCreate(system, 0, newname, driver_name, device_id, device_uri))
          break;
      }
    }
  }

  return (false);
}


//
// 'system_cb()' - Setup the system object.
//

static pappl_system_t *			// O - System object
system_cb(
    int           num_options,		// I - Number options
    cups_option_t *options,		// I - Options
    void          *data)		// I - Callback data (unused)
{
  pappl_system_t	*system;	// System object
  const char		*val,		// Current option value
			*hostname,	// Hostname, if any
			*logfile,	// Log file, if any
			*system_name;	// System name, if any
  pappl_loglevel_t	loglevel;	// Log level
  int			port = 0;	// Port number, if any
  pappl_soptions_t	soptions = PAPPL_SOPTIONS_MULTI_QUEUE | PAPPL_SOPTIONS_WEB_INTERFACE | PAPPL_SOPTIONS_WEB_LOG | PAPPL_SOPTIONS_WEB_SECURITY;
					// System options
  static pappl_version_t versions[1] =	// Software versions
  {
    { "brf", "", 1.0, 1.0}};


  // Parse standard log and server options...
  if ((val = cupsGetOption("log-level", num_options, options)) != NULL)
  {
    if (!strcmp(val, "fatal"))
      loglevel = PAPPL_LOGLEVEL_FATAL;
    else if (!strcmp(val, "error"))
      loglevel = PAPPL_LOGLEVEL_ERROR;
    else if (!strcmp(val, "warn"))
      loglevel = PAPPL_LOGLEVEL_WARN;
    else if (!strcmp(val, "info"))
      loglevel = PAPPL_LOGLEVEL_INFO;
    else if (!strcmp(val, "debug"))
      loglevel = PAPPL_LOGLEVEL_DEBUG;
    else
    {
      fprintf(stderr, "brf: Bad log-level value '%s'.\n", val);
      return (NULL);
    }
  }
  else
    loglevel = PAPPL_LOGLEVEL_UNSPEC;

  logfile     = cupsGetOption("log-file", num_options, options);
  hostname    = cupsGetOption("server-hostname", num_options, options);
  system_name = cupsGetOption("system-name", num_options, options);

  if ((val = cupsGetOption("server-port", num_options, options)) != NULL)
  {
    if (!isdigit(*val & 255))
    {
      fprintf(stderr, "brf: Bad server-port value '%s'.\n", val);
      return (NULL);
    }
    else
      port = atoi(val);
  }

  // State file...
  if ((val = getenv("SNAP_DATA")) != NULL)
  {
    snprintf(brf_statefile, sizeof(brf_statefile), "%s/brf.conf", val);
  }
  else if ((val = getenv("XDG_DATA_HOME")) != NULL)
  {
    snprintf(brf_statefile, sizeof(brf_statefile), "%s/.brf.conf", val);
  }
#ifdef _WIN32
  else if ((val = getenv("USERPROFILE")) != NULL)
  {
    snprintf(brf_statefile, sizeof(brf_statefile), "%s/AppData/Local/brf.conf", val);
  }
  else
  {
    papplCopyString(brf_statefile, "/brf.ini", sizeof(brf_statefile));
  }
#else
  else if ((val = getenv("HOME")) != NULL)
  {
    snprintf(brf_statefile, sizeof(brf_statefile), "%s/.brf.conf", val);
  }
  else
  {
    papplCopyString(brf_statefile, "/etc/brf.conf", sizeof(brf_statefile));
  }
#endif // _WIN32

  // Create the system object...
  if ((system = papplSystemCreate(soptions, system_name ? system_name : "brf", port, "_print,_universal", cupsGetOption("spool-directory", num_options, options), logfile ? logfile : "-", loglevel, cupsGetOption("auth-service", num_options, options), /* tls_only */false)) == NULL)
    return (NULL);

  papplSystemAddListeners(system, NULL);
  papplSystemSetHostName(system, hostname);

  papplSystemSetMIMECallback(system, mime_cb, NULL);
  //papplSystemAddMIMEFilter(system, brf_TESTPAGE_MIMETYPE, "image/pwg-raster", lprintTestFilterCB, NULL);

  papplSystemSetPrinterDrivers(system, (int)(sizeof(brf_drivers) / sizeof(brf_drivers[0])), brf_drivers, autoadd_cb, /*create_cb*/NULL, driver_cb, system);

  
  papplSystemSetFooterHTML(system, "Copyright &copy; 2019-2021 by Michael R Sweet. All rights reserved.");
  papplSystemSetSaveCallback(system, (pappl_save_cb_t)papplSystemSaveState, (void *)brf_statefile);
  papplSystemSetVersions(system, (int)(sizeof(versions) / sizeof(versions[0])), versions);

  fprintf(stderr, "brf: statefile='%s'\n", brf_statefile);

  if (!papplSystemLoadState(system, brf_statefile))
  {
    // No old state, use defaults and auto-add printers...
    papplSystemSetDNSSDName(system, system_name ? system_name : "brf");

    papplLog(system, PAPPL_LOGLEVEL_INFO, "Auto-adding printers...");
    papplDeviceList(PAPPL_DEVTYPE_USB, (pappl_device_cb_t)printer_cb, system, papplLogDevice, system);
  }

  return (system);
}


