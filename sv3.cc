// Copyright (C) 2013, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
// Economic rights: Technische Universitaet Dresden (Germany)

// This file is part of sv3.

// sv3 is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

// sv3 is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License version 2 for more details.


#include <cstdio>
#include <algorithm>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <getopt.h>

#include <string>
#include <sstream>


#include <hash/ethernet.hh>
#include <switch.hh>
#include <listener.hh>
#include <config.hh>
#include <tracing.hh>

/// Tracing

#ifdef TRACING

void open_trace_file(std::string file)
{
  if (not file.length()) {
    std::stringstream ss;
    ss << "trace-" << getpid();
    file = ss.str();
  }

  printf("Trace output is in '%s'.\n\n", file.c_str());
  int fd = open(file.c_str(), O_RDWR | O_TRUNC | O_CREAT, 0644);
  if (fd < 0 or ftruncate(fd, Switch::TRACE_SIZE) != 0) {
    perror("open");
    exit(EXIT_FAILURE);
  }
  
  void *r = mmap(nullptr, Switch::TRACE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                      fd, 0);
  if (r == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
  }

  Switch::trace_buffer = reinterpret_cast<char *>(r);

  close(fd);
}

#endif

/// Signal handling

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

/// Main function

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
	 "Built from git revision "
#include "version.inc"
	 " with " COMPILER COMPILER_VERSION ".\n"
#ifdef SV3_BENCHMARK_OK
	 "Built with optimal compiler flags. Let the benchmarking commence!\n"
#else
	 "Suboptimal compilation flags. Do not use for benchmarking!\n"
#endif
	 "Blame Julian Stecklina <jsteckli@os.inf.tu-dresden.de>.\n\n");

  int force               = false;

  static struct option long_options [] = {
    { "force",            no_argument, &force,                       1  },
    { "poll-us",          required_argument, 0,                     'p' },
    { "batch-size",       required_argument, 0,                     'b' },
#ifdef TRACING
    { "trace-file",       required_argument, 0,                     't' },
#endif
    { 0, 0, 0, 0 },
  };

  int         poll_us    = 50;
  int         batch_size = 16;
#ifdef TRACING
  std::string trace_file;
#endif

  int opt;
  int opt_idx;

  while ((opt = getopt_long(argc, argv, "f", long_options, &opt_idx)) != -1) {
    switch (opt) {
    case 0:
      continue;
    case 'f':
      force = true;
      break;
    case 'p':
      poll_us = atoi(optarg);
      break;
    case 'b':
      batch_size = atoi(optarg);
      break;
#ifdef TRACING
    case 't':
      trace_file = optarg;
      break;
#endif
    case '?':
    default: /* '?' */
      fprintf(stderr,
              "Usage: %s [-f|--force] [--poll-us us] [--batch-size n]\n"
              "          [--trace-file file]\n",
              argv[0]);
      return EXIT_FAILURE;
    }
  }

#ifdef TRACING
  open_trace_file(trace_file);
#endif

  try {
    Switch::Switch   sv3(poll_us, batch_size);
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
