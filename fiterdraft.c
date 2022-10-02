#include <pappl/pappl.h>
bool
brf_filter(
    pappl_job_t    *job,		// I - Job
    pappl_device_t *device,		// I - Device
    void *data)                         // I - Global data
{
  int                   i;
  pr_printer_app_global_data_t *global_data =
    (pr_printer_app_global_data_t *)data;
  pr_cups_device_data_t *device_data = NULL;
  pr_job_data_t         *job_data;      // PPD data for job
  ppd_filter_data_ext_t *filter_data_ext;
  ppd_file_t            *ppd;           // PPD of the printer
  const char            *informat;
  const char		*filename;	// Input filename
  int			fd;		// Input file descriptor
  pr_spooling_conversion_t *conversion; // Spooling conversion to use
                                        // for pre-filtering
  char                  *filter_path = NULL; // Filter from PPD to use for
                                        // this job
  int                   nullfd;         // File descriptor for /dev/null
  pappl_pr_options_t	*job_options;	// Job options
  bool			ret = false;	// Return value
  ppd_filter_external_cups_t* ppd_filter_params = NULL; // Parameters for CUPS
                                        // filter defined in the PPD
  pr_print_filter_function_data_t *print_params; // Paramaters for
                                        // pr_print_filter_function()
  cf_filter_filter_in_chain_t banner_filter = // cfFilterBannerToPDF() filter
                                        // function in filter chain, mainly for
  {                                     // PDF test pages
    cfFilterBannerToPDF,
    NULL,
    "bannertopdf"
  };
  int                   is_banner = 0;  // Do we have cfFilterBannerToPDF()
                                        // instructions in our PDF input file


  //
  // Load the printer's assigned PPD file, and find out which PPD option
  // seetings correspond to our job options
  //

  job_options = papplJobCreatePrintOptions(job, INT_MAX, 1);

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
	      "Printing job in spooling mode");

  job_data = pr_create_job_data(job, job_options);
  filter_data_ext =
    (ppd_filter_data_ext_t *)cfFilterDataGetExt(job_data->filter_data,
						PPD_FILTER_DATA_EXT);
  ppd = filter_data_ext->ppd;

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
	 (pr_spooling_conversion_t *)
	 cupsArrayFirst(global_data->config->spooling_conversions);
       conversion;
       conversion =
	 (pr_spooling_conversion_t *)
	 cupsArrayNext(global_data->config->spooling_conversions))
  {
    if (strcmp(conversion->srctype, informat) != 0)
      continue;
    if ((filter_path =
	 pr_ppd_find_cups_filter(conversion->dsttype,
				 ppd->num_filters, ppd->filters,
				 global_data->filter_dir)) != NULL)
      break;
  }

  if (conversion == NULL || filter_path == NULL)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR,
		"No pre-filter found for input format %s",
		informat);
    return (false);
  }

  // Set input and output formats for the filter chain
  job_data->filter_data->content_type = conversion->srctype;
  job_data->filter_data->final_content_type = conversion->dsttype;

  // Convert PPD file data into printer IPP attributes and options,
  // for the filter functions being able to use it
  ppdFilterLoadPPD(job_data->filter_data);

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
	      "Converting input file to format: %s", conversion->dsttype);
  if (filter_path[0] == '.')
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		"Passing on PostScript directly to printer");
  else if (filter_path[0] == '-')
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		"Passing on %s directly to printer", conversion->dsttype);
  else
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		"Using CUPS filter (printer driver): %s", filter_path);

  //
  // Connect the job's filter_data to the backend
  //

  if (strncmp(job_data->device_uri, "cups:", 5) == 0)
  {
    // Get the device data
    device_data = (pr_cups_device_data_t *)papplDeviceGetData(device);

    // Connect the filter_data
    device_data->filter_data = job_data->filter_data;
  }

  //
  // Check whether the PDF input is a banner or test page
  //

  if (strcmp(informat, "application/pdf") == 0 ||
      strcmp(informat, "application/vnd.cups-pdf") == 0)
  {
    FILE *fp;
    char *line = NULL;
    size_t len = 0;

    fp = fdopen(fd, "r");
    while (getline(&line, &len, fp) != -1)
      if (strncmp(line, "%%#PDF-BANNER", 13) == 0 ||
	  strncmp(line, "%%PDF-BANNER", 12) == 0)
      {
	papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		    "Input PDF file is banner or test page file, calling bannertopdf to add printer and job information");
	is_banner = 1;
	job_data->filter_data->content_type = "application/vnd.cups-pdf-banner";
	break;
      }
    rewind(fp);
    free(line);
  }

  //
  // Set up filter function chain
  //

  job_data->chain = cupsArrayNew(NULL, NULL);
  if (is_banner)
    cupsArrayAdd(job_data->chain, &banner_filter);
  for (i = 0; i < conversion->num_filters; i ++)
    cupsArrayAdd(job_data->chain, &(conversion->filters[i]));
  if (strlen(filter_path) > 1) // A null filter is a single char, '-'
                               // or '.', whereas an actual filter has
                               // a path starting with '/', so at
                               // least 2 chars.
  {
    ppd_filter_params =
      (ppd_filter_external_cups_t *)calloc(1, sizeof(ppd_filter_external_cups_t));
    ppd_filter_params->filter = filter_path;
    job_data->ppd_filter =
      (cf_filter_filter_in_chain_t *)calloc(1, sizeof(cf_filter_filter_in_chain_t));
    job_data->ppd_filter->function = ppdFilterExternalCUPS;
    job_data->ppd_filter->parameters = ppd_filter_params;
    job_data->ppd_filter->name = strrchr(filter_path, '/') + 1;
    cupsArrayAdd(job_data->chain, job_data->ppd_filter);
  } else
    job_data->ppd_filter = NULL;
  job_data->print =
    (cf_filter_filter_in_chain_t *)calloc(1, sizeof(cf_filter_filter_in_chain_t));
  // Put filter function to send data to PAPPL's built-in backend at the end
  // of the chain
  print_params =
    (pr_print_filter_function_data_t *)
    calloc(1, sizeof(pr_print_filter_function_data_t));
  print_params->device = device;
  print_params->device_uri = job_data->device_uri;
  print_params->job = job;
  print_params->global_data = global_data;
  job_data->print->function = pr_print_filter_function;
  job_data->print->parameters = print_params;
  job_data->print->name = "Backend";
  cupsArrayAdd(job_data->chain, job_data->print);

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

  if (cfFilterChain(fd, nullfd, 1, job_data->filter_data, job_data->chain) == 0)
    ret = true;

  //
  // Update status
  //

  pr_update_status(papplJobGetPrinter(job), device);

  //
  // Stop the backend and disconnect the job's filter_data from the backend
  //

  if (strncmp(job_data->device_uri, "cups:", 5) == 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
		"Shutting down CUPS backend");

    // Stop the backend
    // We stop it here explicitly as we will free the filter_data structure
    // and without it the backend shutdoen will not have access to the log
    // function any more.
    pr_cups_dev_stop_backend(device);

    // Get the device data
    device_data = (pr_cups_device_data_t *)papplDeviceGetData(device);

    // Disconnect the filter_data
    device_data->filter_data = NULL;
  }

  //