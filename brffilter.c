
#include <pappl/pappl.h>
#include <cupsfilters/log.h>
#include <cupsfilters/filter.h>
#include <ppd/ppd-filter.h>
#include <limits.h>

//
// 'BRFTestFilterCB()' - Print a test page.
//

// Items to configure the properties of this Printer Application
// These items do not change while the Printer Application is running
typedef struct brf_printer_app_config_s
{
  // Identification of the Printer Application
  const char        *system_name;        // Name of the system
  const char        *system_package_name;// Name of Printer Application
                                         // package/executable
  const char        *version;            // Program version number string
  unsigned short    numeric_version[4];  // Numeric program version
  const char        *web_if_footer;      // HTML Footer for web interface


  // Callback functions

  // Auto-add (automatic driver assignment) callback
  pappl_pr_autoadd_cb_t autoadd_cb;

  // Printer identify callback (Printer makes noise, lights up display, ...
  // without printing, to find printer under several others)
  pappl_pr_identify_cb_t identify_cb;

  // Print a test page (To check whether configuration is OK)
  pappl_pr_testpage_cb_t testpage_cb;


  // Spooling conversion paths (input and output mime type, filter function,
  // parameters), more desired (simpler) conversions first, less desired
  // later (first match in list gets used)
  cups_array_t      *spooling_conversions;

  // Supported data formats to get from streaning Raster input and the
  // needed callback functions (output mime type, 5 callback functions:
  // start/end job, start/emd page, output raster line), more desired formats
  // (streamability) first: CUPS Raster, PosdtScript, PDF (we will actually
  // send PCLm, so that at least some printers stream).
  cups_array_t      *stream_formats;

  // CUPS backends to be ignored (comman-separated list, empty or NULL
  // for allowing all backends)
  const char        *backends_ignore;

  // CUPS backends to use exclusively (comman-separated list, empty or
  // NULL for including all backends)
  const char        *backends_only;

  // Data for the test page callback function
  // For pr_testpage() this is simply the file name of the only one test
  // page without directory
  void              *testpage_data;

} pr_printer_app_config_t;

// Global variables for this Printer Application.
// Note that the Printer Application can only run one system at the same time
// Items adjustable by command line options and environment variables and also
// values obtained at run time
typedef struct brf_printer_app_global_data_s
{
  pr_printer_app_config_t *config;
  pappl_system_t          *system;
  int                     num_drivers;     // Number of drivers (from the PPDs)
  pappl_pr_driver_t       *drivers;        // Driver index (for menu and
                                           // auto-add)
   char              spool_dir[1024];     // Spool directory, customizable via
                                         // SPOOL_DIR environment variable                                         

} brf_printer_app_global_data_t;



// Data for brf_print_filter_function()
typedef struct brf_print_filter_function_data_s
// look-up table
{
  pappl_device_t *device;                    // Device
  char *device_uri;                          // Printer device URI
  pappl_job_t *job;                          // Job
  brf_printer_app_global_data_t *global_data; // Global data
} brf_print_filter_function_data_t;

typedef struct brf_cups_device_data_s
{
  char *device_uri;    // Device URI
  int inputfd,         // FD for job data input
      backfd,          // FD for back channel
      sidefd;          // FD for side channel
  int backend_pid;     // PID of CUPS backend
  double back_timeout, // Timeout back channel (sec)
      side_timeout;    // Timeout side channel (sec)

  cf_filter_filter_in_chain_t *chain_filter; // Filter from PPD file
  cf_filter_data_t *filter_data;             // Common data for filter functions
  ppd_filter_external_cups_t backend_params; // Parameters for launching
                                             // backend via ppdFilterExternalCUPS()
  bool internal_filter_data;                 // Is filter_data
                                             // internal?
} brf_cups_device_data_t;

typedef struct brf_spooling_conversion_s
{
  const char *srctype;                   // Input data type
  const char *dsttype;                   // Output data type
  int num_filters;                       // Number of filters
  cf_filter_filter_in_chain_t filters[]; // List of filters with
                                         // parameters
} brf_spooling_conversion_t;

static brf_spooling_conversion_t brf_convert_pdf_to_brf =
    {
        "application/pdf",
        "application/vnd.cups-paged-brf",
        1,
        {{ppdFilterExternalCUPS,
          NULL,
          "berftotext"}}};
bool // O - `true` on success, `false` on failure
BRFTestFilterCB(
    pappl_job_t *job,       // I - Job
    pappl_device_t *device, // I - Output device
    void *cbdata)           // I - Callback data (not used)
{
  pappl_pr_options_t *job_options = papplJobCreatePrintOptions(job, INT_MAX, 0);
  brf_spooling_conversion_t *conversion;     // Spooling conversion to use
                                             // for pre-filtering
  cf_filter_filter_in_chain_t *chain_filter, // Filter from PPD file
      *print;
  brf_cups_device_data_t *device_data = NULL;
  ppd_filter_data_ext_t *filter_data_ext;
  brf_print_filter_function_data_t *print_params;
  brf_printer_app_global_data_t *global_data;
  cf_filter_data_t *filter_data;
  cups_array_t *spooling_conversions;
  cups_array_t *chain;
  const char *informat;
  const char *filename;     // Input filename
  int fd;                   // Input file descriptor

  int nullfd;               // File descriptor for /dev/null

  bool ret = false;    // Return value
  int num_options = 0; // Number of PPD print options
  cups_option_t *options = NULL;
  ppd_filter_external_cups_t *ppd_filter_params = NULL;

  pappl_pr_driver_data_t driver_data;
  pappl_printer_t *printer = papplJobGetPrinter(job);
  char device_uri = papplPrinterGetDeviceURI(printer);

  // Prepare job data to be supplied to filter functions/CUPS filters
  // called during job execution
  filter_data = (cf_filter_data_t *)calloc(1, sizeof(cf_filter_data_t));
  // job_data->filter_data = filter_data;
  filter_data->printer = strdup(papplPrinterGetName(printer));
  filter_data->job_id = papplJobGetID(job);
  filter_data->job_user = strdup(papplJobGetUsername(job));
  filter_data->job_title = strdup(papplJobGetName(job));
  filter_data->copies = job_options->copies;
  filter_data->job_attrs = NULL;     // We use PPD/filter options
  filter_data->printer_attrs = NULL; // We use the printer's PPD file
  filter_data->num_options = num_options;
  filter_data->options = options; // PPD/filter options
  filter_data->extension = NULL;
  filter_data->back_pipe[0] = -1;

  filter_data->back_pipe[1] = -1;
  filter_data->side_pipe[0] = -1;
  filter_data->side_pipe[1] = -1;
  filter_data->logdata = job;
  filter_data->iscanceledfunc = papplJobIsCanceled(job); // Function to indicate
                                                         // whether the job got
                                                         // canceled
  filter_data->iscanceleddata = job;
  // //
  // // Load the printer's assigned PPD file, and find out which PPD option
  // // seetings correspond to our job options
  // //

  // papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
  //             "Printing job in spooling mode");

  // filter_data_ext =
  //     (ppd_filter_data_ext_t *)cfFilterDataGetExt(filter_data,
  //                                                 PPD_FILTER_DATA_EXT);

  //
  // Open the input file...
  //

  filename = papplJobGetFilename(job);
  if ((fd = open(filename, O_RDONLY)) < 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open input file '%s' for printing: %s",
                filename, strerror(errno));
    return (false);
  }

  //
  // Get input file format
  //

  informat = papplJobGetFormat(job);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
              "Input file format: %s", informat);

  //
  // Find filters to use for this job
  //

  for (conversion =
           (brf_spooling_conversion_t *)
               cupsArrayFirst(spooling_conversions);
       conversion;
       conversion =
           (brf_spooling_conversion_t *)
               cupsArrayNext(spooling_conversions))
  {
    if (strcmp(conversion->srctype, informat) == 0)
      break;
    
  }
  if (conversion == NULL )
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR,
                "No pre-filter found for input format %s",
                informat);
    return (false);
  }
  // Set input and output formats for the filter chain
  filter_data->content_type = conversion->srctype;
  filter_data->final_content_type = conversion->dsttype;

  //
  // Connect the job's filter_data to the backend
  //

  if (strncmp(device_uri, "cups:", 5) == 0)
  {
    // Get the device data
    device_data = (brf_cups_device_data_t *)papplDeviceGetData(device);

    // Connect the filter_data
    device_data->filter_data = filter_data;
  }

  //
  // Set up filter function chain
  //

  chain = cupsArrayNew(NULL, NULL);

  for (int i = 0; i < conversion->num_filters; i++)
    cupsArrayAdd(chain, &(conversion->filters[i]));
    
    chain_filter = NULL;
  print =
      (cf_filter_filter_in_chain_t *)calloc(1, sizeof(cf_filter_filter_in_chain_t));
  // Put filter function to send data to PAPPL's built-in backend at the end
  // of the chain
  print_params =
      (brf_print_filter_function_data_t *)
          calloc(1, sizeof(brf_print_filter_function_data_t));
  print_params->device = device;
  print_params->device_uri = device_uri;
  print_params->job = job;
  print_params->global_data = global_data;
  print->function = brf_print_filter_function;
  print->parameters = print_params;
  print->name = "Backend";
  cupsArrayAdd(chain, print);

  //
  // Update status
  //

  pr_update_status(papplJobGetPrinter(job), device);

  //
  // Fire up the filter functions
  //

  papplJobSetImpressions(job, 1);

  // The filter chain has no output, data is going to the device
  nullfd = open("/dev/null", O_RDWR);

  if (cfFilterChain(fd, nullfd, 1, filter_data, chain) == 0)
    ret = true;

  //
  // Update status
  //

  pr_update_status(papplJobGetPrinter(job), device);

 

  return (ret);
}

int                                               // O - Error status
brf_print_filter_function(int inputfd,            // I - File descriptor input
                                                  //     stream
                          int outputfd,           // I - File descriptor output
                                                  //     stream (unused)
                          int inputseekable,      // I - Is input stream
                                                  //     seekable? (unused)
                          cf_filter_data_t *data, // I - Job and printer data
                          void *parameters)       // I - PAPPL output device
{
  ssize_t bytes;                    // Bytes read/written
  char buffer[65536];               // Read/write buffer
  cf_logfunc_t log = data->logfunc; // Log function
  void *ld = data->logdata;         // log function data
  brf_print_filter_function_data_t *params =
      (brf_print_filter_function_data_t *)parameters;
  pappl_device_t *device = params->device; // PAPPL output device
  pappl_job_t *job = params->job;
  pappl_printer_t *printer;
  brf_printer_app_global_data_t *global_data = params->global_data;
  char filename[2048]; // Name for debug copy of the
                       // job
  int debug_fd = -1;   // File descriptor for debug copy

  (void)inputseekable;

  // Remove debug copies of old jobs
  pr_clean_debug_copies(global_data);

  if (papplSystemGetLogLevel(global_data->system) == PAPPL_LOGLEVEL_DEBUG)
  {
    // We are in debug mode
    // Debug copy file name (in spool directory)
    printer = papplJobGetPrinter(job);
    snprintf(filename, sizeof(filename), "%s/debug-jobdata-%s-%d.prn",
             global_data->spool_dir, papplPrinterGetName(printer),
             papplJobGetID(job));
    if (log)
      log(ld, CF_LOGLEVEL_DEBUG,
          "Backend: Creating debug copy of what goes to the printer: %s", filename);
    // Open the file
    debug_fd = open(filename, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
  }

  while ((bytes = read(inputfd, buffer, sizeof(buffer))) > 0)
  {
    if (debug_fd >= 0)
      if (write(debug_fd, buffer, (size_t)bytes) != bytes)
      {
        if (log)
          log(ld, CF_LOGLEVEL_ERROR,
              "Backend: Debug copy: Unable to write %d bytes, stopping debug copy, continuing job output.",
              (int)bytes);
        close(debug_fd);
        debug_fd = -1;
      }

    if (papplDeviceWrite(device, buffer, (size_t)bytes) < 0)
    {
      if (log)
        log(ld, CF_LOGLEVEL_ERROR,
            "Backend: Output to device: Unable to send %d bytes to printer.",
            (int)bytes);
      if (debug_fd >= 0)
        close(debug_fd);
      close(inputfd);
      close(outputfd);
      return (1);
    }
  }
  papplDeviceFlush(device);

  if (debug_fd >= 0)
    close(debug_fd);

  close(inputfd);
  close(outputfd);
  return (0);
}