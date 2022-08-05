
#include "config.h"
#include "filter.h"
#include <limits.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <cups/file.h>
#include <cups/array.h>
#include <ppd/ppd.h>

/*
 * 'cfFilterExternalCUPS()' - Filter function which calls an external,
 *                          classic CUPS filter, for example a
 *                          (proprietary) printer driver which cannot
 *                          be converted to a filter function or is to
 *                          awkward or risky to convert for example
 *                          when the printer hardware is not available
 *                          for testing
 */

int                                     /* O - Error status */
cfFilterExternalCUPS(int inputfd,         /* I - File descriptor input stream */
		   int outputfd,        /* I - File descriptor output stream */
		   int inputseekable,   /* I - Is input stream seekable? */
		   cf_filter_data_t *data, /* I - Job and printer data */
		   void *parameters)    /* I - Filter-specific parameters */
{
  cf_filter_external_cups_t *params = (cf_filter_external_cups_t *)parameters;
  int           i;
  int           is_backend = 0;      /* Do we call a CUPS backend? */
  int		pid,		     /* Process ID of filter */
                stderrpid,           /* Process ID for stderr logging process */
                wpid;                /* PID reported as terminated */
  int		fd;		     /* Temporary file descriptor */
  int           backfd, sidefd;      /* file descriptors for back and side
                                        channels */
  int           stderrpipe[2];       /* Pipe to log stderr */
  cups_file_t   *fp;                 /* File pointer to read log lines */
  char          buf[2048];           /* Log line buffer */
  cf_loglevel_t log_level;       /* Log level of filter's log message */
  char          *ptr1, *ptr2,
                *msg,                /* Filter log message */
                *filter_name;        /* Filter name for logging */
  char          filter_path[1024];   /* Full path of the filter */
  char          **argv,		     /* Command line args for filter */
                **envp = NULL;       /* Environment variables for filter */
  int           num_all_options = 0;
  cups_option_t *all_options = NULL;
  char          job_id_str[16],
                copies_str[16],
                *options_str = NULL;
  cups_option_t *opt;
  int status = 65536;
  int wstatus;
  cf_logfunc_t log = data->logfunc;
  void          *ld = data->logdata;
  cf_filter_iscanceledfunc_t iscanceled = data->iscanceledfunc;
  void          *icd = data->iscanceleddata;


  if (!params->filter || !params->filter[0]) {
    if (log) log(ld, CF_LOGLEVEL_ERROR,
		 "cfFilterExternalCUPS: Filter executable path/command not specified");
    return (1);
  }

  /* Check whether back/side channel FDs are valid and not all-zero
     from calloc'ed filter_data */
  if (data->back_pipe[0] == 0 && data->back_pipe[1] == 0)
    data->back_pipe[0] = data->back_pipe[1] = -1;
  if (data->side_pipe[0] == 0 && data->side_pipe[1] == 0)
    data->side_pipe[0] = data->side_pipe[1] = -1;

  /* Select the correct end of the back/side channel pipes:
     [0] for filters, [1] for backends */
  is_backend = (params->is_backend ? 1 : 0);
  backfd = data->back_pipe[is_backend];
  sidefd = data->side_pipe[is_backend];

  /* Filter name for logging */
  if ((filter_name = strrchr(params->filter, '/')) != NULL)
    filter_name ++;
  else
    filter_name = (char *)params->filter;

 /*
  * Ignore broken pipe signals...
  */

  signal(SIGPIPE, SIG_IGN);

 /*
  * Copy the current environment variables and add some important ones
  * needed for correct execution of the CUPS filter (which is not running
  * out of CUPS here)
  */

  /* Some default environment variables from CUPS, will get overwritten
     if also defined in the environment in which the caller is started
     or in the parameters */
  add_env_var("CUPS_DATADIR", CUPS_DATADIR, &envp);
  add_env_var("CUPS_SERVERBIN", CUPS_SERVERBIN, &envp);
  add_env_var("CUPS_SERVERROOT", CUPS_SERVERROOT, &envp);
  add_env_var("CUPS_STATEDIR", CUPS_STATEDIR, &envp);
  add_env_var("SOFTWARE", "CUPS/2.5.99", &envp); /* Last CUPS with PPDs */

  /* Copy the environment in which the caller got started */
  if (environ)
    for (i = 0; environ[i]; i ++)
      add_env_var(environ[i], NULL, &envp);

  /* Set the environment variables given by the parameters */
  if (params->envp)
    for (i = 0; params->envp[i]; i ++)
      add_env_var(params->envp[i], NULL, &envp);

  /* Add CUPS_SERVERBIN to the beginning of PATH */
  ptr1 = get_env_var("PATH", envp);
  ptr2 = get_env_var("CUPS_SERVERBIN", envp);
  if (ptr2 && ptr2[0])
  {
    if (ptr1 && ptr1[0])
    {
      snprintf(buf, sizeof(buf), "%s/%s:%s",
	       ptr2, params->is_backend ? "backend" : "filter", ptr1);
      ptr1 = buf;
    }
    else
      ptr1 = ptr2;
    add_env_var("PATH", ptr1, &envp);
  }

  if (params->is_backend < 2) /* Not needed in discovery mode of backend */
  {
    /* Print queue name from filter data */
    if (data->printer)
      add_env_var("PRINTER", data->printer, &envp);
    else
      add_env_var("PRINTER", "Unknown", &envp);

    /* PPD file path/name from filter data, required for most CUPS filters */
    if (data->ppdfile)
      add_env_var("PPD", data->ppdfile, &envp);

    /* Device URI from parameters */
    if (params->is_backend && params->device_uri)
      add_env_var("DEVICE_URI", (char *)params->device_uri, &envp);
  }

  /* Determine full path for the filter */
  if (params->filter[0] == '/' ||
      (ptr1 = get_env_var("CUPS_SERVERBIN", envp)) == NULL || !ptr1[0])
    strncpy(filter_path, params->filter, sizeof(filter_path) - 1);
  else
    snprintf(filter_path, sizeof(filter_path), "%s/%s/%s", ptr1,
	     params->is_backend ? "backend" : "filter", params->filter);

  /* Log the resulting list of environment variable settings
     (with any authentication info removed)*/
  if (log)
  {
    for (i = 0; envp[i]; i ++)
      if (!strncmp(envp[i], "AUTH_", 5))
	log(ld, CF_LOGLEVEL_DEBUG, "cfFilterExternalCUPS (%s): envp[%d]: AUTH_%c****",
	    filter_name, i, envp[i][5]);
      else if (!strncmp(envp[i], "DEVICE_URI=", 11))
	log(ld, CF_LOGLEVEL_DEBUG, "cfFilterExternalCUPS (%s): envp[%d]: DEVICE_URI=%s",
	    filter_name, i, sanitize_device_uri(envp[i] + 11,
						buf, sizeof(buf)));
      else
	log(ld, CF_LOGLEVEL_DEBUG, "cfFilterExternalCUPS (%s): envp[%d]: %s",
	    filter_name, i, envp[i]);
  }

  if (params->is_backend < 2) {
   /*
    * Filter or backend for job execution
    */

   /*
    * Join the options from the filter data and from the parameters
    * If an option is present in both filter data and parameters, the
    * value in the filter data has priority
    */

    for (i = 0, opt = params->options; i < params->num_options; i ++, opt ++)
      num_all_options = cupsAddOption(opt->name, opt->value, num_all_options,
				      &all_options);
    for (i = 0, opt = data->options; i < data->num_options; i ++, opt ++)
      num_all_options = cupsAddOption(opt->name, opt->value, num_all_options,
				      &all_options);

   /*
    * Create command line arguments for the CUPS filter
    */

    argv = (char **)calloc(7, sizeof(char *));

    /* Numeric parameters */
    snprintf(job_id_str, sizeof(job_id_str) - 1, "%d",
	     data->job_id > 0 ? data->job_id : 1);
    snprintf(copies_str, sizeof(copies_str) - 1, "%d",
	     data->copies > 0 ? data->copies : 1);

    /* Options, build string of "Name1=Value1 Name2=Value2 ..." but use
       "Name" and "noName" instead for boolean options */
    for (i = 0, opt = all_options; i < num_all_options; i ++, opt ++) {
      if (strcasecmp(opt->value, "true") == 0 ||
	  strcasecmp(opt->value, "false") == 0) {
	options_str =
	  (char *)realloc(options_str,
			  ((options_str ? strlen(options_str) : 0) +
			   strlen(opt->name) +
			   (strcasecmp(opt->value, "false") == 0 ? 2 : 0) + 2) *
			  sizeof(char));
	if (i == 0)
	  options_str[0] = '\0';
	sprintf(options_str + strlen(options_str), " %s%s",
		(strcasecmp(opt->value, "false") == 0 ? "no" : ""), opt->name);
      } else {
	options_str =
	  (char *)realloc(options_str,
			  ((options_str ? strlen(options_str) : 0) +
			   strlen(opt->name) + strlen(opt->value) + 3) *
			  sizeof(char));
	if (i == 0)
	  options_str[0] = '\0';
	sprintf(options_str + strlen(options_str), " %s=%s", opt->name, opt->value);
      }
    }

    /* Find DEVICE_URI environment variable */
    if (params->is_backend && !params->device_uri)
      for (i = 0; envp[i]; i ++)
	if (strncmp(envp[i], "DEVICE_URI=", 11) == 0)
	  break;

    /* Add items to array */
    argv[0] = strdup((params->is_backend && params->device_uri ?
		      (char *)sanitize_device_uri(params->device_uri,
						  buf, sizeof(buf)) :
		      (params->is_backend && envp[i] ?
		       (char *)sanitize_device_uri(envp[i] + 11,
						   buf, sizeof(buf)) :
		       (data->printer ? data->printer :
			(char *)params->filter))));
    argv[1] = job_id_str;
    argv[2] = data->job_user ? data->job_user : "Unknown";
    argv[3] = data->job_title ? data->job_title : "Untitled";
    argv[4] = copies_str;
    argv[5] = options_str ? options_str + 1 : "";
    argv[6] = NULL;

    /* Log the arguments */
    if (log)
      for (i = 0; argv[i]; i ++)
	log(ld, CF_LOGLEVEL_DEBUG, "cfFilterExternalCUPS (%s): argv[%d]: %s",
	    filter_name, i, argv[i]);
  } else {
   /*
    * Backend in device discovery mode
    */

    argv = (char **)calloc(2, sizeof(char *));
    argv[0] = strdup((char *)params->filter);
    argv[1] = NULL;
  }

 /*
  * Execute the filter
  */

  if (pipe(stderrpipe) < 0) {
    if (log) log(ld, CF_LOGLEVEL_ERROR,
		 "cfFilterExternalCUPS (%s): Could not create pipe for stderr: %s",
		 filter_name, strerror(errno));
    return (1);
  }

  if ((pid = fork()) == 0) {
   /*
    * Child process goes here...
    *
    * Update stdin/stdout/stderr as needed...
    */

    if (inputfd != 0) {
      if (inputfd < 0) {
        inputfd = open("/dev/null", O_RDONLY);
	if (log) log(ld, CF_LOGLEVEL_ERROR,
		     "cfFilterExternalCUPS (%s): No input file descriptor supplied for CUPS filter - %s",
		   filter_name, strerror(errno));
      }

      if (inputfd > 0) {
	fcntl_add_cloexec(inputfd);
        if (dup2(inputfd, 0) < 0) {
	  if (log) log(ld, CF_LOGLEVEL_ERROR,
		       "cfFilterExternalCUPS (%s): Failed to connect input file descriptor with CUPS filter's stdin - %s",
		       filter_name, strerror(errno));
	  goto fd_error;
	} else
	  if (log) log(ld, CF_LOGLEVEL_DEBUG,
		       "cfFilterExternalCUPS (%s): Connected input file descriptor %d to CUPS filter's stdin.",
		       filter_name, inputfd);
	close(inputfd);
      }
    } else
      if (log) log(ld, CF_LOGLEVEL_DEBUG,
		   "cfFilterExternalCUPS (%s): Input comes from stdin, letting the filter grab stdin directly",
		   filter_name);

    if (outputfd != 1) {
      if (outputfd < 0)
        outputfd = open("/dev/null", O_WRONLY);

      if (outputfd > 1) {
	fcntl_add_cloexec(outputfd);
	dup2(outputfd, 1);
	close(outputfd);
      }
    }

    if (strcasestr(params->filter, "gziptoany")) {
      /* Send stderr to the Nirwana if we are running gziptoany, as
	 gziptoany emits a false "PAGE: 1 1" */
      if ((fd = open("/dev/null", O_RDWR)) > 2) {
	fcntl_add_cloexec(fd);
	dup2(fd, 2);
	close(fd);
      } else
        close(fd);
    } else {
      /* Send stderr into pipe for logging */
      fcntl_add_cloexec(stderrpipe[1]);
      dup2(stderrpipe[1], 2);
      fcntl_add_nonblock(2);
    }
    close(stderrpipe[0]);
    close(stderrpipe[1]);

    if (params->is_backend < 2) { /* Not needed in discovery mode of backend */
      /* Back channel */
      if (backfd != 3 && backfd >= 0) {
	dup2(backfd, 3);
	close(backfd);
	fcntl_add_nonblock(3);
      } else if (backfd < 0) {
	if ((backfd = open("/dev/null", O_RDWR)) > 3) {
	  dup2(backfd, 3);
	  close(backfd);
	} else
	  close(backfd);
	fcntl_add_nonblock(3);
      }

      /* Side channel */
      if (sidefd != 4 && sidefd >= 0) {
	dup2(sidefd, 4);
	close(sidefd);
	fcntl_add_nonblock(4);
      } else if (sidefd < 0) {
	if ((sidefd = open("/dev/null", O_RDWR)) > 4) {
	  dup2(sidefd, 4);
	  close(sidefd);
	} else
	  close(sidefd);
	fcntl_add_nonblock(4);
      }
    }

   /*
    * Execute command...
    */

    execve(filter_path, argv, envp);

    if (log) log(ld, CF_LOGLEVEL_ERROR,
		 "cfFilterExternalCUPS (%s): Execution of %s %s failed - %s",
		 filter_name, params->is_backend ? "backend" : "filter",
		 filter_path, strerror(errno));

  fd_error:
    exit(errno);
  } else if (pid > 0) {
    if (log) log(ld, CF_LOGLEVEL_INFO,
		 "cfFilterExternalCUPS (%s): %s (PID %d) started.",
		 filter_name, filter_path, pid);
  } else {
    if (log) log(ld, CF_LOGLEVEL_ERROR,
		 "cfFilterExternalCUPS (%s): Unable to fork process for %s %s",
		 filter_name, params->is_backend ? "backend" : "filter",
		 filter_path);
    close(stderrpipe[0]);
    close(stderrpipe[1]);
    status = 1;
    goto out;
  }
  if (inputfd >= 0)
    close(inputfd);
  if (outputfd >= 0)
    close(outputfd);

 /*
  * Log the filter's stderr
  */

  if ((stderrpid = fork()) == 0) {
   /*
    * Child process goes here...
    */

    close(stderrpipe[1]);
    fp = cupsFileOpenFd(stderrpipe[0], "r");
    while (cupsFileGets(fp, buf, sizeof(buf)))
      if (log) {
	if (strncmp(buf, "DEBUG: ", 7) == 0) {
	  log_level = CF_LOGLEVEL_DEBUG;
	  msg = buf + 7;
	} else if (strncmp(buf, "DEBUG2: ", 8) == 0) {
	  log_level = CF_LOGLEVEL_DEBUG;
	  msg = buf + 8;
	} else if (strncmp(buf, "INFO: ", 6) == 0) {
	  log_level = CF_LOGLEVEL_INFO;
	  msg = buf + 6;
	} else if (strncmp(buf, "WARNING: ", 9) == 0) {
	  log_level = CF_LOGLEVEL_WARN;
	  msg = buf + 9;
	} else if (strncmp(buf, "ERROR: ", 7) == 0) {
	  log_level = CF_LOGLEVEL_ERROR;
	  msg = buf + 7;
	} else if (strncmp(buf, "PAGE: ", 6) == 0 ||
		   strncmp(buf, "ATTR: ", 6) == 0 ||
		   strncmp(buf, "STATE: ", 7) == 0 ||
		   strncmp(buf, "PPD: ", 5) == 0) {
	  log_level = CF_LOGLEVEL_CONTROL;
	  msg = buf;
	} else {
	  log_level = CF_LOGLEVEL_DEBUG;
	  msg = buf;
	}
	if (log_level == CF_LOGLEVEL_CONTROL)
	  log(ld, log_level, msg);
	else
	  log(ld, log_level, "cfFilterExternalCUPS (%s): %s", filter_name, msg);
      }
    cupsFileClose(fp);
    /* No need to close the fd stderrpipe[0], as cupsFileClose(fp) does this
       already */
    /* Ignore errors of the logging process */
    exit(0);
  } else if (stderrpid > 0) {
    if (log) log(ld, CF_LOGLEVEL_INFO,
		 "cfFilterExternalCUPS (%s): Logging (PID %d) started.",
		 filter_name, stderrpid);
  } else {
    if (log) log(ld, CF_LOGLEVEL_ERROR,
		 "cfFilterExternalCUPS (%s): Unable to fork process for logging",
		 filter_name);
    close(stderrpipe[0]);
    close(stderrpipe[1]);
    status = 1;
    goto out;
  }

  close(stderrpipe[0]);
  close(stderrpipe[1]);

 /*
  * Wait for filter and logging processes to finish
  */

  status = 0;

  while (pid > 0 || stderrpid > 0) {
    if ((wpid = wait(&wstatus)) < 0) {
      if (errno == EINTR && iscanceled && iscanceled(icd)) {
	if (log) log(ld, CF_LOGLEVEL_DEBUG,
		     "cfFilterExternalCUPS (%s): Job canceled, killing %s ...",
		     filter_name, params->is_backend ? "backend" : "filter");
	kill(pid, SIGTERM);
	pid = -1;
	kill(stderrpid, SIGTERM);
	stderrpid = -1;
	break;
      } else
	continue;
    }

    /* How did the filter terminate */
    if (wstatus) {
      if (WIFEXITED(wstatus)) {
	/* Via exit() anywhere or return() in the main() function */
	if (log) log(ld, CF_LOGLEVEL_ERROR,
		     "cfFilterExternalCUPS (%s): %s (PID %d) stopped with status %d",
		     filter_name,
		     (wpid == pid ?
		      (params->is_backend ? "Backend" : "Filter") :
		      "Logging"),
		     wpid, WEXITSTATUS(wstatus));
      } else {
	/* Via signal */
	if (log) log(ld, CF_LOGLEVEL_ERROR,
		     "cfFilterExternalCUPS (%s): %s (PID %d) crashed on signal %d",
		     filter_name,
		     (wpid == pid ?
		      (params->is_backend ? "Backend" : "Filter") :
		      "Logging"),
		     wpid, WTERMSIG(wstatus));
      }
      status = 1;
    } else {
      if (log) log(ld, CF_LOGLEVEL_INFO,
		   "cfFilterExternalCUPS (%s): %s (PID %d) exited with no errors.",
		   filter_name,
		   (wpid == pid ?
		    (params->is_backend ? "Backend" : "Filter") : "Logging"),
		   wpid);
    }
    if (wpid == pid)
      pid = -1;
    else  if (wpid == stderrpid)
      stderrpid = -1;
  }

 /*
  * Clean up
  */

 out:
  cupsFreeOptions(num_all_options, all_options);
  if (options_str)
    free(options_str);
  free(argv[0]);
  free(argv);
  for (i = 0; envp[i]; i ++)
    free(envp[i]);
  free(envp);

  return (status);
}

