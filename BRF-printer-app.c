# include <pappl/pappl.h>
# include <ppd/ppd.h>
# include <cupsfilters/log.h>
# include <cupsfilters/filter.h>
# include <cups/cups.h>
# include <string.h>
# include <limits.h>





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
