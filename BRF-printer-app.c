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
static void   brf_identify(pappl_printer_t *printer, pappl_identify_actions_t actions, const char *message);

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
// 'brf_setup()' - Setup BRF drivers.
//

static void
brf_setup(
    pappl_system_t *system)      // I - System
{
  static const char * const names[] =   // Driver names
  {
    
    "generic",

  };

  static const char * const desc[] =    // Driver descriptions
  {
   
    "Generic",
    
  };

  papplSystemSetPrintDrivers(system, (int)(sizeof(names) / sizeof(names[0])), names, desc, brf_callback, "brf_printer_app");
}

//
// 'pcl_callback()' - PCL callback.
//

static bool				   // O - `true` on success, `false` on failure
pcl_callback(
    pappl_system_t       *system,	   // I - System
    const char           *driver_name,   // I - Driver name
    const char           *device_uri,	   // I - Device URI
    pappl_pdriver_data_t *driver_data,   // O - Driver data
    ipp_t                **driver_attrs, // O - Driver attributes
    void                 *data)	   // I - Callback data
{
  int   i;                               // Looping variable


  if (!driver_name || !device_uri || !driver_data || !driver_attrs)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Driver callback called without required information.");
    return (false);
  }

  if (!data || strcmp((const char *)data, "hp_printer_app"))
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Driver callback called with bad data pointer.");
    return (false);
  }

  driver_data->identify           = brf_identify;
  driver_data->identify_default   = PAPPL_IDENTIFY_ACTIONS_SOUND;
  driver_data->identify_supported = PAPPL_IDENTIFY_ACTIONS_DISPLAY | PAPPL_IDENTIFY_ACTIONS_SOUND;
  driver_data->print              = NULL;//to be decide
  driver_data->rendjob            = NULL;
  driver_data->rendpage           = NULL;
  driver_data->rstartjob          = NULL;
  driver_data->rstartpage         = NULL;
  driver_data->rwrite             = NULL;
  driver_data->status             = brf_status;
  driver_data->format             = "application/vnd.hp-postscript";
  driver_data->orient_default     = IPP_ORIENT_NONE;
  driver_data->quality_default    = IPP_QUALITY_NORMAL;

  if (!strcmp(driver_name, "generic"))
  {
    strlcpy(driver_data->make_and_model, "HP DeskJet", sizeof(driver_data->make_and_model));

    driver_data->num_resolution  = 3;
    driver_data->x_resolution[0] = 150;
    driver_data->y_resolution[0] = 150;
    driver_data->x_resolution[1] = 300;
    driver_data->y_resolution[1] = 300;
    driver_data->x_resolution[2] = 600;
    driver_data->y_resolution[2] = 600;
    driver_data->x_default = driver_data->y_default = 300;

    driver_data->raster_types = PAPPL_PWG_RASTER_TYPE_BLACK_1 | PAPPL_PWG_RASTER_TYPE_BLACK_8 | PAPPL_PWG_RASTER_TYPE_SGRAY_8 | PAPPL_PWG_RASTER_TYPE_SRGB_8;

    driver_data->color_supported = PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_AUTO_MONOCHROME | PAPPL_COLOR_MODE_COLOR | PAPPL_COLOR_MODE_MONOCHROME;
    driver_data->color_default   = PAPPL_COLOR_MODE_AUTO;

    driver_data->num_media = (int)(sizeof(pcl_hp_deskjet_media) / sizeof(pcl_hp_deskjet_media[0]));
    memcpy(driver_data->media, pcl_hp_deskjet_media, sizeof(pcl_hp_deskjet_media));

    driver_data->sides_supported = PAPPL_SIDES_ONE_SIDED;
    driver_data->sides_default   = PAPPL_SIDES_ONE_SIDED;

    driver_data->num_source = 3;
    driver_data->source[0]  = "tray-1";
    driver_data->source[1]  = "manual";
    driver_data->source[2]  = "envelope";

    driver_data->num_type = 5;
    driver_data->type[0] = "stationery";
    driver_data->type[1] = "bond";
    driver_data->type[2] = "special";
    driver_data->type[3] = "transparency";
    driver_data->type[4] = "photographic-glossy";

    driver_data->left_right = 635;	 // 1/4" left and right
    driver_data->bottom_top = 1270;	 // 1/2" top and bottom

    for (i = 0; i < driver_data->num_source; i ++)
    {
      if (strcmp(driver_data->source[i], "envelope"))
        snprintf(driver_data->media_ready[i].size_name, sizeof(driver_data->media_ready[i].size_name), "na_letter_8.5x11in");
      else
        snprintf(driver_data->media_ready[i].size_name, sizeof(driver_data->media_ready[i].size_name), "env_10_4.125x9.5in");
    }
  }
  else
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "No dimension information in driver name '%s'.", driver_name);
    return (false);
  }

  // Fill out ready and default media (default == ready media from the first source)
  for (i = 0; i < driver_data->num_source; i ++)
  {
    pwg_media_t *pwg = pwgMediaForPWG(driver_data->media_ready[i].size_name);

    if (pwg)
    {
      driver_data->media_ready[i].bottom_margin = driver_data->bottom_top;
      driver_data->media_ready[i].left_margin   = driver_data->left_right;
      driver_data->media_ready[i].right_margin  = driver_data->left_right;
      driver_data->media_ready[i].size_width    = pwg->width;
      driver_data->media_ready[i].size_length   = pwg->length;
      driver_data->media_ready[i].top_margin    = driver_data->bottom_top;
      snprintf(driver_data->media_ready[i].source, sizeof(driver_data->media_ready[i].source), "%s", driver_data->source[i]);
      snprintf(driver_data->media_ready[i].type, sizeof(driver_data->media_ready[i].type), "%s", driver_data->type[0]);
    }
  }

  driver_data->media_default = driver_data->media_ready[0];

  return (true);
}







//
// 'brf_identify()' - Identify the printer.
//

static void
brf_identify(
    pappl_printer_t          *printer,	// I - Printer
    pappl_identify_actions_t actions, 	// I - Actions to take
    const char               *message)	// I - Message, if any
{
  (void)printer;
  (void)actions;

  // Identify a printer using display, flash, sound or speech.
}



