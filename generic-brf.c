

//
// Include necessary headers...
//

#include <pappl/pappl.h>

//
// Local globals...
//


//
// Local functions...
//

static bool	brf_gen_printfile(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	brf_gen_rendjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	brf_gen_rendpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	brf_gen_rstartjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool	brf_gen_rstartpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool	brf_gen_status(pappl_printer_t *printer);

static const char * const brf_generic_media[] =
{       // Supported media sizes for Generic BRF printers
  //to be done
  // "na_letter_8.5x11in",
  // "na_legal_8.5x14in",
  // "executive_7x10in",
  // "na_tabloid_11x17in",
  // "iso_a3_11.7x16.5in",
  // "iso_a4_8.3x11.7in",
  // "iso_a5_5.8x8.3in",
  // "jis_b5_7.2x10.1in",
  // "env_b5_6.9x9.8in",
  // "env_10_4.125x9.5in",
  // "env_c5_6.4x9in",
  // "env_dl_8.66x4.33in",
  // "env_monarch_3.875x7.5in"
};



bool					// O - `true` on success, `false` on error
brf_gen(
    pappl_system_t         *system,	// I - System
    const char             *driver_name,// I - Driver name
    const char             *device_uri,	// I - Device URI
    const char             *device_id,	// I - 1284 device ID
    pappl_pr_driver_data_t *data,	// I - Pointer to driver data
    ipp_t                  **attrs,	// O - Pointer to driver attributes
    void                   *cbdata)	// I - Callback data (not used)
{
  data->printfile_cb  = brf_gen_printfile;
  data->rendjob_cb    = brf_gen_rendjob;
  data->rendpage_cb   = brf_gen_rendpage;
  data->rstartjob_cb  = brf_gen_rstartjob;
  data->rstartpage_cb = brf_gen_rstartpage;
  data->status_cb     = brf_gen_status;
  data->format        = "application/vnd.cups-paged-brf";

  data->num_resolution = 1;

  if (strstr(driver_name, "-203dpi"))
  {
    data->x_resolution[0] = 203;
    data->y_resolution[0] = 203;
  }
  else
  {
    data->x_resolution[0] = 300;
    data->y_resolution[0] = 300;
  }

  data->x_default = data->y_default = data->x_resolution[0];

  
  data->num_media = (int)(sizeof(brf_generic_media) / sizeof(brf_generic_media[0]));
  memcpy(data->media, brf_generic_media, sizeof(brf_generic_media));

  data->bottom_top = data->left_right = 1;
  // to be done
  // data->num_source = 3;
  //   data->source[0]  = "tray-1";
  //   data->source[1]  = "manual";
  //   data->source[2]  = "envelope";

  // data->num_type = 5;
  //   data->type[0] = "stationery";
  //   data->type[1] = "bond";
  //   data->type[2] = "special";
  //   data->type[3] = "transparency";
  //   data->type[4] = "photographic-glossy";

  

  data->media_default.bottom_margin = data->bottom_top;
  data->media_default.left_margin   = data->left_right;
  data->media_default.right_margin  = data->left_right;

  data->media_default.top_margin = data->bottom_top;


  data->media_ready[0] = data->media_default;


  data->speed_default      = 0;
  data->speed_supported[0] = 2540;
  data->speed_supported[1] = 6 * 2540;


  return (true);
}


//
// 'Brf_generic_print()' - Print a file.
//

static bool				// O - `true` on success, `false` on failure
brf_gen_printfile(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  int		fd;			// Input file
  ssize_t	bytes;			// Bytes read/written
  char		buffer[65536];		// Read/write buffer


  // Copy the raw file...
  papplJobSetImpressions(job, 1);

  if ((fd  = open(papplJobGetFilename(job), O_RDONLY)) < 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open print file '%s': %s", papplJobGetFilename(job), strerror(errno));
    return (false);
  }

  while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
  {
    if (papplDeviceWrite(device, buffer, (size_t)bytes) < 0)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to send %d bytes to printer.", (int)bytes);
      close(fd);
      return (false);
    }
  }
  close(fd);

  papplJobSetImpressionsCompleted(job, 1);

  return (true);
}


//
// 'Brf_generic_rendjob()' - End a job.
//

static bool				// O - `true` on success, `false` on failure
brf_gen_rendjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  (void)job;
  (void)options;
  (void)device;

  return (true);
}


//
// 'Brf_generic_rendpage()' - End a page.
//

static bool				// O - `true` on success, `false` on failure
brf_gen_rendpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  (void)job;
  (void)page;

  papplDevicePuts(device, "P1\n");


  return (true);
}


//
// 'Brf_generic_rstartjob()' - Start a job.
//

static bool				// O - `true` on success, `false` on failure
brf_gen_rstartjob(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device)		// I - Output device
{
  (void)job;
  (void)options;
  (void)device;

  return (true);
}


//
// 'Brf_generic_rstartpage()' - Start a page.
//

static bool				// O - `true` on success, `false` on failure
brf_gen_rstartpage(
    pappl_job_t        *job,		// I - Job
    pappl_pr_options_t *options,	// I - Job options
    pappl_device_t     *device,		// I - Output device
    unsigned           page)		// I - Page number
{
  int		ips;			// Inches per second


  (void)job;
  (void)page;

  papplDevicePuts(device, "\nN\n");


 
  return (true);
}




//
// 'Brf_generic_status()' - Get current printer status.
//

static bool				// O - `true` on success, `false` on failure
brf_gen_status(
    pappl_printer_t *printer)		// I - Printer
{
  (void)printer;

  return (true);
}
