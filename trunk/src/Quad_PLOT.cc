/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2017  Dr. Jürgen Sauermann

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

#include <iomanip>

#include "Quad_PLOT.hh"

Quad_PLOT  Quad_PLOT::_fun;
Quad_PLOT * Quad_PLOT::fun = &Quad_PLOT::_fun;

sem_t Quad_PLOT::plot_threads_sema;
Simple_string<pthread_t, false> Quad_PLOT::plot_threads;

#if defined(HAVE_XCB_XCB_H)

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "ComplexCell.hh"
#include "FloatCell.hh"
#include "Workspace.hh"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include <iostream>
#include <iomanip>

using namespace std;

typedef uint16_t  Pixel;
typedef uint32_t Color;
typedef std::string String;

//=============================================================================
/// data for one plot line
class Plot_data_row
{
public:
   /// constructor
   Plot_data_row(const double * pX, const double * pY, uint32_t idx,
                 uint32_t len)
   : X(pX), Y(pY), row_num(idx), N(len)
   {
     min_X = max_X = get_X(0);
     min_Y = max_Y = get_Y(0);
     for (unsigned int n = 1; n < N; ++n)
         {
            const double Xn = get_X(n);
            if (min_X > Xn)   min_X = Xn;
            if (max_X < Xn)   max_X = Xn;

            const double Yn = get_Y(n);
            if (min_Y > Yn)   min_Y = Yn;
            if (max_Y < Yn)   max_Y = Yn;
         }
   }

   /// destructor
   ~Plot_data_row()
   {
     // all rows belong to the same double *, so we only delete row 0
     //
     if (row_num == 0)   delete[] X;
   }

   /// return the number of data points
   uint32_t get_N() const
      { return N; }

   /// return the n-th X coordinate
   double get_X(uint32_t n) const
      { return X ? X[n] : n; }

   /// return the n-th Y coordinate
   double get_Y(uint32_t n) const
      { return Y[n]; }

   /// return the smallest value in X
   double get_min_X() const
      { return min_X; }

   /// return the largest value in X
   double get_max_X() const
      { return max_X; }

   /// return the smallest value in Y
   double get_min_Y() const
      { return min_Y; }

   /// return the largest value in X (if present) otherwise N-1
   double get_max_Y() const
      { return max_Y; }

protected:
   /// the X coordinates
   const double * X;

   /// the Y coordinates
   const double * Y;

   /// the row number
   const uint32_t row_num;

   /// the row length
   const uint32_t N;

   /// the smallest value in X
   double min_X;

   /// the largest value in X
   double max_X;

   /// the smallest value in Y
   double min_Y;

   /// the largest value in Y
   double max_Y;
};
//=============================================================================
class Plot_data
{
public:
   /// constructor
   Plot_data(uint32_t rows, uint32_t cols)
   : row_count(rows),
     col_count(cols),
     idx(0)
      {
        data_rows = new const Plot_data_row *[row_count];
      }

   /// destructor
   ~Plot_data()
   {
     loop(r, idx)   delete data_rows[r];
     delete data_rows;
   }

   /// return the number of plot lines
   uint32_t get_row_count() const
      { return row_count; }

   /// return the number of data points
   uint32_t get_col_count() const
      { return col_count; }

   /// return the X and Y values of the data matrix row/col
   void get_XY(double & X, double & Y, uint32_t row, uint32_t col) const
        {
          Assert(row < idx);
          X = data_rows[row]->get_X(col);
          Y = data_rows[row]->get_Y(col);
        }

   /// return the smallest X value in the data matrix
   double get_min_X() const
      { return min_X; }

   /// return the largest X value in the data matrix
   double get_max_X() const
      { return max_X; }

   /// return the smallest Y value in the data matrix
   double get_min_Y() const
      { return min_Y; }

   /// return the largest Y value in the data matrix
   double get_max_Y() const
      { return max_Y; }

   /// add a row to the data matrix
   void add_row(const Plot_data_row * row)
      {
        Assert(idx < row_count);
        data_rows[idx++] = row;
        if (idx == 1)   // first row
           {
             min_X = row->get_min_X();
             max_X = row->get_max_X();
             min_Y = row->get_min_Y();
             max_Y = row->get_max_Y();
           }
        else
           {
             if (min_X > row->get_min_X())   min_X = row->get_min_X();
             if (max_X < row->get_max_X())   max_X = row->get_max_X();
             if (min_Y > row->get_min_Y())   min_Y = row->get_min_Y();
             if (max_Y < row->get_max_Y())   max_Y = row->get_max_Y();
           }
      }

   /// convert a number to a string
   static const char * uint32_t_to_str(uint32_t val)
      {
        static char ret[40];
        snprintf(ret, sizeof(ret), "%u", val);
        ret[sizeof(ret) - 1] = 0;
        return ret;
      }

   /// convert a Pixel to a string
   static const char * Pixel_to_str(Pixel val)
      {
        static char ret[40];
        snprintf(ret, sizeof(ret), "%u pixel", val);
        ret[sizeof(ret) - 1] = 0;
        return ret;
      }

   /// convert a double to a string
   static const char * double_to_str(double val)
      {
        static char ret[40];
        snprintf(ret, sizeof(ret), "%lf pixel", val);
        ret[sizeof(ret) - 1] = 0;
        return ret;
      }

   /// convert a Color to a string
   static const char * Color_to_str(Color val)
      {
        static char ret[40];
        snprintf(ret, sizeof(ret), "#%2.2X%2.2X%2.2X",
                 val >> 16 & 0xFF, val >>  8 & 0xFF, val       & 0xFF);
         ret[sizeof(ret) - 1] = 0;
         return ret;
      }

   /// convert a String to a string
   static String String_to_str(String val)
      {
        return val;
      }

   /// convert a string to a Pixel
   static Pixel Pixel_from_str(const char * str, const char * & error)
      { error = 0;   return strtol(str, 0, 10); }

   /// convert a string to a double
   static double double_from_str(const char * str, const char * & error)
      { error = 0;   return strtold(str, 0); }

   static Color Color_from_str(const char * str, const char * & error);

   /// convert a string to a uint32_t
   static uint32_t uint32_t_from_str(const char * str, const char * & error)
      { error = 0;   return strtol(str, 0, 10); }

   /// convert a string to a String
   static String String_from_str(const char * str, const char * & error)
      { error = 0;   return str; }

protected:
   /// the rows of the data matrix (0-terminated)
   const Plot_data_row ** data_rows;

   /// the number of rows in the data matrix
   const uint32_t row_count;

   /// the number of columns in the data matrix
   const uint32_t col_count;

   /// the current number of columns in the data matrix
   uint32_t idx;

   /// the smallest value in X
   double min_X;

   /// the largest value in X
   double max_X;

   /// the smallest value in Y
   double min_Y;

   /// the largest value in Y
   double max_Y;
};
//-----------------------------------------------------------------------------
Color
Plot_data::Color_from_str(const char * str, const char * & error)
{
   error = 0;   // assume no error

uint32_t r, g, b;
   if (3 == sscanf(str, " %u %u %u", &r, &b, &g))
      return (r & 0xFF) << 16 | (g & 0xFF) << 8 | (b & 0xFF);

   if (const char * h = strchr(str, '#'))
      {
        if (strlen(h) == 4)   // #RGB
           {
             const int v = strtol(str + 1, 0, 16);
             return (0x11*(v >> 8 & 0x0F)) << 16
                  | (0x11*(v >> 4 & 0x0F)) << 8
                  | (0x11*(v      & 0x0F)  << 0);
           }
        else if (strlen(h) == 7)   // #RRGGBB
           {
             return strtol(str + 1, 0, 16);
           }
      }

   error = "Bad color format";
   return 0;
}
//=============================================================================
/// properties of a single plot line
class Plot_line_properties
{
public:
   /// constructor
   Plot_line_properties(int lnum) :
#define gdef(_ty, _na, _val, _descr) 
#define ldef(_ty,  na,  val, _descr) na(val),
#include "Quad_PLOT.def"
   line_number(lnum)
   {
     snprintf(legend_name_buffer, sizeof(legend_name_buffer), "Line-%u", lnum);
     legend_name = legend_name_buffer;
     switch(lnum)
        {
           default: line_color = point_color = 0x000000;   break;
           case 0:  line_color = point_color = 0x00E000;   break;
           case 1:  line_color = point_color = 0xFF0000;   break;
           case 2:  line_color = point_color = 0x0000FF;   break;
           case 3:  line_color = point_color = 0xFF00FF;   break;
           case 4:  line_color = point_color = 0x00FFFF;   break;
           case 5:  line_color = point_color = 0xFFFF00;   break;
        }
   }

   // get_XXX() and set_XXX functions
#define gdef(_ty, _na, _val, _descr)
#define ldef(ty,  na,  _val, _descr) ty get_ ## na() const   { return na; } \
                             void set_ ## na(ty val)   { na = val; }
#include "Quad_PLOT.def"

   int print(ostream & out) const;

#define gdef(_ty, _na, _val, _descr)
#define ldef(ty,  na,  _val, _descr) ty na;
#include "Quad_PLOT.def"
  const int line_number;
   char legend_name_buffer[50];
};
//-----------------------------------------------------------------------------
int
Plot_line_properties::print(ostream & out) const
{
char cc[40];
#define gdef(_ty, _na, _val, _descr)
#define ldef(ty,  na,  _val, _descr)                      \
   snprintf(cc, sizeof(cc), #na "-%u:  ", line_number);   \
   CERR << setw(20) << cc << Plot_data::ty ## _to_str(na) << endl;
#include "Quad_PLOT.def"

   return 0;
}
//=============================================================================
/// properties of the entire plot window
class Plot_window_properties
{
public:
   /// constructor
   Plot_window_properties(const Plot_data * data, int verbosity);

   /// destructor
   ~Plot_window_properties()
      {
        delete &plot_data;
      }

   /// update derived properties after changing primary ones
   void update(int verbosity);

   /// handle window resize event
   void set_window_size(int width, int height);

   // get_XXX() and set_XXX functions
#define ldef(_ty, _na, _val, _descr)
#define gdef(ty,  na,  _val, _descr) ty get_ ## na() const   { return na; } \
                             void set_ ## na(ty val)   { na = val; }
#include "Quad_PLOT.def"

   Plot_line_properties ** get_line_properties() const
      { return line_properties; }

   Pixel valX2pixel(double val_X) const
      { return pa_border_L + val_X * scale_X; }

   Pixel valY2pixel(double val_Y) const
      { return pa_border_T + pa_height - val_Y * scale_Y; }

   int print(ostream & out) const;

   /// for e.g. att_and_val = "pa_width: 600" set pa_width to 600.
   /// Return error string on error.
   const char * set_attribute(const char * att_and_val);

   Pixel  get_window_width() const    { return window_width; }
   Pixel  get_window_height() const   { return window_height; }
   double get_min_X() const           { return min_X; }
   double get_max_X() const           { return max_X; }
   double get_min_Y() const           { return min_Y; }
   double get_max_Y() const           { return max_Y; }
   double get_scale_X() const         { return scale_X; }
   double get_scale_Y() const         { return scale_Y; }
   double get_tile_X() const          { return tile_X; }
   double get_tile_Y() const          { return tile_Y; }
   int    get_gridx_last() const      { return gridx_last; }
   int    get_gridy_last() const      { return gridy_last; }
   int    get_verbosity() const       { return verbosity; }
   void  set_verbosity(int verb)      { verbosity = verb; }

   const Plot_data & get_plot_data() const   { return plot_data; }

protected:
   const int line_count;
   const Plot_data & plot_data;

#define ldef(_ty, _na, _val, _descr)
#define gdef(ty,  na,  _val, _descr) ty na;
#include "Quad_PLOT.def"

   Pixel window_width;
   Pixel window_height;
   double min_X;
   double max_X;
   double min_Y;
   double max_Y;
   double scale_X;
   double scale_Y;
   double tile_X;
   double tile_Y;
   int gridx_last;
   int gridy_last;
   int verbosity;

   Plot_line_properties ** line_properties;

   /// round val up to the next higher 1/2/5×10^N
   static double round_up_125(double val);
};
//-----------------------------------------------------------------------------
Plot_window_properties::Plot_window_properties(const Plot_data * data,
                                               int verbosity) :
   line_count(data->get_row_count()),
   plot_data(*data),
#define gdef(_ty,  na,  val, _descr) na(val),
#define ldef(_ty, _na, _val, _descr) 
#include "Quad_PLOT.def"

   window_width(pa_border_L + pa_width  + pa_border_R),
   window_height(pa_border_T + pa_height + pa_border_B),
   min_X(data->get_min_X()),
   max_X(data->get_max_X()),
   min_Y(data->get_min_Y()),
   max_Y(data->get_max_Y()),
   scale_X(0),
   scale_Y(0),
   tile_X(0),
   tile_Y(0),
   verbosity(0)
{
   (verbosity > 1) && CERR << setw(20) << "min_X: " << min_X << endl
                           << setw(20) << "max_X: " << max_X << endl
                           << setw(20) << "min_Y: " << min_Y << endl
                           << setw(20) << "max_Y: " << max_Y << endl;

   line_properties = new Plot_line_properties *[line_count + 1];
   loop (l, line_count)   line_properties[l] = new Plot_line_properties(l);
   line_properties[line_count] = 0;
   update(verbosity);
}
//-----------------------------------------------------------------------------
void
Plot_window_properties::set_window_size(int width, int height)
{
   // window_width and window_height are, despite of their names, the
   // size of the plot area (without borders).
   //
   pa_width  = width  - pa_border_L - pa_border_R;
   pa_height = height - pa_border_T - pa_border_B;
   update(0);
}
//-----------------------------------------------------------------------------

void
Plot_window_properties::update(int verbosity)
{
   window_width  = pa_border_L + pa_width  + pa_border_R;
   window_height = pa_border_T + pa_height + pa_border_B;

   min_X = plot_data.get_min_X();
   max_X = plot_data.get_max_X();
   min_Y = plot_data.get_min_Y();
   max_Y = plot_data.get_max_Y();

double delta_X = max_X - min_X;
double delta_Y = max_Y - min_Y;

   // first approximation for value → pixel factor
   //
   scale_X = pa_width  / delta_X;
   scale_Y = pa_height / delta_Y;

   verbosity > 1 &&
      CERR << setw(20) << "delta_X (1): " << delta_X << " pixels/X" << endl
           << setw(20) << "delta_Y (1): " << delta_Y << " pixels/Y" << endl
           << setw(20) << "scale_X (1): " << scale_X << " pixels/X" << endl
           << setw(20) << "scale_Y (1): " << scale_Y << " pixels/Y" << endl;

const double tile_X_raw = gridx_pixels / scale_X;
const double tile_Y_raw = gridy_pixels / scale_Y;

   verbosity > 1 &&
      CERR << setw(20) << "tile_X_raw: " << tile_X_raw << endl
           << setw(20) << "tile_Y_raw: " << tile_Y_raw << endl;

   tile_X = round_up_125(tile_X_raw);
   tile_Y = round_up_125(tile_Y_raw);

const int min_Xi = floor(min_X / tile_X);
const int min_Yi = floor(min_Y / tile_Y);
const int max_Xi = ceil(max_X / tile_X);
const int max_Yi = ceil(max_Y / tile_Y);
   gridx_last = 0.5 + max_Xi - min_Xi;
   gridy_last = 0.5 + max_Yi - min_Yi;

   min_X = tile_X * floor(min_X / tile_X);
   min_Y = tile_Y * floor(min_Y / tile_Y);
   max_X = tile_X * ceil(max_X  / tile_X);
   max_Y = tile_Y * ceil(max_Y  / tile_Y);

   delta_X = max_X - min_X;
   delta_X = max_Y - min_Y;

   // final value → pixel factor
   //
   scale_X = pa_width / delta_X;
   scale_Y = pa_height / delta_Y;

   verbosity > 1 &&
      CERR << setw(20) << "min_X (2): " << min_X << endl
           << setw(20) << "max_X (2): " << max_X << endl
           << setw(20) << "min_Y (2): " << min_Y << endl
           << setw(20) << "max_Y (2): " << max_Y << endl
           << setw(20) << "delta_X (2): " << delta_X << endl
           << setw(20) << "delta_Y (2): " << delta_Y << endl
           << setw(20) << "scale_X (2): " << scale_X << " pixels/X" << endl
           << setw(20) << "scale_Y (2): " << scale_Y << " pixels/Y" << endl;
}
//-----------------------------------------------------------------------------
int
Plot_window_properties::print(ostream & out) const
{
#define ldef(_ty, _na, _val, _descr)
#define gdef(ty,  na,  _val, _descr) \
   out << setw(20) << #na ":  " << Plot_data::ty ## _to_str(na) << endl;
#include "Quad_PLOT.def"

   for (Plot_line_properties ** lp = line_properties; *lp; ++lp)
       {
         out << endl;
         (*lp)->print(out);
       }

   return 0;
}
//-----------------------------------------------------------------------------
const char *
Plot_window_properties::set_attribute(const char * att_and_val)
{
   while (*att_and_val && (*att_and_val <= ' '))   ++ att_and_val;
const char * colon = strchr(att_and_val, ':');
   if (colon == 0)   return "Expecting 'attribute: value' but no : found";

const char * value = colon + 1;
   while (*value && (*value <= ' '))   ++value;

const char * minus = strchr(att_and_val, '-');
   if (minus && minus < colon)   // line attribute
      {
        const int line = strtol(minus + 1, 0, 10);
        if (line >= line_count)   return "invalid line number";

#define gdef(_ty, _na, _val, _descr)
#define ldef(ty,  na,  val, _descr)                                       \
         if (!strncmp(#na "-", att_and_val, minus - att_and_val))         \
            { const char * error = 0;                                     \
Q(line) \
              line_properties[line]->set_ ## na(Plot_data::ty ##          \
                                                _from_str(value, error)); \
              return error;                                               \
            }
#include "Quad_PLOT.def"
      }
   else                          // window attribute
      {
#define ldef(_ty, _na, _val, _descr)
#define gdef(ty,  na,  _val, _descr) \
         if (!strncmp(#na, att_and_val, colon - att_and_val))         \
            { const char * error = 0;                                 \
              set_ ## na(Plot_data::ty ## _from_str(value, error));   \
              return error;                                           \
            }
#include "Quad_PLOT.def"
      }

   return "Unknown attribute";
}
//-----------------------------------------------------------------------------
double
Plot_window_properties::round_up_125(double val)
{
int expo = 0;
   while (val >= 10)    { val /= 10;   ++expo; }
   while (val <  1)     { val *= 10;   --expo; }

   // at this point 1 ≤ val < 10.
   //
   if (expo > -18 && expo < 18)   // use integer arithmetic for 10^expo
      {
        const int abs_expo = expo < 0 ? -expo : expo;
        uint64_t expo_val = 1;
        loop(a, abs_expo)   expo_val *= 10;

        if (val <= 2.0)   return  expo < 0 ? 2.0/expo_val : 2.0*expo_val;
        if (val <= 5.0)   return  expo < 0 ? 5.0/expo_val : 5.0*expo_val;
        return  expo < 0 ? 10.0/expo_val : 10.0*expo_val;
      }
   else                           // use float arithmetic for 10^expo
      {
        const double expo_val = exp10(expo);
        if (val <= 2.0)   return  expo < 0 ? 2.0/expo_val : 2.0*expo_val;
        if (val <= 5.0)   return  expo < 0 ? 5.0/expo_val : 5.0*expo_val;
        return  expo < 0 ? 10.0/expo_val : 10.0*expo_val;
      }
}
//=============================================================================
void
testCookie(xcb_void_cookie_t cookie, xcb_connection_t * conn,
           const char * errMessage)
{
xcb_generic_error_t * error = xcb_request_check(conn, cookie);
   if (error)
      {
        CERR <<"ERROR: " << errMessage << " : " << error->error_code << endl;
        xcb_disconnect(conn);
        exit (-1);
      }
}
//-----------------------------------------------------------------------------
xcb_char2b_t *
build_chars(const char * str, size_t length)
{
xcb_char2b_t * ret = new xcb_char2b_t[length];
   if (!ret)   return 0;

   loop(l, length)
       {
         ret[l].byte1 = 0;
         ret[l].byte2 = str[l];
       }

  return ret;
}
//-----------------------------------------------------------------------------
struct string_width_height
{
   string_width_height(const char * string, xcb_connection_t * conn,
                       xcb_font_t font);
   int width;
   int height;
};

string_width_height::string_width_height(const char * string,
                                         xcb_connection_t * conn,
                                         xcb_font_t font)
{
xcb_char2b_t * xcb_str = build_chars(string, strlen(string));

xcb_query_text_extents_cookie_t cookie =
   xcb_query_text_extents(conn, font, strlen(string), xcb_str);

xcb_query_text_extents_reply_t * reply =
   xcb_query_text_extents_reply(conn, cookie, 0);

  if (!reply) puts("STRING ERROR");

  width  = reply->overall_width;
  height = reply->font_ascent + reply->font_descent;
  // origin = reply->font_ascent;

  delete xcb_str;
  free(reply);
}
//-----------------------------------------------------------------------------
xcb_gcontext_t
getFontGC(xcb_connection_t * conn, xcb_screen_t * screen, xcb_window_t window,
          const char * font_name, xcb_font_t font,
          xcb_void_cookie_t & fontCookie)
{
   /* get font */
   fontCookie = xcb_open_font_checked(conn, font, strlen (font_name),
                                      font_name);

   testCookie(fontCookie, conn, "can't open font");

   /* create graphics context */
xcb_gcontext_t gc = xcb_generate_id (conn);
uint32_t mask     = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
uint32_t values[3] = { screen->black_pixel,
                           screen->white_pixel,
                           font };

xcb_void_cookie_t gcCookie = xcb_create_gc_checked (conn,
                                                    gc,
                                                    window,
                                                    mask,
                                                    values);

   testCookie(gcCookie, conn, "can't create gc");
   return gc;
}
//-----------------------------------------------------------------------------
void
draw_text(xcb_connection_t * conn, xcb_gcontext_t text_gc,
          xcb_window_t window, int16_t x, int16_t y, const char * label)
{
/* draw the text */
xcb_void_cookie_t textCookie = xcb_image_text_8_checked(conn, strlen(label),
                                                        window, text_gc, x, y,
                                                        label);

   testCookie(textCookie, conn, "can't paste text");
}
//-----------------------------------------------------------------------------
void
draw_line(xcb_connection_t * conn, xcb_drawable_t window, xcb_gcontext_t gc,
           int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
const xcb_segment_t segment = { x0, y0, x1, y1 };
   xcb_poly_segment(conn, window, gc, 1, &segment);
}
//-----------------------------------------------------------------------------
void
draw_point(xcb_connection_t * conn, xcb_drawable_t window, xcb_gcontext_t gc,
           int16_t x, int16_t y, uint16_t point_size)
{
   x -= point_size >> 1;
   y -= point_size >> 1;

const xcb_arc_t arc = { x, y, point_size, point_size, 0, int16_t(360 << 6) };
   xcb_poly_fill_arc(conn, window, gc, 1, &arc);
}
//-----------------------------------------------------------------------------

#define ITEMS(x) sizeof(x)/sizeof(*x), x
void
do_plot(xcb_connection_t * conn, xcb_screen_t * screen, xcb_drawable_t window,
        xcb_gcontext_t gc, const Plot_window_properties & w_props,
        const Plot_data & data)
{
   // draw the background
   //
   {
     enum { mask = XCB_GC_FOREGROUND };
     const uint32_t value = w_props.get_canvas_color();
     xcb_change_gc(conn, gc, mask, &value);
     xcb_rectangle_t rect = { 0, 0, w_props.get_window_width(),
                                    w_props.get_window_height() };
     xcb_poly_fill_rectangle(conn, window, gc, 1, &rect);
   }

Plot_line_properties ** l_props = w_props.get_line_properties();

   // get graphics context...
   //
const xcb_font_t font = xcb_generate_id(conn);
xcb_void_cookie_t fontCookie;
xcb_gcontext_t text_gc = getFontGC(conn, screen, window, "fixed",
                                   font, fontCookie);

   // draw vertical (X-) grid lines...
   //
   {
     enum { mask = XCB_GC_FOREGROUND | XCB_GC_LINE_WIDTH };
     const uint32_t values[] = { w_props.get_gridX_color(),
                                 w_props.get_gridX_line_width() };
     xcb_change_gc(conn, gc, mask, values);

     const int py0 = w_props.valY2pixel(0);
     const double dy = w_props.get_max_Y() - w_props.get_min_Y();
     const int py1 = w_props.valY2pixel(dy);
     for (int ix = 0; ix <= w_props.get_gridx_last(); ++ix)
         {
           const double v = w_props.get_min_X() + ix*w_props.get_tile_X();
           const int px = w_props.valX2pixel(v - w_props.get_min_X());
           if (ix == 0 || ix == w_props.get_gridx_last() ||
               w_props.get_gridx_style() == 1)
              {
                draw_line(conn, window, gc, px, py0, px, py1);
              }
           else if (w_props.get_gridx_style() == 2)
              {
                draw_line(conn, window, gc, px, py0 - 5, px, py0 + 5);
              }

           char cc[40];
           snprintf(cc, sizeof(cc), "%.1lf", v);
           string_width_height wh(cc, conn, font);
           draw_text(conn, text_gc, window,
                     px - wh.width/2, py0 + wh.height + 3, cc);
         }
   }

   // draw horizontal (Y-) grid lines...
   //
   {
     enum { mask = XCB_GC_FOREGROUND | XCB_GC_LINE_WIDTH};
     const uint32_t values[] = { w_props.get_gridY_color(),
                                 w_props.get_gridY_line_width() };
     xcb_change_gc(conn, gc, mask, values);

     const int px0 = w_props.valX2pixel(0);
     const double dx = w_props.get_max_X() - w_props.get_min_X();
     const int px1 = w_props.valX2pixel(dx);
     for (int iy = 0; iy <= w_props.get_gridy_last(); ++iy)
         {
           const double v = w_props.get_min_Y() + iy*w_props.get_tile_Y();
           const int py = w_props.valY2pixel(v - w_props.get_min_Y());
           if (iy == 0 || iy == w_props.get_gridy_last() ||
               w_props.get_gridy_style() == 1)
              {
                draw_line(conn, window, gc, px0, py, px1, py);
              }
           else if (w_props.get_gridy_style() == 2)
              {
                draw_line(conn, window, gc, px0 - 5, py, px0 + 5, py);
              }

           char cc[40];
           snprintf(cc, sizeof(cc), "%.1lf", v);
           string_width_height wh(cc, conn, font);
           draw_text(conn, text_gc, window,
                     px0 - wh.width - 5, py + wh.height/2 - 1, cc);
         }
}

   // draw the legend
   //
   for (int l = 0; l_props[l]; ++l)
       {
         const Plot_line_properties & lp = *l_props[l];

         enum { mask = XCB_GC_FOREGROUND | XCB_GC_LINE_WIDTH };
         const uint32_t values[] = { lp.line_color, lp.line_width };
         xcb_change_gc(conn, gc, mask, values);

         const int x0 = w_props.get_legend_X();
         const int x1 = x0 + (w_props.get_legend_X() >> 1);
         const int x2 = x1 + (w_props.get_legend_X() >> 1);
         const int xt = x2 + 10;
         const int y  = w_props.get_legend_Y() + l*w_props.get_legend_dY();
         const Pixel point_size = l_props[l]->point_size;
         draw_line(conn, window, gc, x0, y, x2, y);
         draw_point(conn, window, gc, x1, y, point_size);
         draw_text(conn, text_gc, window, xt, y + 5, lp.legend_name.c_str());
       }

   // draw the plot line(s)...
   //
   loop(l, data.get_row_count())
       {
         enum { mask = XCB_GC_FOREGROUND
                     | XCB_GC_LINE_WIDTH
              };
         const uint32_t values[] = { l_props[l]->line_color,
                                     l_props[l]->line_width
                                   };
         xcb_change_gc(conn, gc, mask, values);

         int last_x = 0;
         int last_y = 0;
         loop(n, data.get_col_count())
             {
               double vx, vy;   data.get_XY(vx, vy, l, n);
               const int px = w_props.valX2pixel(vx - w_props.get_min_X());
               const int py = w_props.valY2pixel(vy - w_props.get_min_Y());
               if (n)   draw_line(conn, window, gc, last_x, last_y, px, py);
               last_x = px;
               last_y = py;
             }

         const Pixel point_size = l_props[l]->point_size;
         loop(n, data.get_col_count())
             {
               double vx, vy;   data.get_XY(vx, vy, l, n);
               const Pixel px = w_props.valX2pixel(vx - w_props.get_min_X());
               const Pixel py = w_props.valY2pixel(vy - w_props.get_min_Y());
               draw_point(conn, window, gc, px, py, point_size);
             }
       }

   // free the text_gc
   //
   testCookie(xcb_free_gc(conn, text_gc), conn, "can't free gc");

   // close font
   fontCookie = xcb_close_font_checked(conn, font);
   testCookie(fontCookie, conn, "can't close font");
}
//-----------------------------------------------------------------------------
xcb_atom_t
get_atom_ID(xcb_connection_t * conn, int only_existing, const char * name)
{
xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, only_existing,
                                                  strlen(name), name);
xcb_intern_atom_reply_t & reply = *xcb_intern_atom_reply(conn, cookie, 0);
   return reply.atom;
}
//-----------------------------------------------------------------------------
void *
plot_main(void * vp_props)
{
Plot_window_properties & w_props =
      *reinterpret_cast<Plot_window_properties *>(vp_props);

const Plot_data & data = w_props.get_plot_data();

   // open a connection to the X server
   //
xcb_connection_t * conn = xcb_connect(0, 0);

   // get the first screen
   //
const xcb_setup_t * setup = xcb_get_setup(conn);
xcb_screen_t * screen = xcb_setup_roots_iterator(setup).data;

   // create a graphic context
   //
xcb_gcontext_t gc = xcb_generate_id(conn);
   {
     enum { mask = XCB_GC_GRAPHICS_EXPOSURES };
//               | XCB_EVENT_MASK_STRUCTURE_NOTIFY };
     const uint32_t value = 0;

     xcb_create_gc(conn, gc, screen->root, mask, &value);
   }

   // create a window
   //
xcb_drawable_t window = xcb_generate_id(conn);
   {
     enum { mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK};
     const uint32_t values[] = { w_props.get_canvas_color(),
                                 XCB_EVENT_MASK_EXPOSURE
                               | XCB_EVENT_MASK_STRUCTURE_NOTIFY };
     xcb_create_window(conn,                            // connection
                       XCB_COPY_FROM_PARENT,            // depth
                       window,                          // window Id
                       screen->root,                    // parent window
                       50, 50,                          // position on screen
                       w_props.get_window_width(),
                       w_props.get_window_height(),
                       w_props.get_border_width(),
                       XCB_WINDOW_CLASS_INPUT_OUTPUT,   // class
                       screen->root_visual,             // visual
                       mask, values);                   // mask and values

     xcb_change_property(conn, XCB_PROP_MODE_REPLACE, window,
                       XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                       w_props.get_caption().size(),
                       w_props.get_caption().c_str());
   }

   // tell the window manager that we will handle WM_DELETE_WINDOW events...
   //
const xcb_atom_t window_deleted = get_atom_ID(conn, 0, "WM_DELETE_WINDOW");
const xcb_atom_t WM_protocols   = get_atom_ID(conn, 1, "WM_PROTOCOLS");
   xcb_change_property(conn, XCB_PROP_MODE_REPLACE, window, WM_protocols,
                       XCB_ATOM_ATOM, /* bits */ 32, 1, &window_deleted);

   // remember who has the focus before mapping a new window
   //
const xcb_get_input_focus_reply_t * focusReply =
   xcb_get_input_focus_reply(conn, xcb_get_input_focus(conn), 0);

   // map the window on the screen and flush
   //
   xcb_map_window(conn, window);
   xcb_flush(conn);

   // X main loop
   for (;;)
      {
        xcb_generic_event_t * event = xcb_poll_for_event(conn);
        if (event == 0)   // nothing happened
           {
             pthread_t thread = pthread_self();
             bool zombie = true;
             sem_wait(&Quad_PLOT::plot_threads_sema);
                loop(pt, Quad_PLOT::plot_threads.size())
                    if (Quad_PLOT::plot_threads[pt] == thread)
                       {
                         zombie = false;
                         break;
                       }
             sem_post(&Quad_PLOT::plot_threads_sema);

             if (zombie)
                {
                  free(event);
                  xcb_disconnect(conn);
                  delete &w_props;
                  return 0;
                }

             usleep(200000);
             continue;
           }

        switch(event->response_type & ~0x80)
           {
             case XCB_EXPOSE:             // 12
                  do_plot(conn, screen, window, gc, w_props, data);
                  if (focusReply)
                     {
                       /* this is the first XCB_EXPOSE event. X has moved
                          the focus way from our caller to the newly
                          created focus window (which is somehat annoying).

                          Return the focus to the window from which we have
                          stolen it.
                        */
                       xcb_set_input_focus(conn, XCB_INPUT_FOCUS_PARENT,
                                           focusReply->focus, XCB_CURRENT_TIME);
                       focusReply = 0;
                    }
                 xcb_flush(conn);
                 break;

             case XCB_UNMAP_NOTIFY:       // 18
             case XCB_MAP_NOTIFY:         // 19
             case XCB_REPARENT_NOTIFY:    // 21
                  break;

             case XCB_CONFIGURE_NOTIFY:   // 22
                  if (focusReply)   break;   // not yet exposed

                  {
                    const xcb_configure_notify_event_t * notify =
                          reinterpret_cast<const xcb_configure_notify_event_t *>
                             (event);
                    w_props.set_window_size(notify->width, notify->height);
                  }
             break;

             case XCB_CLIENT_MESSAGE:     // 33
                  if (reinterpret_cast<xcb_client_message_event_t *>(event)
                           ->data.data32[0] == window_deleted)
                     {
                       // CERR << "Killed!" << endl;
                       free(event);
                       xcb_disconnect(conn);
                       delete &w_props;
                       sem_wait(&Quad_PLOT::plot_threads_sema);
                          const int count = Quad_PLOT::plot_threads.size();
                          const pthread_t thread = pthread_self();
                          loop(pt, count)
                              if (Quad_PLOT::plot_threads[pt] == thread)
                                 {
                                   Quad_PLOT::plot_threads[pt] =
                                      Quad_PLOT::plot_threads[count - 1];
                                   Quad_PLOT::plot_threads.pop();
                                   break;
                                 }
                       sem_post(&Quad_PLOT::plot_threads_sema);
                       return 0;
                     }
                  break;

             default:
                w_props.get_verbosity() > 0 &&
                   CERR << "unexpected event type "
                        << static_cast<unsigned int>(event->response_type)
                         << " (ignored)" << endl;
           }

        free(event);
      }

   CERR << "plot_main() done!" << endl;
   return 0;
}
//=============================================================================
Token
Quad_PLOT::eval_AB(Value_P A, Value_P B)
{
   if (B->get_rank() < 1 || B->get_rank() > 2)   RANK_ERROR;
   if (B->element_count() < 2)                   LENGTH_ERROR;

   // plot window with default attrinutes
   //
Plot_data * data = setup_data(B.get());
   if (data == 0)   DOMAIN_ERROR;

Plot_window_properties * w_props = new Plot_window_properties(data, verbosity);
   if (w_props == 0)
      {
        delete data;
         WS_FULL;
      }

   if (A->get_rank() > 1)   RANK_ERROR;

const ShapeItem len_A = A->element_count();
   if (len_A < 1)   LENGTH_ERROR;

const APL_Integer qio = Workspace::get_IO();

   loop(a, len_A)
       {
         const Cell & cell_A = A->get_ravel(a);
         if (!cell_A.is_pointer_cell())
            {
               MORE_ERROR() << "A[" << (a + qio)
                            << "] is not a string in A ⎕PLOT B";
               DOMAIN_ERROR;
            }

         const Value * attr = cell_A.get_pointer_value().get();
         if (!attr->is_char_string())
            {
               MORE_ERROR() << "A[" << (a + qio)
                            << "] is not a string in A ⎕PLOT B";
               DOMAIN_ERROR;
            }

         const UCS_string ucs = attr->get_UCS_ravel();
         UTF8_string utf(ucs);
         const char * error = w_props->set_attribute(utf.c_str());
         if (error)
            {
              MORE_ERROR() << error << " in string '" << ucs << "'";
              DOMAIN_ERROR;
            }
       }

   w_props->update(verbosity);
   return Token(TOK_APL_VALUE1, plot_data(w_props, data));
}
//-----------------------------------------------------------------------------
Token
Quad_PLOT::eval_B(Value_P B)
{
   if (B->get_rank() == 0)   // create a zombie by removing it from plot_threads
      {
        APL_Integer B0 = B->get_ravel(0).get_int_value();
        if (B0 <= 0 && B0 >= -2)   // verbose mode
           {
             verbosity = -B0;
             CERR << "⎕PLOT verbosity set to " << verbosity << endl;
             return Token(TOK_APL_VALUE1, Idx0(LOC));
           }

        bool found = false;
        sem_wait(&plot_threads_sema);
           loop(pt, Quad_PLOT::plot_threads.size())
               {
                 if (Quad_PLOT::plot_threads[pt] != B0)   continue;

                 plot_threads[pt] = plot_threads[plot_threads.size() - 1];
                 plot_threads.pop();
                 found = true;
                 break;
               }
         sem_post(&plot_threads_sema);
        return Token(TOK_APL_VALUE1, IntScalar(found ? B0 : 0, LOC));
      }

   if (B->get_rank() == 1 && B->element_count() == 0)
      {
        help();
        return Token(TOK_APL_VALUE1, Idx0(LOC));
      }

   if (B->get_rank() < 1 || B->get_rank() > 2)   RANK_ERROR;
   if (B->element_count() < 2)                   LENGTH_ERROR;

   // plot window with default attrinutes
   //
Plot_data * data = setup_data(B.get());
   if (data == 0)   DOMAIN_ERROR;

Plot_window_properties * w_props = new Plot_window_properties(data, verbosity);
   if (w_props == 0)
      {
        delete data;
         WS_FULL;
      }

// w_props->update(verbosity);
   return Token(TOK_APL_VALUE1, plot_data(w_props, data));
}
//-----------------------------------------------------------------------------
Plot_data *
Quad_PLOT::setup_data(const Value * B)
{
const ShapeItem cols_B = B->get_cols();
const ShapeItem rows_B = B->get_rows();
const ShapeItem len_B = rows_B * cols_B;

   // check data
   //
   loop(b, len_B)   if (!B->get_ravel(b).is_numeric())   return 0;

const APL_Integer qio = Workspace::get_IO();

double * X = new double[2*len_B];
   if (!X)   WS_FULL;
double * Y = X + len_B;

   loop(b, len_B)
       {
         const Cell & cB = B->get_ravel(b);
         if (cB.is_complex_cell())
            {
              X[b] = cB.get_real_value();
              Y[b] = cB.get_imag_value();
            }
         else
            {
              X[b] = qio + b % cols_B;
              Y[b] = cB.get_real_value();
            }
       }

Plot_data * data = new Plot_data(rows_B, cols_B);
   loop(r, rows_B)
       {
         Plot_data_row * data_r = new Plot_data_row(X + r*cols_B,
                                                    Y + r*cols_B, r, cols_B);

         data->add_row(data_r);
       }

   return data;
}
//-----------------------------------------------------------------------------
Value_P
Quad_PLOT::plot_data(Plot_window_properties * w_props,
                     const Plot_data * data)
{
   w_props->set_verbosity(verbosity);
   verbosity > 0 && w_props->print(CERR);

pthread_t thread;
   pthread_create(&thread, 0, plot_main, w_props);
   sem_wait(&Quad_PLOT::plot_threads_sema);
      plot_threads.append(thread);
   sem_post(&Quad_PLOT::plot_threads_sema);
   return IntScalar(thread, LOC);
}
//-----------------------------------------------------------------------------
void
Quad_PLOT::help() const
{
   CERR <<
"\n"
"   ⎕PLOT Usage:\n"
"\n"
"   ⎕PLOT B     plot B with default attribute values\n"
"   A ⎕PLOT B   plot B with attributes A\n"
"\n"
"   A is a nested vector of strings.\n"
"   Each string has the form \"Attribute: Value\"\n"
"   Colors are specified either as #RGB or as #RRGGBB or as RR GG BB)\n"
"   The attributes understood and their defaults are:\n"
"\n"
"   1. Global (plot window) Attributes:\n"
"\n";

   CERR << left;

#define ldef(_ty, _na, _val, _descr)
#define gdef(ty,  na,  val, descr)           \
   CERR << setw(20) << #na ":  " << setw(14) \
        << Plot_data::ty ## _to_str(val) << " (" << #descr << ")" << endl;
#include "Quad_PLOT.def"

   CERR <<
"\n"
"   2. Local (plot line N) Attributes:\n"
"\n";

#define gdef(_ty, _na, _val, _descr)
#define ldef(ty,  na,  val, descr)             \
   CERR << setw(20) << #na "-N:  " << setw(14) \
        << Plot_data::ty ## _to_str(val) << " (" << #descr << ")" << endl;
#include "Quad_PLOT.def"

   CERR << right;
}
//-----------------------------------------------------------------------------

#else // no libxce...

//-----------------------------------------------------------------------------
Token
Quad_PLOT::eval_B(Value_P B)
{
    MORE_ERROR() <<
"⎕PLOT is not available because either no libcxb library was found on this\n"
"system when GNU APL was compiled, or because it was disabled in ./configure.";

   SYNTAX_ERROR;
   return Token();
}
//-----------------------------------------------------------------------------
Token
Quad_PLOT::eval_AB(Value_P A, Value_P B)
{
    MORE_ERROR() <<
"⎕PLOT is not available because either no libcxb library was found on this\n"
"system when GNU APL was compiled, or because it was disabled in ./configure.";

   SYNTAX_ERROR;
   return Token();
}
//-----------------------------------------------------------------------------

#endif // HAVE_PLOTW3_H

