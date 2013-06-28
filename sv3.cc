
#include <cstdio>
#include <algorithm>
#include <unistd.h>
#include <signal.h>

#include <hash/ethernet.hh>
#include <switch.hh>
#include <listener.hh>

static Switch::Switch *signal_switch;
static bool            signal_caught;

static void sigint_handler(int)
{
  // Try graceful shutdown first. On second signal exit directly.
  if (not signal_caught) {
    signal_switch->shutdown();
    signal_caught = true;
  } else {
    _Exit(EXIT_FAILURE);
  }
}

#if defined(__GNUC__) && !defined(__clang__)
# define COMPILER "gcc "
# define COMPILER_VERSION __VERSION__
#elif defined(__clang__)
# define COMPILER "clang "
# define COMPILER_VERSION __clang_version__
#else
# define COMPILER "an unknown compiler"
# define COMPILER_VERSION ""
#endif

int main(int argc, char **argv)
{
  printf("           ____\n"
	 "  ____  __|_  / Userspace\n"
	 " (_-< |/ //_ <  Software\n"
	 "/___/___/____/  Switch\n\n"
	 "Built from git revision ["
#include "version.inc"
	 "] with " COMPILER COMPILER_VERSION "."
	 "\nBlame Julian Stecklina <jsteckli@os.inf.tu-dresden.de>\n\n");

  bool force = false;
  int  opt;
  while ((opt = getopt(argc, argv, "f")) != -1) {
    switch (opt) {
    case 'f':
      force = true;
      break;
    default: /* '?' */
      fprintf(stderr, "Usage: %s [-f]\n", argv[0]);
      return EXIT_FAILURE;
    }
  }

  try {
    Switch::Switch   sv3;
    Switch::Listener listener(sv3, force);

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler   = sigint_handler;
    sa.sa_flags     = 0;
    signal_switch   = &sv3;

    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    sv3.loop();

    return EXIT_SUCCESS;
  } catch (std::system_error &e) {
    fprintf(stderr, "\nFatal system error: '%s'\n", e.what());
  }

  return EXIT_FAILURE;
}

// EOF
