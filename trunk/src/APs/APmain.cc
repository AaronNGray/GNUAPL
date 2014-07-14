/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2013  Dr. Jürgen Sauermann

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

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>

#include <cassert>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <vector>

#include "APmain.hh"
#include "Common.hh"
#include "PrintOperator.hh"
#include "Svar_DB.hh"
#include "Svar_signals.hh"

#ifdef DYNAMIC_LOG_WANTED
extern bool LOG_startup;
extern bool LOG_shared_variables;
extern bool LOG_Svar_DB_signals;
bool LOG_startup = false;
bool LOG_shared_variables = false;
bool LOG_Svar_DB_signals = false;
#else
#endif

extern const char * prog_name();

//-----------------------------------------------------------------------------

bool verbose = false;
int event_port = 0;

const char * prog = 0;
char pref[FILENAME_MAX + 20];

pid_t parent_pid = 0;

/// coupled variables
vector<Coupled_var> coupled_vars;

/// the place where the AP specific part has detected an error
string error_loc = "?";

/// the name of this AP (aplXXX where XXX is the processor number)
char AP_NAME[40] = "apl" STR(AP_NUM);

AP_num3 ProcessorID::id(NO_AP, AP_NULL, AP_NULL);

ostream & get_CERR()
{
   return cerr;
}
//-----------------------------------------------------------------------------
uint64_t
now_ms()
{
timeval current;
   gettimeofday(&current, 0);

uint64_t ret = current.tv_sec;
   ret *= 1000;
   ret += current.tv_usec/1000;
   return ret;
}
//-----------------------------------------------------------------------------
/// return true if key already exists; otherwise add variable and return false
bool
add_var(SV_key key)
{
   for (int c = 0; c < coupled_vars.size(); ++c)
       {
         Coupled_var & cv = coupled_vars[c];
         if (key == cv.key)   return true;   // key already exists
       }

   // key is new; add it to coupled_vars
   //
Coupled_var cv = { key, 0, 0 };
   initialize(cv);
   coupled_vars.push_back(cv);

   return false;
}
//-----------------------------------------------------------------------------
void
print_vars(ostream & out)
{
   for (int c = 0; c < coupled_vars.size(); ++c)
       {
         Coupled_var & cv = coupled_vars[c];
         get_CERR() << "   key: 0x" << hex << cv.key << dec << " ";
         const uint32_t * varname = Svar_DB::get_varname(cv.key);
         if (varname)
            {
              while (*varname)   get_CERR() << (Unicode)(*varname++);
            }
         else
            {
              get_CERR() << "(unknown var)";
            }
         get_CERR() << endl;
       }
}
//-----------------------------------------------------------------------------
void
do_Assert(const char * cond, const char * fun, const char * file, int line)
{
   get_CERR() << "Assertion '" << cond << "' failed at "
        << file << ":" << line << endl;

   assert(0);
}
//-----------------------------------------------------------------------------
void
control_C(int)
{
AP_num3 this_proc = ProcessorID::get_id();
   if (verbose)   get_CERR() << pref << " unregistering processor "
                       << this_proc << endl;

const TCP_socket tcp = Svar_DB::get_DB_tcp();
string progname(prog_name());
   { UNREGISTER_PROCESSOR_c request(tcp, this_proc.proc,
                                         this_proc.parent,
                                         this_proc.grand,
                                         progname);
   }

   if (verbose)   get_CERR() << pref << " done (got ^C)" << endl;

   exit(0);
}

static struct sigaction old_ctl_C_action;
static struct sigaction new_ctl_C_action;

//-----------------------------------------------------------------------------
int usage()
{
   get_CERR()
<< "Usage:"                            << endl
<< prog << " [options]"                << endl
<<                                        endl
<< "options:"                          << endl
<< "    --auto        automatically started (exit after last retract)"  << endl
<< "    --id num      run as processor num"  << endl
<< "    --par num     run as child of num"  << endl
<< "    --ppid num    terminate if process num has died" << endl
<< "    --gra num     run as grandchild of num"  << endl
<< "    -h, --help    print this help" << endl
<< "    -v            verbose"         << endl
<<                                        endl;

   return 1;
}
//-----------------------------------------------------------------------------
int
main(int argc, char * argv[])
{
bool need_help = false;
bool auto_started = false;

   prog = argv[0];
char bin_path[FILENAME_MAX];
   strncpy(bin_path, prog, sizeof(bin_path));

char * slash = strrchr(bin_path, '/');
   if (slash)
      {
        prog = slash + 1;
        *slash = 0;
      }

   Svar_DB::init(bin_path, prog, false, true);

   if (strrchr(prog, '/'))   prog = strrchr(prog, '/') + 1;
   snprintf(pref, sizeof(pref) - 1, "%s(%u) ", prog, getpid());

   // set default processor ID
   ProcessorID::set_own_ID(AP_num(AP_NUM));

   for (int a = 1; a < argc; )
       {
         const char * opt = argv[a++];
         const char * val = (a < argc) ? argv[a] : 0;

         if      (!strcmp(opt, "-h"))             need_help = true;
         else if (!strcmp(opt, "--help"))         need_help = true;
         else if (!strcmp(opt, "--auto"))         auto_started = true;
         else if (!strcmp(opt, "-v"))             verbose = true;
         else if (!strcmp(opt, "--id") && val)
                          { ProcessorID::set_own_ID(AP_num(atoi(val))); ++a; }
         else if (!strcmp(opt, "--par") && val)
                         { ProcessorID::set_parent_ID(AP_num(atoi(val))); ++a; }
         else if (!strcmp(opt, "--ppid") && val)
                        { parent_pid = atoi(val); ++a; }
         else if (!strcmp(opt, "--gra") && val)
                         { ProcessorID::set_grand_ID(AP_num(atoi(val))); ++a; }
         else
            {
              get_CERR() << pref << ": Bad command line option "
                   << argv[a] << endl;
              need_help = true;
            }
       }

   if (need_help)   return usage();

   snprintf(AP_NAME, sizeof(AP_NAME), "apl%u", ProcessorID::get_id().proc);

   // serious attempt to run: run in the background
   //
   if (fork())   return 0;            // parent returns (daemonize)

   // child code...
   // register as e.g. AP210-port. We do this BEFORE closing stdout so that
   // caller of our parent waits until we have closed stdout
   //
UdpServerSocket sock(LOC, 0);
   if (verbose)  sock.set_debug(get_CERR());

   memset(&new_ctl_C_action, 0, sizeof(struct sigaction));
   new_ctl_C_action.sa_handler = &control_C;
   sigaction(SIGTERM, &new_ctl_C_action, &old_ctl_C_action);

Svar_partner this_proc;
   this_proc.clear();
   this_proc.id = ProcessorID::get_id();
   this_proc.pid = getpid();
   this_proc.port = sock.get_local_port();

const TCP_socket tcp = Svar_DB::get_DB_tcp();
string progname(prog_name());

      { REGISTER_PROCESSOR_c request(tcp, this_proc.id.proc,
                                          this_proc.id.parent,
                                          this_proc.id.grand,
                                          this_proc.pid,
                                          this_proc.port,
                                          progname);
      }

   fclose(stdout);   // cause getc() of caller to return EOF !

   for (bool goon = true; goon;)
       {
         uint8_t buff[MAX_SIGNAL_CLASS_SIZE];
         const Signal_base * signal = Signal_base::recv_UDP(sock, buff, 10000);
         if (signal == 0)   // no signal for 10 seconds
            {
              // if our parent has died, then we are done
              //
              if (parent_pid && !Svar_partner::pid_alive(parent_pid))
                 {
                   goon = false;
                   if (verbose)   get_CERR() << AP_NAME
                                       << " done (parent died)" << endl;
                 }
              continue;
            }

         switch(signal->get_sigID())
            {
            case sid_DISCONNECT:   // master (local APL interpreter) disconnect
                 if (verbose)   get_CERR() << AP_NAME << " got DISCONNECT" << endl;
                 goon = false;
                 continue;

            case sid_NEW_VARIABLE:     // a new (not yet matched) offer
                 if (verbose)   get_CERR() << AP_NAME << " got NEW_VARIABLE" << endl;
                 {
                   const SV_key key = signal->get__NEW_VARIABLE__key();
                   const uint32_t * varname = Svar_DB::get_varname(key);
                  if (varname == 0)
                     {
                       get_CERR() << "Could not find svar for key "
                            << key << " at " << LOC << endl;
                       continue;
                     }

                   if (! is_valid_varname(varname))
                      {
                        get_CERR() << "Bad varname: ";
                        while (*varname)   get_CERR() << (Unicode)(*varname++);
                        get_CERR() << " at " << LOC << endl;
                        continue;
                      }

                   add_var(key);
                 }
                 continue;

            case sid_MAKE_OFFER:        // a new offer from a peer
                 if (verbose)   get_CERR() << AP_NAME << " got MAKE_OFFER" << endl;
                 {
                   const SV_key key = signal->get__MAKE_OFFER__key();
                   const uint32_t * varname = Svar_DB::get_varname(key);
                  if (varname == 0)
                     {
                       get_CERR() << "Could not find svar for key "
                            << key << " at " << LOC << endl;
                       continue;
                     }

                   if (! is_valid_varname(varname))
                      {
                       get_CERR() << "Bad varname ";
                        while (*varname)   get_CERR() << (Unicode)(*varname++);
                        get_CERR() << " at " << LOC << endl;
                       continue;
                      }

                   // make a counter offer (APs 100 and 210) or not (APnnn)
                   //
                   if (!make_counter_offer(key))   continue;   // APnnn

                   SV_Coupling coupling = NO_COUPLING;
                   const AP_num3 offering_id = Svar_DB::find_offering_id(key);

                   Svar_DB::match_or_make(varname, offering_id,
                                          this_proc, coupling);

                   add_var(key);

                   Svar_DB::set_state(key, true, LOC);
                 }
                 continue;

            case sid_OFFER_MATCHED:     // our offer was matched
                 if (verbose)   get_CERR() << AP_NAME << " got OFFER_MATCHED" << endl;
                 {
                   const SV_key key = signal->get__OFFER_MATCHED__key();

                   add_var(key);
                   Svar_DB::add_event(key, ProcessorID::get_id(),
                                      SVE_OFFER_MATCHED);
                 }
                 continue;

            case sid_RETRACT_OFFER:     // ⎕SVR varname
                 if (verbose)   get_CERR() << AP_NAME << " got RETRACT_OFFER" << endl;
                 {
                   const SV_key key = signal->get__RETRACT_OFFER__key();
                   for (int c = 0; c < coupled_vars.size(); ++c)
                       {
                         Coupled_var & cv = coupled_vars[c];
                         if (key == cv.key)
                            {
                              retract(cv);
                              coupled_vars.erase(coupled_vars.begin() + c);

                              if (coupled_vars.size() == 0 && auto_started)
                                 {
                                   if (verbose)      get_CERR() << AP_NAME << " done"
                                      " (last variable retracted)" << endl;
                                   goon  = false;
                                   break;
                                 }
                           }
                       }
                   Svar_DB::add_event(key, ProcessorID::get_id(),
                                      SVE_OFFER_RETRACT);
                 }
                 continue;

            case sid_GET_VALUE:
                 if (verbose)   get_CERR() << AP_NAME << " got GET_VALUE" << endl;
                 {
                   const SV_key key = signal->get__GET_VALUE__key();
                   APL_error_code error = E_VALUE_ERROR;
                   bool found = false;
                   string data;
                   for (int c = 0; c < coupled_vars.size(); ++c)
                       {
                         Coupled_var & cv = coupled_vars[c];
                         if (key == cv.key)
                            {
                              found = true;
                              error_loc = LOC;   error = get_value(cv, data);
                              break;
                           }
                       }

                   if (!found)
                      {
                        get_CERR() << "Key 0x" << hex << key << dec
                             << " not found. Variables are:"
                             << endl;
                        print_vars(get_CERR());
                        error_loc = LOC;   error = E_VALUE_ERROR;
                      }

                   VALUE_IS_c response(sock, key, error, error_loc, data);
                 }
                 continue;

            case sid_ASSIGN_VALUE:
                 if (verbose)   get_CERR() << AP_NAME << " got ASSIGN_VALUE" << endl;
                 {
                   const SV_key key = signal->get__ASSIGN_VALUE__key();
                   APL_error_code error = E_VALUE_ERROR;
                   bool found = false;
                   for (int c = 0; c < coupled_vars.size(); ++c)
                       {
                         Coupled_var & cv = coupled_vars[c];
                         if (key == cv.key)
                            {
                              found = true;
                              error_loc = LOC;   error = assign_value(cv,
                                         signal->get__ASSIGN_VALUE__cdr_value());
                              break;
                           }
                       }

                   if (!found)
                      {
                        get_CERR() << "Key 0x" << hex << key << dec
                             << " not found. Variables are:"
                             << endl;
                        print_vars(get_CERR());
                        error_loc = LOC;   error = E_VALUE_ERROR;
                      }

                   SVAR_ASSIGNED_c response(sock, key, error, error_loc);
                 }
                 continue;

            default: get_CERR() << pref << ": bad signal ID "
                          << signal->get_sigID() << endl;
          }
       }

      { UNREGISTER_PROCESSOR_c request(tcp, this_proc.id.proc,
                                            this_proc.id.parent,
                                            this_proc.id.grand,
                                            progname);
      }

   return 0;
}
//-----------------------------------------------------------------------------
ostream & operator << (ostream & out, const AP_num3 & ap3)
{
   return out << ap3.proc << "." << ap3.parent << "." << ap3.grand;
}
//-----------------------------------------------------------------------------
ostream &
operator << (ostream & os, Unicode uni)
{
   if (uni < 0x80)      return os << (char)uni;

   if (uni < 0x800)     return os << (char)(0xC0 | (uni >> 6))
                                  << (char)(0x80 | (uni & 0x3F));

   if (uni < 0x10000)    return os << (char)(0xE0 | (uni >> 12))
                                   << (char)(0x80 | (uni >>  6 & 0x3F))
                                   << (char)(0x80 | (uni       & 0x3F));

   if (uni < 0x200000)   return os << (char)(0xF0 | (uni >> 18))
                                   << (char)(0x80 | (uni >> 12 & 0x3F))
                                   << (char)(0x80 | (uni >>  6 & 0x3F))
                                   << (char)(0x80 | (uni       & 0x3F));

   if (uni < 0x4000000)  return os << (char)(0xF8 | (uni >> 24))
                                   << (char)(0x80 | (uni >> 18 & 0x3F))
                                   << (char)(0x80 | (uni >> 12 & 0x3F))
                                   << (char)(0x80 | (uni >>  6 & 0x3F))
                                   << (char)(0x80 | (uni       & 0x3F));

   return os << (char)(0xFC | (uni >> 30))
             << (char)(0x80 | (uni >> 24 & 0x3F))
             << (char)(0x80 | (uni >> 18 & 0x3F))
             << (char)(0x80 | (uni >> 12 & 0x3F))
             << (char)(0x80 | (uni >>  6 & 0x3F))
             << (char)(0x80 | (uni       & 0x3F));
}
//-----------------------------------------------------------------------------
