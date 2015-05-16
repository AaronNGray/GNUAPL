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

#include "Bif_OPER2_INNER.hh"
#include "Bif_OPER1_REDUCE.hh"
#include "PointerCell.hh"
#include "Workspace.hh"

Bif_OPER2_INNER   Bif_OPER2_INNER::_fun;
Bif_OPER2_INNER * Bif_OPER2_INNER::fun = &Bif_OPER2_INNER::_fun;

Bif_OPER2_INNER::PJob_product Bif_OPER2_INNER::job;

//-----------------------------------------------------------------------------
Token
Bif_OPER2_INNER::eval_ALRB(Value_P A, Token & _LO, Token & _RO, Value_P B)
{
   if (!_LO.is_function())    SYNTAX_ERROR;
   if (!_RO.is_function())    SYNTAX_ERROR;

Function * LO = _LO.get_function();
Function * RO = _RO.get_function();

   if (!RO->has_result())   DOMAIN_ERROR;
   if (!LO->has_result())   DOMAIN_ERROR;

   return inner_product(A, _LO, _RO, B);
}
//-----------------------------------------------------------------------------
void
Bif_OPER2_INNER::scalar_inner_product() const
{
#ifdef PERFORMANCE_COUNTERS_WANTED
#ifdef HAVE_RDTSC
const uint64_t start_1 = cycle_counter();
#endif
#endif

  // the empty cases have been ruled out already in inner_product()

const ShapeItem Z_len = job.ZAh * job.ZBl;
   job.ec = E_NO_ERROR;

#if PARALLEL_ENABLED
   if (  Parallel::run_parallel
      && Thread_context::get_active_core_count() > 1
      && Z_len > get_dyadic_threshold())
      {
        job.cores = Thread_context::get_active_core_count();
        Thread_context::do_work = PF_scalar_inner_product;
        Thread_context::M_fork("scalar_inner_product");   // start pool
        PF_scalar_inner_product(Thread_context::get_master());
        Thread_context::M_join();
      }
   else
#endif // PARALLEL_ENABLED
      {
        job.cores = CCNT_1;
        PF_scalar_inner_product(Thread_context::get_master());
      }

#ifdef PERFORMANCE_COUNTERS_WANTED
#ifdef HAVE_RDTSC
const uint64_t end_1 = cycle_counter();
   Performance::fs_OPER2_INNER_AB.add_sample(end_1 - start_1, Z_len);
#endif
#endif
}
//-----------------------------------------------------------------------------
void
Bif_OPER2_INNER::PF_scalar_inner_product(Thread_context & tctx)
{
const ShapeItem Z_len = job.ZAh * job.ZBl;

const ShapeItem slice_len = (Z_len + job.cores - 1)/job.cores;
ShapeItem z = tctx.get_N() * slice_len;
ShapeItem end_z = z + slice_len;
   if (end_z > Z_len)   end_z = Z_len;

Cell product;   // the result of LO, e.g. of × in +/×

   for (; z < end_z; ++z)
       {
        const ShapeItem zah = z/job.ZBl;         // z row = A row
        const ShapeItem zbl = z - zah*job.ZBl;   // z column = B column
        const Cell * row_A = job.cA + job.incA*(zah * job.LO_len);
        const Cell * col_B = job.cB + job.incB*zbl;
        loop(l, job.LO_len)
           {
             job.ec = (col_B->*job.RO)(&product, row_A);
             if (job.ec != E_NO_ERROR)   return;

             if (l == 0)   // first element
                {
                  job.ec = (col_B->*job.RO)(job.cZ + z, row_A);
                  if (job.ec != E_NO_ERROR)   return;
                }
             else
                {
                  job.ec = (col_B->*job.RO)(&product, row_A);
                  if (job.ec != E_NO_ERROR)   return;
                  job.ec = (product.*job.LO)(job.cZ + z, job.cZ + z);
                  if (job.ec != E_NO_ERROR)   return;
                }
            row_A += job.incA;
            col_B += job.incB*job.ZBl;
           }
       }
}
//-----------------------------------------------------------------------------
Token
Bif_OPER2_INNER::inner_product(Value_P A, Token & _LO, Token & _RO, Value_P B)
{
Function * LO = _LO.get_function();
Function * RO = _RO.get_function();

   Assert1(LO);
   Assert1(RO);

Shape shape_A1;
ShapeItem len_A = 1;
   if (!A->is_scalar())
      {
        len_A = A->get_last_shape_item();
        shape_A1 = A->get_shape().without_axis(A->get_rank() - 1);
      }

Shape shape_B1;
ShapeItem len_B = 1;
   if (!B->is_scalar())
      {
        len_B = B->get_shape_item(0);
        shape_B1 = B->get_shape().without_axis(0);
      }

   // we do not check len_A == len_B here, since a non-scalar LO may
   // accept different lengths of its left and right arguments

const ShapeItem items_A = shape_A1.get_volume();
const ShapeItem items_B = shape_B1.get_volume();

   if (items_A == 0 || items_B == 0)   // empty result
      {
        // the outer product portion of LO.RO is empty.
        // Apply the fill function of RO
        //
        const Shape shape_Z = shape_A1 + shape_B1;
        return fill(shape_Z, A, RO, B, LOC);
      }

Value_P Z(shape_A1 + shape_B1, LOC);

   // an important (and the most likely) special case is LO and RO being scalar
   // functions. This case can be implemented in a far simpler fashion than
   // the general case.
   //
   job.LO = LO->get_scalar_f2();
   job.RO = RO->get_scalar_f2();
   if (job.LO && job.RO && A->is_simple() && B->is_simple() && len_A)
      {
        if (len_A != len_B &&
            !(A->is_scalar() || B->is_scalar()))   LENGTH_ERROR;

        job.cZ     = &Z->get_ravel(0);
        job.cA     = &A->get_ravel(0);
        job.incA   = A->is_scalar() ? 0 : 1;
        job.ZAh    = items_A;
        job.LO_len = A->is_scalar() ? len_B : len_A;
        job.cB     = &B->get_ravel(0);
        job.incB   = B->is_scalar() ? 0 : 1;
        job.ZBl    = items_B;
        job.ec     = E_NO_ERROR;

        scalar_inner_product();
        if (job.ec != E_NO_ERROR)   throw_apl_error(job.ec, LOC);

        Z->set_default(*B.get());
 
        Z->check_value(LOC);
        return Token(TOK_APL_VALUE1, Z);
      }

EOC_arg arg(Z, A, LO, RO, B, LOC);
INNER_PROD & _arg = arg.u.u_INNER_PROD;
   _arg.A_enclosed = arg.A->get_rank() > 1;
   _arg.B_enclosed = arg.B->get_rank() > 1;

   // enclose last axis of A if necessary
   //
   if (_arg.A_enclosed)
      {
        const Shape last_axis(A->get_rank() - 1);
        arg.A = Bif_F12_PARTITION::enclose_with_axes(last_axis, A);
      }
   
   // enclose first axis of B if necessary
   //
   if (_arg.B_enclosed)
      {
        const Shape first_axis(0);
        arg.B = Bif_F12_PARTITION::enclose_with_axes(first_axis, B);
      }
   
   _arg.items_A = items_A;
   _arg.items_B = items_B;

   _arg.how = 0;
   _arg.last_ufun = 0;
   return finish_inner_product(arg, true);
}
//-----------------------------------------------------------------------------
Token
Bif_OPER2_INNER::finish_inner_product(EOC_arg & arg, bool first)
{
INNER_PROD & _arg = arg.u.u_INNER_PROD;

   if (_arg.how == 1)   goto RO_done;
   if (_arg.how == 2)   goto LO_done;

   Assert1(_arg.how == 0);

   while (++arg.z < _arg.items_A * _arg.items_B)
      {
        {
          const ShapeItem a = arg.z / _arg.items_B;
          const ShapeItem b = arg.z % _arg.items_B;

          Value_P RO_A(arg.A, LOC);
          if (_arg.A_enclosed)
             {
               RO_A = arg.A->get_ravel(a).get_pointer_value();
             }

          Value_P RO_B(arg.B, LOC);
          if (_arg.B_enclosed)
             {
               RO_B = arg.B->get_ravel(b).get_pointer_value();
             }

          const Token T1 = arg.RO->eval_AB(RO_A, RO_B);

          if (T1.get_tag() == TOK_ERROR)   return T1;

          if (T1.get_tag() == TOK_SI_PUSHED)
             {
               // RO is a user defined function. If LO is also user defined
               // then the context pushed is not the last user defined function
               // call for this operator. Otherwise it may or may not be the
               //  last call.
               //
               if (a >= (_arg.items_A - 1) && b >= (_arg.items_B - 1))
                  {
                    // at this point, the last ravel element of Z is being
                    // computed. If LO is not user defined, then this call is
                    // the last user defined function call.
                    // Otherwise the LO-call below will be the last.
                    //
                    if (!arg.LO->may_push_SI())   _arg.last_ufun = 1;
                  }

               _arg.how = 1;   // user-defined RO
               if (first)   // first call
                  Workspace::SI_top()->add_eoc_handler(eoc_RO, arg, LOC);
               else           // subsequent call
                  Workspace::SI_top()->move_eoc_handler(eoc_RO, &arg, LOC);

               return T1;   // continue in user defined function...
             }

          // RO was a system function. Set Z[z] ← A[a] RO B1[b]
          //
          Value_P A_RO_B = T1.get_apl_val();
          new (arg.Z->next_ravel()) PointerCell(A_RO_B, arg.Z.getref());
        }

RO_done:
        // at this point Z[z] is the result of a system- or user-defined
        // function RO. Compute LO/Z[z].
        {
          Token LO(TOK_FUN1, arg.LO);
          Value_P ZZ = arg.Z->get_ravel(arg.z).get_pointer_value();
          const Token T2 = Bif_OPER1_REDUCE::fun->eval_LB(LO, ZZ);
          arg.Z->get_ravel(arg.z).release(LOC);

          if (T2.get_tag() == TOK_ERROR)   return T2;
          if (T2.get_tag() == TOK_SI_PUSHED)   // user-defined LO
             {
               _arg.how = 2;   // user-defined LO
               _arg.last_ufun = (arg.z >= _arg.items_A * _arg.items_B - 1);
               Workspace::SI_top()->add1_eoc_handler(eoc_LO, arg, LOC);
               return T2;
             }

          // LO was a system function. store it in Z 
          //
          arg.Z->get_ravel(arg.z).init_from_value(T2.get_apl_val(),
                                               arg.Z.getref(), LOC);
        }

LO_done: ;
      }

   arg.Z->set_default(*arg.B.get());
 
   arg.Z->check_value(LOC);
   return Token(TOK_APL_VALUE1, arg.Z);
}
//-----------------------------------------------------------------------------
bool
Bif_OPER2_INNER::eoc_LO(Token & token)
{
   if (token.get_Class() != TC_VALUE)   return false;   // stop it

EOC_arg * arg = Workspace::SI_top()->remove_eoc_handlers();
EOC_arg * next = arg->next;
INNER_PROD & _arg = arg->u.u_INNER_PROD;

   if (!_arg.last_ufun)   Workspace::pop_SI(LOC);

   // a user defined function has returned a value. Store it.
   //
   Assert(_arg.how == 2);
   arg->Z->get_ravel(arg->z).init_from_value(token.get_apl_val(),
                                         arg->Z.getref(), LOC);
   copy_1(token, finish_inner_product(*arg, false), LOC);
   if (token.get_tag() == TOK_SI_PUSHED)   return true;   // continue

   delete arg;
   Workspace::SI_top()->set_eoc_handlers(next);
   if (next)   return next->handler(token);

   return false;
}
//-----------------------------------------------------------------------------
bool
Bif_OPER2_INNER::eoc_RO(Token & token)
{
   if (token.get_Class() != TC_VALUE)   return false;   // stop it

EOC_arg * arg = Workspace::SI_top()->remove_eoc_handlers();
EOC_arg * next = arg->next;
INNER_PROD & _arg = arg->u.u_INNER_PROD;

   if (!_arg.last_ufun)   Workspace::pop_SI(LOC);

   // a user defined RO has returned a value. Store it.
   //
   Assert(_arg.how == 1);
Value_P A_RO_B = token.get_apl_val();
   new (arg->Z->next_ravel()) PointerCell(A_RO_B, arg->Z.getref());

   copy_1(token, finish_inner_product(*arg, false), LOC);
   if (token.get_tag() == TOK_SI_PUSHED)   return true;   // continue

   delete arg;
   Workspace::SI_top()->set_eoc_handlers(next);
   if (next)   return next->handler(token);

   return false;
}
//-----------------------------------------------------------------------------
