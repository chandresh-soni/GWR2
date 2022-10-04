

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
static bool	brf_gen_rwriteline(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);

static const char * const brf_gen_media[] =
{       // Supported media sizes for Generic BRF printers
   "na_legal_8.5x14in",
  "na_letter_8.5x11in",
  "na_executive_7x10in",
  "iso_a4_210x297mm",
  "iso_a5_148x210mm",
  "jis_b5_182x257mm",
  "iso_b5_176x250mm",
  "na_number-10_4.125x9.5in",
  "iso_c5_162x229mm",
  "iso_dl_110x220mm",
  "na_monarch_3.875x7.5in"
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
  data->rwriteline_cb = brf_gen_rwriteline;
  data->status_cb     = brf_gen_status;
  data->format        = "application/vnd.cups-paged-brf";

  data->num_resolution = 1;
  data->x_resolution[0] = 200;
  data->y_resolution[0] = 200;
  // data->x_resolution[1] = 300;
  // data->y_resolution[1] = 300;

  data->x_default = data->y_default = data->x_resolution[0];

  
  data->num_media = (int)(sizeof(brf_gen_media) / sizeof(brf_gen_media[0]));
  memcpy(data->media, brf_gen_media, sizeof(brf_gen_media));
  
    papplCopyString(data->media_default.size_name,"iso_a4_210x297mm", sizeof(data->media_default.size_name));
    data->media_default.size_width  = 1 * 21000;
    data->media_default.size_length = 1 * 29700;
  data->left_right = 635;	 // 1/4" left and right
  data->bottom_top = 1270;	
  


  data->media_default.bottom_margin = data->bottom_top;
  data->media_default.left_margin   = data->left_right;
  data->media_default.right_margin  = data->left_right;

  data->media_default.top_margin = data->bottom_top;
    /* Three paper trays (MSN names) */
    data->num_source = 3;
    data->source[0]  = "tray-1";
    data->source[1]  = "manual";
    data->source[2]  = "envelope";
    //a types (MSN names) */
    data->num_type = 8;
    data->type[0]  = "stationery";
    data->type[1]  = "stationery-inkjet";
    data->type[2]  = "stationery-letterhead";
    data->type[3]  = "cardstock";
    data->type[4]  = "labels";
    data->type[5]  = "envelope";
    data->type[6]  = "transparency";
    data->type[7]  = "photographic";
  papplCopyString(data->media_default.source, "tray-1", sizeof(data->media_default.source));
  papplCopyString(data->media_default.type, "labels", sizeof(data->media_default.type));
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
// 'brf_gen_rwriteline()' - Write a raster line.
//
static bool				// O - `true` on success, `false` on failure
brf_gen_rwriteline(
    pappl_job_t         *job,		// I - Job
    pappl_pr_options_t  *options,	// I - Job options
    pappl_device_t      *device,	// I - Output device
    unsigned            y,		// I - Line number
    const unsigned char *line)		// I - Line
{
  if (line[0] || memcmp(line, line + 1, options->header.cupsBytesPerLine - 1))
  {
    unsigned		i;		// Looping var
    const unsigned char	*lineptr;	// Pointer into line
    unsigned char	buffer[300],
			*bufptr;	// Pointer into buffer

    for (i = options->header.cupsBytesPerLine, lineptr = line, bufptr = buffer; i > 0; i --)
      *bufptr++ = ~*lineptr++;

    papplDevicePrintf(device, "GW0,%u,%u,1\n", y, options->header.cupsBytesPerLine);
    papplDeviceWrite(device, buffer, options->header.cupsBytesPerLine);
    papplDevicePuts(device, "\n");
  }

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
