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

#include <string.h>

#include <iomanip>
#include <vector>

#include "Common.hh"
#include "Error.hh"
#include "PrintOperator.hh"
#include "TestFiles.hh"
#include "UCS_string.hh"
#include "ValueHistory.hh"

VH_entry VH_entry::history[VALUEHISTORY_SIZE];
int VH_entry::idx = 0;

//----------------------------------------------------------------------------
VH_entry::VH_entry(const Value * _val, VH_event _ev, int _iarg,
                   const char * _loc)
  : event(_ev),
    val(_val),
    iarg(_iarg),
    loc(_loc)
{
   testcase_file = TestFiles::get_testfile_name();
   testcase_line = TestFiles::get_current_lineno();
}
//----------------------------------------------------------------------------
void
VH_entry::init()
{
   memset(history, 0, sizeof(history));
}
//----------------------------------------------------------------------------
void
add_event(const Value * val, VH_event ev, int ia, const char * loc)
{
VH_entry * entry = VH_entry::history + VH_entry::idx++;
   if (VH_entry::idx >= VALUEHISTORY_SIZE)   VH_entry::idx = 0;

   new(entry) VH_entry(val, ev, ia, loc);
}
//----------------------------------------------------------------------------
void
print_value_history(ostream & out, const Value * val)
{
   // search backwards for events of val.
   //
vector<const VH_entry *> var_events;
int cidx = VH_entry::idx;

   loop(e, VALUEHISTORY_SIZE)
       {
         --cidx;
         if (cidx < 0)   cidx = VALUEHISTORY_SIZE - 1;

         const VH_entry * entry = VH_entry::history + cidx;
         
         if (entry->event == VHE_None)     break;      // end of history

         if (entry->event == VHE_Error)
            { 
              // add error event to every value history
              //
              var_events.push_back(entry);
              continue;
            }

          if (entry->val != val)            continue;   // some other var

          var_events.push_back(entry);

          if (entry->event == VHE_Create)   break;   // create event found
       }

   out << endl << "value " << (const void *)val
       << " has " << var_events.size()
       << " events in its history:" << endl;

int flags = 0;
const VH_entry * previous = 0;
   loop(e, var_events.size())
      {
        const VH_entry * vev = var_events[var_events.size() - e - 1];
        vev->print(flags, out, val, previous);
        previous = vev;
      }
   out << endl;
}
//----------------------------------------------------------------------------
static UCS_string
flags_name(ValueFlags flags)
{
UCS_string ret;

  if (flags & VF_marked)   ret.append(UNI_ASCII_M);
  if (flags & VF_complete) ret.append(UNI_ASCII_C);
  if (flags & VF_left)     ret.append(UNI_ASCII_AMPERSAND);
  if (flags & VF_arg)      ret.append(UNI_ASCII_A);
  if (flags & VF_eoc)      ret.append(UNI_ASCII_E);
  if (flags & VF_deleted)  ret.append(UNI_DELTA);
  if (flags & VF_index)    ret.append(UNI_SQUISH_QUAD);
  if (flags & VF_nested)   ret.append(UNI_ASCII_s);
  if (flags & VF_forever)  ret.append(UNI_INFINITY);
  if (flags & VF_assigned) ret.append(UNI_LEFT_ARROW);
  if (flags & VF_shared)   ret.append(UNI_NABLA);

   while (ret.size() < 4)   ret.append(UNI_ASCII_SPACE);
   return ret;
}
//----------------------------------------------------------------------------
void
VH_entry::print(int & flags, ostream & out, const Value * val,
               const VH_entry * previous) const
{
const ValueFlags flags_before = (ValueFlags)flags;

   if (previous == 0                            ||
       previous->testcase_file == 0             ||
       previous->testcase_line != testcase_line ||
       strcmp(previous->testcase_file, testcase_file))
      {
        if (testcase_file)   out << "  FILE:        " << testcase_file
                                 << ":" << testcase_line << endl;
      }

   switch(event)
      {
        case VHE_Create:
             out << "  VHE_Create   " << flags_before
                 << "              ";
             break;

        case VHE_Unroll:
             out << "  VHE_Unroll   " << flags_before
                 << "              ";
             break;

        case VHE_Check:
             flags |= VF_complete;
             out << "  VHE_Check   " << flags_before << " ";
             if (flags_before != flags)   out << (ValueFlags)flags << " ";
             else                         out << "            ";
             break;

        case VHE_SetFlag:
             flags |= iarg;
             out << "  Set " << flags_name((ValueFlags)iarg)
                 << "     " << flags_before << " ";
             if (flags_before != flags)   out << (ValueFlags)flags << " ";
             else                         out << "            ";
             break;

        case VHE_ClearFlag:
             flags &= ~iarg;
             out << "  Clear " << flags_name((ValueFlags)iarg)
                 << "   " << flags_before << " ";
             if (flags_before != flags)   out << (ValueFlags)flags << " ";
             else                         out << "            ";
             break;

        case VHE_Erase:
             out << "  VHE_Erase    " << flags_before << " ";
             if (flags_before & VF_deleted)
                {
                  out << "** TWICE ** ";
                }
             else if (flags_before & VF_DONT_DELETE)
                {
                  out << "still owned ";
                }
             else
                {
                  flags |= VF_deleted;
                  if (flags_before != flags)   out << (ValueFlags)flags << " ";
                  else                         out << "            ";
                }
             break;

        case VHE_Error:
             out << "  " << setw(36)
                 << Error::error_name((ErrorCode)iarg) << " ";
             break;

        case VHE_PtrNew:
             out << "  VHE_PtrNew   " << setw(26) << iarg;
             break;

        case VHE_PtrCopy1:
             out << "  VHE_PtrCopy1 " << setw(26) << iarg;
             break;

        case VHE_PtrCopy2:
             out << "  VHE_PtrCopy2 " << setw(26) << iarg;
             break;

        case VHE_PtrCopy3:
             out << "  VHE_PtrCopy2 " << setw(26) << iarg;
             break;

        case VHE_PtrClr:
             out << "  VHE_PtrDel   " << setw(26) << iarg;
             break;

        case VHE_PtrDel:
             out << "  VHE_PtrDel   " << setw(26) << iarg;
             break;

        case VHE_TokCopy1:
             out << "  VHE_TokCopy1 " << setw(26) << iarg;
             break;

        case VHE_TokMove1:
             out << "  VHE_TokMove1 " << setw(26) << iarg;
             break;

        case VHE_TokMove2:
             out << "  VHE_TokMove2 " << setw(26) << iarg;
             break;

        case VHE_Completed:
             out << "  VHE_Completed" << setw(26) << iarg;
             break;

        case VHE_Stale:
             out << "  VHE_Stale    " << setw(26) << iarg;
             break;

        default:
             out << "Unknown event  " << HEX(event) << endl;
             return;
      }

   out << left << setw(30) << (loc ? loc : "<no-loc>") << endl;
}
//----------------------------------------------------------------------------

