/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2022  Dr. Jürgen Sauermann

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

#ifndef __SHAPE_HH_DEFINED__
#define __SHAPE_HH_DEFINED__

#include "Common.hh"
#include "Error_macros.hh"

#include <string.h>   // memset()

// ----------------------------------------------------------------------------
/// the shape of an APL value
class Shape
{
public:
   /// constructor: shape of a scalar.
   Shape()
   : rho_rho(0),
     volume(1)
   { memset(&rho, 0, sizeof(rho)); }

   /// constructor: shape of a vector of length \b len
   Shape(ShapeItem len)
   : rho_rho(1),
     volume(len)
   { rho[0] = len; }

   /// constructor: shape of a matrix with \b rows rows and \b cols columns
   Shape(ShapeItem rows, ShapeItem cols)
   : rho_rho(2),
     volume(rows*cols)
   { rho[0] = rows;   rho[1] = cols; }

   /// constructor: shape of a cube
   Shape(ShapeItem height, ShapeItem rows, ShapeItem cols)
   : rho_rho(3),
     volume(height*rows*cols)
   { rho[0] = height;   rho[1] = rows;   rho[2] = cols; }

   /// constructor: arbitrary shape
   Shape(uRank rk, const ShapeItem * sh)
   : rho_rho(0),
     volume(1)
   { loop(r, rk)   add_shape_item(sh[r]); }

   /// constructor: shape of another shape
   Shape(const Shape & other)
   : rho_rho(other.rho_rho),
     volume(other.volume)
   {
     /* this may trigger a somewhat bogus -Wmaybe-uninitialized warning but
        makes some workspaces 3 times faster. The slower code with no warning
        would be:

        loop(r, rhorho)   rho[r] = other.rho[r];

        but that prevents loop-unrolling.
     */
     loop(r, MAX_RANK)   rho[r] = other.rho[r];
   }

   /// constructor: shape defined by the ravel of an APL value \b A
   /// throw RANK or LENGTH error where needed. Negative values are allowed
   /// in order to support e.g. ¯4 ↑ B
   Shape(const Value & A, int qio_A);

   /// return a shape like this, but with negative elements made positive
   Shape abs() const;

   /// return a shape with the upper \b cnt dimensions of this shape
   Shape high_shape(uRank cnt) const
      { Assert(cnt <= get_rank());   return Shape(cnt, rho); }

   /// return a shape with the lower \b cnt dimensions of this shape
   Shape low_shape(uRank cnt) const
      { Assert(cnt <= get_rank());
        return Shape(cnt, rho + (get_rank() - cnt)); }

   /// catenate two shapes, \b this shape provides the higher dimensions while
   /// \b lower provides the lower dimensions of the shape returned
   Shape operator +(const Shape & lower) const
   { Shape ret(*this);
     loop(r, lower.rho_rho)    ret.add_shape_item(lower.rho[r]);
     return ret; }

   /// return true iff \b this shape equals \b other
   bool operator ==(const Shape & other) const;

   /// return true iff \b this shape is different from \b other
   bool operator !=(const Shape & other) const
      { return ! (*this == other); }

   /// return the rank
   uRank get_rank() const
   { return rho_rho; }

   /// return the length of dimension \b r
   ShapeItem get_shape_item(sAxis r) const
   { Assert(r < rho_rho);   return rho[r]; }

   /// return the length of dimension \b r
   ShapeItem get_transposed_shape_item(sAxis r) const
   { Assert(r < rho_rho);   return rho[rho_rho - r - 1]; }

   /// return the length of the first axis, or 1 for scalars
   ShapeItem get_first_shape_item() const
   { return rho_rho ? rho[0] : 1; }

   /// return the length of the last axis, or 1 for scalars
   ShapeItem get_last_shape_item() const
   { return rho_rho ? rho[rho_rho - 1] : 1; }

   /// return the length of the last axis, or 1 for scalars
   ShapeItem get_cols() const
   { return rho_rho ?  rho[rho_rho - 1] : 1; }

   /// return the product of all but the the last dimension, or 1 for scalars
   ShapeItem get_rows() const
      { if (rho_rho == 0)   return 1;   // scalar
        if (const ShapeItem cols = rho[rho_rho - 1])   return volume / cols;
        ShapeItem count = 1;
        loop(r, rho_rho - 1)   count *= rho[r];
        return count; }

   /// modify dimension \b r
   void set_shape_item(sAxis r, ShapeItem sh)
      { Assert(r < rho_rho);
        if (rho[r])   { volume /= rho[r];  rho[r] = sh;  volume *= rho[r]; }
        else          { rho[r] = sh;   recompute_volume();                 } }

   /// recompute volume (after changing a dimension of len 0)
   void recompute_volume()
      { volume = 1;   loop(r, rho_rho)  volume *= rho[r]; }

   /// increment length of axis \b r by 1
   void increment_shape_item(sRank r)
      { set_shape_item(r, rho[r] + 1); }

   /// set last dimension to \b sh
   void set_last_shape_item(ShapeItem sh)
      { set_shape_item(rho_rho - 1, sh); }

   /// add a dimension of length \b len at the end
   void add_shape_item(ShapeItem len)
      { if (rho_rho >= MAX_RANK)   LIMIT_ERROR_RANK;
        rho[rho_rho++] = len;   volume *= len; }

   /// possibly increase rank by prepending axes of length 1
   void expand_rank(sRank new_rank)
      { if (rho_rho >= new_rank)   return;   // no need to expand
        const int diff = new_rank - rho_rho;
        for (sRank r = rho_rho - 1; r >= 0; --r)   rho[r + diff] = rho[r];
        loop(r, diff)      rho[r] = 1;
        rho_rho = new_rank;
      }

   /// possibly expand rank and increase axes so that B fits into this shape
   void expand(const Shape & B);

   /// return a shape like \b this, but with a new dimension of length len
   /// inserted so that Shape[axis] == len in the returned shape.
   Shape insert_axis(sAxis axis, ShapeItem len) const;

   /// return a shape like \b this, but with axis \b axis removed
   Shape without_axis(sAxis axis) const
      { Shape ret;
        loop(r, rho_rho)   if (r != axis)  ret.add_shape_item(rho[r]);
        return ret;
      }

   /// return a shape like \b this, but with the first axis (↑⍴) removed
   Shape without_first_axis() const
      { Shape ret;   // start with a scalar
        for (sRank r = 1; r < rho_rho;)   ret.add_shape_item(rho[r++]);
        return ret;
      }

   /// return a shape like \b this, but with the last axis (¯1↑⍴) removed
   Shape without_last_axis() const
      { Shape ret;   // start with a scalar
        for (sRank r = 0; r < (rho_rho - 1);)  ret.add_shape_item(rho[r++]);
        return ret;
      }

   /// return the number of ravel elements (1 for scalars,i
   ///  else product of shapes)
   ShapeItem get_volume() const
      { return volume; }

   /// return the number of ravel elements, but at least 1 (for empty values
   /// and scalars) else product of shapes)
   ShapeItem get_nz_volume() const
      { return volume ? volume : 1; }

   /// return true iff one of the shape elements is (axis) \b ax
   bool contains_axis(sAxis ax) const
      { loop(r, rho_rho)   if (rho[r] == ax)   return true;   return false; }

   /// return true iff the items of value are at most the items of \b this
   bool contains(const Shape & value) const
      { Assert(rho_rho == value.rho_rho);
        loop(r, rho_rho)   if (value.rho[r] >= rho[r])   return false;
        return true; }

   /// return ⌽ ×\ ⍴ ⌽ this
   Shape get_weights() const
      { Shape ret;   ret.rho_rho = rho_rho;
        ShapeItem weight = 1;
        ret.volume = 1;
        for (sRank r = rho_rho - 1; r >= 0; --r)
           {
             ret.rho[r] = weight;
             ret.volume *= weight;
             weight *= rho[r];
           }
        return ret;
      }

   /// throw an APL error if \b this shape differs from shape B
   void check_same(const Shape & B, ErrorCode rank_err, ErrorCode len_err,
                   const char * loc) const;

   /// return \b true iff \b this shape is empty (some dimension is 0).
   bool is_empty() const
      { loop(r, rho_rho)   if (rho[r] == 0)   return true;   return false; }

   /// return \b reue iff this shape is a permutation of 0, 1, ... rank-1
   bool is_permutation() const;

   /// return the position of \b idx in the ravel.
   ShapeItem ravel_pos(const Shape & idx) const;

   /// the inverse of \b ravel_pos()
   Shape offset_to_index(ShapeItem offset, int quad_io) const;

protected:
   /// the rank (number of valid shape items)
   sRank rho_rho;

   /// the shape
   ShapeItem rho[MAX_RANK];

   /// +/rho
   ShapeItem volume;
};
// ----------------------------------------------------------------------------
/// a shape of rank 3
class Shape3 : public Shape
{
public:
   /// construct a 3-dimensional shape from a n-dimensional shape
   Shape3(const Shape & shape, ShapeItem axis)
   : Shape(1, 1, 1)
   {
     loop(r, shape.get_rank())
        {
          if      (r < axis)   rho[0] *= shape.get_shape_item(r);
          else if (r > axis)   rho[2] *= shape.get_shape_item(r);
          else                 rho[1]  = shape.get_shape_item(r);
        }
   }

   /// construct a 3-dimensional shape from three explicit dimensions
   Shape3(ShapeItem _h, ShapeItem _m, ShapeItem _l)
   : Shape(_h, _m, _l)
   {}

   /// the dimensions above r the axis
   ShapeItem h() const
   { return rho[0]; }

   /// return the offset of ravel item [hh;mm;ll] (from [0;0;0])
   ShapeItem hml(ShapeItem hh, ShapeItem mm, ShapeItem ll) const
      { return ll + l()*(mm + m()*hh); }

   /// the axis dimension
   ShapeItem m() const
   { return rho[1]; }

   /// the dimensions below the axis
   ShapeItem l() const
   { return rho[2]; }
};
// ----------------------------------------------------------------------------

#endif // __SHAPE_HH_DEFINED__
