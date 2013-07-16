/**
 * collectd-td - collectd traffic generator
 * Copyright (C) 2013       Florian octo Forster
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Authors:
 *   Florian Forster <ff at octo.it>
 **/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#if !__GNUC__
# define __attribute__(x) /**/
#endif

#define DEF_NODE "localhost"
#define DEF_SERVICE "8125"

#define DEF_NUM_COUNTERS  1000
#define DEF_NUM_TIMERS    1000
#define DEF_NUM_GAUGES     100
#define DEF_NUM_SETS       100
#define DEF_SET_SIZE       128

static int conf_num_counters = DEF_NUM_COUNTERS;
static int conf_num_timers   = DEF_NUM_TIMERS;
static int conf_num_gauges   = DEF_NUM_GAUGES;
static int conf_num_sets     = DEF_NUM_SETS;
static int conf_set_size     = DEF_SET_SIZE;
static const char *conf_node = DEF_NODE;
static const char *conf_service = DEF_SERVICE;

static int conf_threads_num = 1;

static struct sigaction sigint_action;
static struct sigaction sigterm_action;

static unsigned long long events_sent = 0;
pthread_mutex_t events_sent_lock = PTHREAD_MUTEX_INITIALIZER;
static _Bool loop = 1;

__attribute__((noreturn))
static void exit_usage (int exit_status) /* {{{ */
{
  fprintf ((exit_status == EXIT_FAILURE) ? stderr : stdout,
      PACKAGE_NAME" -- statsd traffic generator\n"
      "\n"
      "  Usage: statsd-tg [OPTION]\n"
      "\n"
      "  Valid options:\n"
      "    -c <number>    Number of counters to emulate. (Default: %i)\n"
      "    -t <number>    Number of timers to emulate. (Default: %i)\n"
      "    -g <number>    Number of gauges to emulate. (Default: %i)\n"
      "    -s <number>    Number of sets to emulate. (Default: %i)\n"
      "    -S <size>      Number of elements in each set. (Default: %i)\n"
      "    -d <dest>      Destination address of the network packets.\n"
      "                   (Default: "DEF_NODE")\n"
      "    -D <port>      Destination port of the network packets.\n"
      "                   (Default: "DEF_SERVICE")\n"
      "    -T <threads>   Number of threads to use to generate load.\n"
      "    -h             Print usage information (this output).\n"
      "\n"
      "Copyright (C) 2013  Florian Forster\n"
      "Licensed under the GNU General Public License, version 2 (GPLv2)\n",
      DEF_NUM_COUNTERS, DEF_NUM_TIMERS, DEF_NUM_GAUGES,
      DEF_NUM_SETS, DEF_SET_SIZE);
  exit (exit_status);
} /* }}} void exit_usage */

static void signal_handler (int signal __attribute__((unused))) /* {{{ */
{
  loop = 0;
} /* }}} void signal_handler */

static int sock_open (void) /* {{{ */
{
  struct addrinfo ai_hints;
  struct addrinfo *ai_list = NULL;
  struct addrinfo *ai_ptr;
  int sock;

  int status;

  memset (&ai_hints, 0, sizeof (ai_hints));
#ifdef AI_ADDRCONFIG
  ai_hints.ai_flags = AI_ADDRCONFIG;
#endif
  ai_hints.ai_family = AF_UNSPEC;
  ai_hints.ai_socktype = SOCK_DGRAM;

  status = getaddrinfo (conf_node, conf_service, &ai_hints, &ai_list);
  if (status != 0)
  {
    fprintf (stderr, "getaddrinfo failed: %s\n", gai_strerror (status));
    exit (EXIT_FAILURE);
  }

  for (ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next)
  {
    int fd;

    fd = socket (ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol);
    if (fd < 0)
    {
      continue;
    }

    status = connect (fd, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
    if (status != 0)
    {
      close (fd);
      continue;
    }

    sock = fd;
    break;
  }

  freeaddrinfo (ai_list);

  if (sock < 0)
  {
    fprintf (stderr, "Opening network socket failed.\n");
    exit (EXIT_FAILURE);
  }

  return (sock);
} /* }}} int sock_open */

static int send_random_event (int sock, unsigned short seed[static 3]) /* {{{ */
{
  long conf_num_total = conf_num_counters + conf_num_timers
      + conf_num_gauges + conf_num_sets;
  /* Not completely fair, but good enough for our use-case. */
  long rnd = nrand48 (seed) % conf_num_total;

  long value = nrand48 (seed);
  char *type;

  char buffer[1024];
  int buffer_size;
  ssize_t status;

  if (rnd < conf_num_counters)
  {
    /* counter */
    type = "c";
    value = (value % 8) + 1;
  }
  else if (rnd < (conf_num_counters + conf_num_timers))
  {
    /* timer */
    type = "ms";
    value = (value % 1024) + 1;
  }
  else if (rnd < (conf_num_counters + conf_num_timers + conf_num_gauges))
  {
    /* gauge */
    type = "g";
    value = (value % 128) - 64;
  }
  else
  {
    /* set */
    type = "s";
    value %= conf_set_size;
  }

  buffer_size = snprintf (buffer, sizeof (buffer), "%06li:%li|%s",
                          rnd, value, type);
  assert (buffer_size > 0);
  if (((size_t) buffer_size) >= sizeof (buffer))
    return (-1);
  assert (buffer[buffer_size] == 0);

  status = send (sock, buffer, (size_t) buffer_size, /* flags = */ 0);
  if (status < 0)
  {
    fprintf (stderr, "send failed: %s\n", strerror (errno));
    return (-1);
  }

  return (0);
} /* }}} int send_random_event */

static int get_integer_opt (const char *str, int *ret_value) /* {{{ */
{
  char *endptr;
  int tmp;

  errno = 0;
  endptr = NULL;
  tmp = (int) strtol (str, &endptr, /* base = */ 0);
  if (errno != 0)
  {
    fprintf (stderr, "Unable to parse option as a number: \"%s\": %s\n",
        str, strerror (errno));
    exit (EXIT_FAILURE);
  }
  else if (endptr == str)
  {
    fprintf (stderr, "Unable to parse option as a number: \"%s\"\n", str);
    exit (EXIT_FAILURE);
  }
  else if (*endptr != 0)
  {
    fprintf (stderr, "Garbage after end of value: \"%s\"\n", str);
    exit (EXIT_FAILURE);
  }

  *ret_value = tmp;
  return (0);
} /* }}} int get_integer_opt */

static int read_options (int argc, char **argv) /* {{{ */
{
  int opt;

#ifdef _SC_NPROCESSORS_ONLN
  conf_threads_num = (int) sysconf (_SC_NPROCESSORS_ONLN);
#endif

  while ((opt = getopt (argc, argv, "c:t:g:s:S:d:D:T:h")) != -1)
  {
    switch (opt)
    {
      case 'c':
        get_integer_opt (optarg, &conf_num_counters);
        break;

      case 't':
        get_integer_opt (optarg, &conf_num_timers);
        break;

      case 'g':
        get_integer_opt (optarg, &conf_num_gauges);
        break;

      case 's':
        get_integer_opt (optarg, &conf_num_sets);
        break;

      case 'S':
        get_integer_opt (optarg, &conf_set_size);
        break;

      case 'd':
        conf_node = optarg;
        break;

      case 'D':
        conf_service = optarg;
        break;

      case 'T':
        get_integer_opt (optarg, &conf_threads_num);
        break;

      case 'h':
        exit_usage (EXIT_SUCCESS);

      default:
        exit_usage (EXIT_FAILURE);
    } /* switch (opt) */
  } /* while (getopt) */

  return (0);
} /* }}} int read_options */

static void *send_thread (void *args __attribute__((unused))) /* {{{ */
{
  int sock;
  unsigned short seed[3];
  struct timespec ts;

  unsigned long long local_events_sent = 0;

  clock_gettime (CLOCK_REALTIME, &ts);
  seed[2] = (unsigned short) (ts.tv_nsec);
  seed[1] = (unsigned short) (ts.tv_nsec >> 16);
  seed[0] = (unsigned short) (ts.tv_sec);

  sock = sock_open ();

  while (loop)
  {
    send_random_event (sock, seed);
    local_events_sent++;
  }

  close (sock);

  pthread_mutex_lock (&events_sent_lock);
  events_sent += local_events_sent;
  pthread_mutex_unlock (&events_sent_lock);

  return (NULL);
} /* }}} void *send_thread */

static void run_threads (void) /* {{{ */
{
  pthread_t threads[conf_threads_num];
  int i;

  for (i = 0; i < conf_threads_num; i++)
  {
    int status;

    status = pthread_create (&threads[i], /* attr = */ NULL,
        send_thread, /* args = */ NULL);
    if (status != 0)
    {
      fprintf (stderr, "pthread_create failed.");
      abort ();
    }
  }

  for (i = 0; i < conf_threads_num; i++)
    pthread_join (threads[i], /* retval = */ NULL);
} /* }}} void run_threads */

static double timespec_diff (struct timespec const *ts0, /* {{{ */
    struct timespec const *ts1)
{
  time_t diff_sec;
  long diff_nsec;

  diff_sec = ts1->tv_sec - ts0->tv_sec;
  diff_nsec += ts1->tv_nsec - ts0->tv_nsec;

  return ((double) diff_sec) + (((double) diff_nsec) / 1.0e9);
} /* }}} double timespec_diff */

int main (int argc, char **argv) /* {{{ */
{
  struct timespec ts_begin;
  struct timespec ts_end;
  double runtime;

  read_options (argc, argv);

  sigint_action.sa_handler = signal_handler;
  sigaction (SIGINT, &sigint_action, /* old = */ NULL);

  sigterm_action.sa_handler = signal_handler;
  sigaction (SIGTERM, &sigterm_action, /* old = */ NULL);

  clock_gettime (CLOCK_MONOTONIC, &ts_begin);
  run_threads ();
  clock_gettime (CLOCK_MONOTONIC, &ts_end);

  runtime = timespec_diff (&ts_begin, &ts_end);
  printf ("Sent %llu events in %.0fs (%.0f events/s).\n",
      events_sent, runtime, ((double) events_sent) / runtime);

  exit (EXIT_SUCCESS);
  return (0);
} /* }}} int main */

/* vim: set sw=2 sts=2 et fdm=marker : */
