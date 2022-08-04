# include <pappl/pappl.h>
# include <ppd/ppd.h>
# include <cupsfilters/log.h>
# include <cupsfilters/filter.h>
# include <cups/cups.h>
# include <string.h>
# include <limits.h>

//
// Local functions...
//

static bool   brf_callback(pappl_system_t *system, const char *driver_name,
			  const char *device_uri,
			  pappl_driver_data_t *driver_data,
			  ipp_t **driver_attrs, void *data);
static void   brf_setup(pappl_system_t *system);

//
// 'main()' - Main entry for the brf-printer-app.
//

int
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  papplMainloop(argc, argv, "1.0", NULL, 0, NULL, NULL, NULL, NULL, NULL, system_cb, NULL, NULL);
  return (0);
}















//
// 'pcl_setup()' - Setup PCL drivers.
//

static void
brf_setup(
    pappl_system_t *system)      // I - System
{
  static const char * const names[] =   // Driver names
  {
    "hp_deskjet",
    "hp_generic",
    "hp_laserjet"
  };

  static const char * const desc[] =    // Driver descriptions
  {
    "HP Deskjet",
    "Generic PCL",
    "HP Laserjet"
  };

  papplSystemSetPrintDrivers(system, (int)(sizeof(names) / sizeof(names[0])), names, desc, pcl_callback, "hp_printer_app");
}
