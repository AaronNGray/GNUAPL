/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2015  Dr. Jürgen Sauermann

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "Common.hh"
#include "LineInput.hh"
#include "LibPaths.hh"
#include "NativeFunction.hh"
#include "Output.hh"
#include "ProcessorID.hh"
#include "Svar_DB.hh"
#include "Value.hh"
#include "UserPreferences.hh"
#include "ValueHistory.hh"

uint64_t total_memory = 0x40000000;   // a little more than 1 Gig
rlim_t initial_rlimit = RLIM_INFINITY;

bool got_WINCH = false;

//-----------------------------------------------------------------------------
static bool attention_raised = false;
static uint64_t attention_count = 0;

extern void set_attention_raised(const char * loc)
{
   attention_raised = true;
}
extern void clear_attention_raised(const char * loc)
{
   attention_raised = false;
}
bool attention_is_raised()
{
   return attention_raised;
}
//-----------------------------------------------------------------------------
APL_time_us interrupt_when = 0;
uint64_t interrupt_count = 0;
static bool interrupt_raised = false;
void set_interrupt_raised(const char * loc)
{
   interrupt_raised = true;
}
void clear_interrupt_raised(const char * loc)
{
   interrupt_raised = false;
}
bool interrupt_is_raised()
{
   return interrupt_raised;
}
//-----------------------------------------------------------------------------
uint64_t
get_free_memory()
{
uint64_t proc_mem = 1000000000;   // assume 1 Gig on error

   if (FILE * pm = fopen("/proc/meminfo", "r"))
      {
        for (;;)
            {
              char buffer[2000];
              if (fgets(buffer, sizeof(buffer) - 1, pm) == 0)   break;
              buffer[sizeof(buffer) - 1] = 0;
              if (!strncmp(buffer, "MemFree:", 8))
                 {
                   proc_mem = atoi(buffer + 8);
                   proc_mem *= 1024;
                   break;
                 }
            }

        fclose(pm);
      }

   return proc_mem;
}
//-----------------------------------------------------------------------------
void
init_1(const char * argv0, bool log_startup)
{
rlimit rl;

#ifndef RLIMIT_AS // BSD does not define RLIMIT_AS
# define RLIMIT_AS RLIMIT_DATA
#endif

enum { MAX_RLIMIT_AS = 4000000000UL };

   getrlimit(RLIMIT_AS, &rl);
   initial_rlimit = rl.rlim_cur;

   if (log_startup)
      {
        if (initial_rlimit == ~rlim_t(0))
          CERR << "initial RLIMIT_AS (aka. virtual memory) is: 'unlimited'"
               << endl;
        else
          CERR << "initial RLIMIT_AS (aka. virtual memory) is: "
               <<  initial_rlimit << endl;
      }

   // if the user has set a memory rlimit (and hopefully knowing what she is
   // doing) then leave it as is; otherwise set the limit to 80 % of the
   // avaiable memory
   //
   total_memory = (get_free_memory() / 10) * 8;
   if (log_startup)
      CERR << "set total memory to " << total_memory
          << " (= 80% of MemFree: in /proc/meminfo)" << endl;

   if (rl.rlim_cur == RLIM_INFINITY)
      {
        rl.rlim_cur = total_memory;
        if (log_startup)
           CERR << "decreasing RLIMIT_AS to: " << rl.rlim_cur << endl;
        setrlimit(rl.rlim_cur, &rl);
      }
   else if (rl.rlim_cur > total_memory)
      {
        rl.rlim_cur = total_memory;
           CERR << "decreasing RLIMIT_AS to: " << rl.rlim_cur << endl;
        setrlimit(total_memory, &rl);
      }

   total_memory = rl.rlim_cur;

   if (log_startup)
      CERR
           << "sizeof(int) is           " << sizeof(int)               << endl
           << "sizeof(long) is          " << sizeof(long)              << endl
           << "sizeof(void *) is        " << sizeof(void *)            << endl
           << "sizeof(Cell) is          " << sizeof(Cell)              << endl
           << "sizeof(Value) is         " << sizeof(Value)
           << " (including " << SHORT_VALUE_LENGTH_WANTED << " Cells)" << endl
           << "sizeof(Svar_record) is   " << sizeof(Svar_record)       << endl
           << "sizeof(Svar_partner) is  " << sizeof(Svar_partner)      << endl
           << "process memory limit is  " << total_memory << " bytes"  << endl;

   // CYGWIN does not have RLIMIT_NPROC
   //
#ifdef RLIMIT_NPROC

   // unlimit the number of threads and processes...
   //
   getrlimit(RLIMIT_NPROC, &rl);
   if (log_startup)
      CERR << "increasing rlimit RLIMIT_NPROC from " <<  rl.rlim_cur
           << " to infinity" << endl;
   rl.rlim_cur = RLIM_INFINITY;
   setrlimit(RLIMIT_NPROC, &rl);

   // limit the virtual memory size to avoid new() problem with large values
   //
#endif

   Avec::init();
   LibPaths::init(argv0, log_startup);
   Value::init();
   VH_entry::init();
}
//-----------------------------------------------------------------------------
/// initialize subsystems that depend on argv[]
void
init_2(bool log_startup)
{
const int retry_max = uprefs.emacs_mode ? 15 : 5;

   Output::init(log_startup);
   Svar_DB::init(LibPaths::get_APL_bin_path(), LibPaths::get_APL_bin_name(),
                 retry_max, log_startup, uprefs.system_do_svars);

   LineInput::init(true);

   Parallel::init(log_startup || LOG_Parallel);
}
//-----------------------------------------------------------------------------
/// the opposite of init()
void
cleanup(bool soft)
{
   if (soft)   // proper clean-up
      {
        ProcessorID::disconnect();

        NativeFunction::cleanup();

        // write line history
        //
        LineInput::close(false);

        Output::reset_colors();
      }
   else        // minimal clean-up
      {
        LineInput::close(true);
        Output::reset_colors();
      }
}
//-----------------------------------------------------------------------------
void
control_C(int)
{
APL_time_us when = now();

   CIN << "^C";

   attention_raised = true;
   ++attention_count;
   if ((when - interrupt_when) < 1000000)   // second ^C within 1 second
      {
        interrupt_raised = true;
        ++interrupt_count;
      }

   interrupt_when = when;
}
//-----------------------------------------------------------------------------

// Probes...

int64_t Probe::dummy = 0;
Probe Probe::probes[Probe::PROBE_COUNT];

Probe & Probe::P0 = probes[0];
Probe & Probe::P_1 = probes[Probe::PROBE_COUNT - 1];
Probe & Probe::P_2 = probes[Probe::PROBE_COUNT - 2];
Probe & Probe::P_3 = probes[Probe::PROBE_COUNT - 3];
Probe & Probe::P_4 = probes[Probe::PROBE_COUNT - 4];
Probe & Probe::P_5 = probes[Probe::PROBE_COUNT - 5];

//-----------------------------------------------------------------------------
void *
common_new(size_t size)
{
void * ret = malloc(size);
const uint64_t iret = reinterpret_cast<uint64_t>(ret);
   CERR << "NEW " << HEX(iret) << "-" << HEX(iret + size)
        << "  (" << HEX(size) << ")" << endl;
   return ret;
}
//-----------------------------------------------------------------------------
void
common_delete(void * p)
{
   CERR << "DEL " << HEX(reinterpret_cast<uint64_t>(p)) << endl;
   free(p);
}
//-----------------------------------------------------------------------------
APL_time_us
now()
{
timeval tv_now;
   gettimeofday(&tv_now, 0);

APL_time_us ret = tv_now.tv_sec;
   ret *= 1000000;
   ret += tv_now.tv_usec;
   return ret;
}
//-----------------------------------------------------------------------------
YMDhmsu::YMDhmsu(APL_time_us at)
   : micro(at % 1000000)
{
const time_t secs = at/1000000;
tm t;
   gmtime_r(&secs, &t);

   year   = t.tm_year + 1900;
   month  = t.tm_mon  + 1;
   day    = t.tm_mday;
   hour   = t.tm_hour;
   minute = t.tm_min;
   second = t.tm_sec;
}
//-----------------------------------------------------------------------------
APL_time_us
YMDhmsu::get() const
{
tm t;
   t.tm_year  = year - 1900;
   t.tm_mon   = month - 1;
   t.tm_mday  = day;
   t.tm_hour  = hour;
   t.tm_min   = minute;
   t.tm_sec   = second;
   t.tm_isdst = 0;

APL_time_us ret =  mktime(&t);
   ret *= 1000000;
   ret += micro;
   return ret;
}
//-----------------------------------------------------------------------------
ostream &
operator << (ostream & out, const Function_PC2 & ft)
{
   return out << ft.low << ":" << ft.high;
}
//-----------------------------------------------------------------------------
ostream &
print_flags(ostream & out, ValueFlags flags)
{
   return out << ((flags & VF_marked)   ?  "M" : "-")
              << ((flags & VF_complete) ?  "C" : "-");
}
//-----------------------------------------------------------------------------
int
nibble(Unicode uni)
{
   switch(uni)
      {
        case UNI_ASCII_0 ... UNI_ASCII_9:   return      (uni - UNI_ASCII_0);
        case UNI_ASCII_A ... UNI_ASCII_F:   return 10 + (uni - UNI_ASCII_A);
        case UNI_ASCII_a ... UNI_ASCII_f:   return 10 + (uni - UNI_ASCII_a);
        default: break;
      }

   return -1;   // uni is not a hex digit
}
//-----------------------------------------------------------------------------
int
sixbit(Unicode uni)
{
   switch(uni)
      {
        case UNI_ASCII_A ... UNI_ASCII_Z:   return      (uni - UNI_ASCII_A);
        case UNI_ASCII_a ... UNI_ASCII_z:   return 26 + (uni - UNI_ASCII_a);
        case UNI_ASCII_0 ... UNI_ASCII_9:   return 52 + (uni - UNI_ASCII_0);

        case UNI_ASCII_PLUS:                             // standard    62
        case UNI_ASCII_MINUS:               return 62;   // alternative 62

        case UNI_ASCII_SLASH:                            // standard    63
        case UNI_ASCII_UNDERSCORE:          return 63;   // alternative 63

        case UNI_ASCII_EQUAL:               return 64;   // fill character

        default: break;
      }

   return -1;   // uni is not a hex digit
}
//-----------------------------------------------------------------------------
