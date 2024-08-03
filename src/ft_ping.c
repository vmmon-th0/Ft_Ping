#include "ft_ping.h"

static char short_options[] = "vh";

static struct option long_options[] = { { "verbose", no_argument, NULL, 'v' },
                                        { "help", no_argument, NULL, 'h' },
                                        { NULL, no_argument, NULL, 0 } };

void
show_usage (void)
{
  printf ("Usage: ping [OPTION]... [ADDRESS]...\n\
		Options :\n\
		-h, --help         display this help and exit\n\
		-v, --verbose      verbose output\n\
	");
}

int
main (int argc, char *argv[])
{
  int opt;
  int long_index = 0;

  while (
      (opt = getopt_long (argc, argv, short_options, long_options, &long_index))
      != EOF)
  {
    switch (opt)
    {
      case 'v':
        // printf ("Verbose with argument : %s\n", optarg);
        break;
      case 'h':
        show_usage ();
        exit(EXIT_SUCCESS);
      default:
        fprintf (stderr, "%c: not implemented\n", opt);
        exit (EXIT_FAILURE);
    }
  }

  if (optind != argc - 1)
  {
    show_usage ();
    exit (EXIT_FAILURE);
  }

  return 0;
}
