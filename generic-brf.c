

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
  "110x115/11x11.5",
  "110x120/11x12",
  "110x170/11x17",
  "115x110/11.5x11",
  "120x120/12x12",
  "A4TF/A4 Tractor Feed"
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
  data->x_resolution[0] = 200;
  data->y_resolution[0] = 200;

  // data->x_resolution[1] = 300;
  // data->y_resolution[1] = 300;

  data->x_default = data->y_default = data->x_resolution[0];

  
  data->num_media = (int)(sizeof(brf_generic_media) / sizeof(brf_generic_media[0]));
  memcpy(data->media, brf_generic_media, sizeof(brf_generic_media));

  data->bottom_top = data->left_right = 1;
  
  data->media_default.bottom_margin = data->bottom_top;
  data->media_default.left_margin   = data->left_right;
  data->media_default.right_margin  = data->left_right;

  data->media_default.top_margin = data->bottom_top;


  data->media_ready[0] = data->media_default;





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
