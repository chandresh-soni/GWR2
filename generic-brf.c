

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

  if (strstr(driver_name, "-cutter"))
    data->finishings |= PAPPL_FINISHINGS_TRIM;

  if (!strncmp(driver_name, "epl2_2inch-", 16))
  {
    // 2 inch printer...
    data->num_media = (int)(sizeof(lprint_epl2_2inch_media) / sizeof(lprint_epl2_2inch_media[0]));
    memcpy(data->media, lprint_epl2_2inch_media, sizeof(lprint_epl2_2inch_media));

    papplCopyString(data->media_default.size_name, "oe_2x3-label_2x3in", sizeof(data->media_default.size_name));
    data->media_default.size_width  = 2 * 2540;
    data->media_default.size_length = 3 * 2540;
  }
  else
  {
    // 4 inch printer...
    data->num_media = (int)(sizeof(lprint_epl2_4inch_media) / sizeof(lprint_epl2_4inch_media[0]));
    memcpy(data->media, lprint_epl2_4inch_media, sizeof(lprint_epl2_4inch_media));

    papplCopyString(data->media_default.size_name, "na_index-4x6_4x6in", sizeof(data->media_default.size_name));
    data->media_default.size_width  = 4 * 2540;
    data->media_default.size_length = 6 * 2540;
  }

  data->bottom_top = data->left_right = 1;

  data->num_source = 1;
  data->source[0]  = "main-roll";

  data->num_type = 3;
  data->type[0]  = "continuous";
  data->type[1]  = "labels";
  data->type[2]  = "labels-continuous";

  data->media_default.bottom_margin = data->bottom_top;
  data->media_default.left_margin   = data->left_right;
  data->media_default.right_margin  = data->left_right;
  papplCopyString(data->media_default.source, "main-roll", sizeof(data->media_default.source));
  data->media_default.top_margin = data->bottom_top;
  data->media_default.top_offset = 0;
  data->media_default.tracking   = PAPPL_MEDIA_TRACKING_MARK;
  papplCopyString(data->media_default.type, "labels", sizeof(data->media_default.type));

  data->media_ready[0] = data->media_default;

  data->mode_configured = PAPPL_LABEL_MODE_TEAR_OFF;
  data->mode_configured = PAPPL_LABEL_MODE_APPLICATOR | PAPPL_LABEL_MODE_CUTTER | PAPPL_LABEL_MODE_CUTTER_DELAYED | PAPPL_LABEL_MODE_KIOSK | PAPPL_LABEL_MODE_PEEL_OFF | PAPPL_LABEL_MODE_PEEL_OFF_PREPEEL | PAPPL_LABEL_MODE_REWIND | PAPPL_LABEL_MODE_RFID | PAPPL_LABEL_MODE_TEAR_OFF;

  data->speed_default      = 0;
  data->speed_supported[0] = 2540;
  data->speed_supported[1] = 6 * 2540;

  data->darkness_configured = 50;
  data->darkness_supported  = 30;

  return (true);
}


//
// 'lprint_epl2_print()' - Print a file.
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
// 'lprint_epl2_rendjob()' - End a job.
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
// 'lprint_epl2_rendpage()' - End a page.
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
// 'lprint_epl2_rstartjob()' - Start a job.
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
// 'lprint_epl2_rstartpage()' - Start a page.
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
// 'lprint_epl2_status()' - Get current printer status.
//

static bool				// O - `true` on success, `false` on failure
brf_gen_status(
    pappl_printer_t *printer)		// I - Printer
{
  (void)printer;

  return (true);
}