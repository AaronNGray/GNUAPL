
/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2018-2022  Dr. Jürgen Sauermann

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

/** @file **/

#include <stdio.h>
#include <zlib.h>
#include <png.h>

#include "Common.hh"
#include "Quad_PNG.hh"

Quad_PNG  Quad_PNG::_fun;
Quad_PNG * Quad_PNG::fun = &Quad_PNG::_fun;

sem_t __PNG_threads_sema;
sem_t * Quad_PNG::PNG_threads_sema = &__PNG_threads_sema;

sem_t __PNG_window_sema;
sem_t * Quad_PNG::PNG_window_sema = &__PNG_window_sema;

static vector<pthread_t> plot_threads;

/// ⎕PNG verbosity bitmap
enum
{
   SHOW_NONE   = 0,   /// show nothing
   SHOW_EVENTS = 1,   ///< show X events
   SHOW_DATA   = 2,   ///< show APL data
   SHOW_DRAW   = 4,   ///< show draw details
   SHOW_ALL    = SHOW_EVENTS | SHOW_DATA | SHOW_DRAW
};
int verbosity = SHOW_NONE;

#if HAVE_GTK3 && defined( HAVE_LIBGTK_3 ) && defined(HAVE_LIBZ)

#include <X11/Xlib.h>
# include <gtk/gtk.h>

/// the number of open plot windows (to see when the last one was closed).
static int plot_window_count = 0;

/// the main() program of the thread that handles one PNG display window.
extern void * plot_PNG_main(void * vp_props);

struct PNG_context
{
   /// constructor
   PNG_context(Value_P B)
   : handle(++next_handle),
     window(0),
     drawing_area(0),
     APL_value(B)
   {}

   /// return the window width
   ShapeItem get_total_width() const
      { return APL_value->get_shape_item(2); }

   /// return the window height
   ShapeItem get_total_height() const
      { return APL_value->get_shape_item(1); }

   Value_P get_APL_value() const
      { return APL_value; }

   void PNG_stop()
      {}

   /// the handle for identifying this PNG_context in APL
   const int handle;

   /// the top-level window of this PNG_context
   GtkWidget * window;

   /// the drawing area in the window of this PNG_context
   GtkWidget * drawing_area;

   /// the RGB matrix to be displayed
   Value_P APL_value;

   static int next_handle;
};

int PNG_context::next_handle = 0;

/// all PNG_contexts (= all open windows) for ⎕PNG
static vector<PNG_context *> all_PNG_contexts;
//-----------------------------------------------------------------------------
Quad_PNG::Quad_PNG()
  : QuadFunction(TOK_Quad_PNG)
{
   __sem_init(PNG_threads_sema,  /* threads */ 0, /* initial */ 1);
   __sem_init(PNG_window_sema, /* processes */ 1, /* initial */ 0);
}
//----------------------------------------------------------------------------
Quad_PNG::~Quad_PNG()
{
   __sem_destroy(PNG_threads_sema);
   __sem_destroy(PNG_window_sema);
}
//---------------------------------------------------------------------------
static void
PNG_warn(png_structp png_ptr, png_const_charp reason)
{
   CERR << "*** libpng warning: " << reason << " ***" << endl;
}
//---------------------------------------------------------------------------
static void
PNG_err(png_structp png_ptr, png_const_charp reason)
{
   MORE_ERROR() << "libpng error: " << reason;
   DOMAIN_ERROR;
}
//-----------------------------------------------------------------------------
Value_P
Quad_PNG::read_PNG_file(const UTF8_string & filename1)
{
   // filename1 is the file to be read. It may or may not have a .png extension
   // If opening filename1 fails we try filename2 = filename1.png
   //
int errno1 = 0;
FILE * in = fopen(filename1.c_str(), "rb");
   if (in == 0 && !filename1.ends_with(".png"))
      {
         errno1 = errno;              // remember why filename1 has failed
         UTF8_string filename2(filename1);
         filename2.append_ASCII(".png");
         in = fopen(filename2.c_str(), "r");
      }

   if (in == 0)   // neither filename nor filename.png
      {
        MORE_ERROR() << "Cannot open file '" << filename1.c_str()
                     << "' in ⎕PNG B: " << strerror(errno1);
        DOMAIN_ERROR;
      }

png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                             0, PNG_err, PNG_warn);
   if (!png_ptr)   WS_FULL;

png_infop info_ptr = png_create_info_struct(png_ptr);
   if (!info_ptr)   WS_FULL;

    png_init_io(png_ptr, in);

   // We use the low-level interface for a better control of the input
   // transformations...

   // 1. read the image parameters.
   //
   png_read_info(png_ptr, info_ptr);

   // primary image parameters (from the PNG file read)
   //
const int width = png_get_image_width(png_ptr, info_ptr);
const int height = png_get_image_height(png_ptr, info_ptr);
const int color_type = png_get_color_type(png_ptr, info_ptr);
const int bit_depth = png_get_bit_depth(png_ptr, info_ptr);

const bool alpha_used   = color_type & 0x04;
const bool color_used   = color_type & 0x02;
const bool palette_used = color_type & 0x01;
const int planes = (alpha_used ? 1 : 0) + (color_used ? 3 : 1);

   // secondary image parameters (derived from the primary parameters)
   //
const int bytes_per_color = (bit_depth + 7) / 8;       // 1 or 2 bytes

const int bytes_per_pixel = planes * bytes_per_color;

   if (verbosity & SHOW_DATA)
      {
        CERR << "width:         " << width  << " pixels"           << endl
             << "height:        " << height << " pixels"           << endl
             << "color_type:    " << color_type                    << endl
             << " ├─── alpha:   " << (alpha_used ? "yes" : "no")   << endl
             << " ├─── color:   " << (color_used ? "yes" : "no")   << endl
             << " └─── palette: " << (palette_used ? "yes" : "no") << endl
             << "bit_depth:     " << bit_depth << " bits/color"    << endl
             << "planes:        " << planes                        << endl;
      }

   // 2. allocate the pixel memory and scanline pointers...
   //
UTF8 * RGB = new UTF8[planes*height*width*2];
UTF8 ** row_pointers = new UTF8 *[height];

UTF8 * scanline = RGB;
   loop(y, height)
       {
         row_pointers[y] = scanline;
         scanline += planes*width*2;
       }

   // 3. set the desired input transformations...
   //
   if (color_type == PNG_COLOR_TYPE_PALETTE)   // color palettes → RGB
      png_set_palette_to_rgb(png_ptr);

   if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)   // bits → byte
      png_set_expand_gray_1_2_4_to_8(png_ptr);

   if (png_get_valid(png_ptr, info_ptr,
      PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_ptr);

   // update the info_ptr according to the desired transformations.
   png_read_update_info(png_ptr, info_ptr);

   png_read_image(png_ptr, row_pointers);

   png_read_end(png_ptr, info_ptr);

Shape shape_Z(/* BW, BW⍺, RGB, or RGB⍺ */ planes, height, width);
Value_P Z(shape_Z, LOC);

   // due to png_set_expand_gray_1_2_4_to_8() above, packed 1-bit, 2-bit,
   // and 4-bit pixels were expanded to one byte per color value. The only
   // question remaining is therefore whether the colors are 8-bit or 16-bit.
   //
   // Like GTK, GNU APL uses a normalized double (0.0-1.0) for colors

   loop(rgb, planes)      // loop over 1, 2, 3, or 4 planes
   loop(y, height)        // loop over scanlines
       {
         const UTF8 * scanline = row_pointers[y];
         loop(x, width)   // loop over the pixels on the scanlines
             {
               const UTF8 * pixel = scanline + x*bytes_per_pixel;
               const UTF8 * color = pixel + rgb*bytes_per_color;
               if (bit_depth == 16)
                  {
                    const uint16_t value = color[0] << 8 | color[1];
                    Z->next_ravel_Float(int(value) / 65535.0);
                  }
               else if (bit_depth == 8)
                  {
                    Z->next_ravel_Float(int(color[0]) / 255.0);
                  }
               else if (bit_depth == 4)
                  {
                    Z->next_ravel_Float(int(color[0]) / 15.0);
                  }
               else if (bit_depth == 2)
                  {
                    Z->next_ravel_Float(int(color[0]) / 3.0);
                  }
               else if (bit_depth == 1)
                  {
                    Z->next_ravel_Float(color[0] ? 1.0 : 0.0);
                  }
               else
                  {
                    MORE_ERROR() << "Illegal bitdepth " << bit_depth
                                 << " in ⎕PNG B (file: " << filename1 << ")";
                    DOMAIN_ERROR;
                  }
             }
       }

   png_destroy_read_struct(&png_ptr, &info_ptr, 0);

   loop(y, height)   row_pointers[y] = 0;   // just in case
   delete [] row_pointers;
   delete [] RGB;

   Z->check_value(LOC);
   return Z;
}
//-----------------------------------------------------------------------------
Token
Quad_PNG::eval_B(Value_P B) const
{
   if (B->get_rank() == 0)   // scalar (integer) argument: plot window control
      {
        const APL_Integer B0 = B->get_cscalar().get_int_value();
        Value_P Z = window_control(B0);
        return Token(TOK_APL_VALUE1, Idx0_0(LOC));

        DOMAIN_ERROR;
      }

   if (B->is_apl_char_vector())   // read PNG file
      {
        const UCS_string filename_ucs(*B);
        UTF8_string filename_utf(filename_ucs);
        Value_P Z = read_PNG_file(filename_utf);
        return Token(TOK_APL_VALUE1, Z);
      }

   if (B->get_rank() == 3)   // display B (an RGB or RGBA matrix)
      {
        const APL_Integer handle = display_PNG_main(B);
        sem_wait(PNG_threads_sema);
        sem_post(PNG_threads_sema);

        sem_wait(PNG_window_sema);   // blocks until window shown

        return Token(TOK_APL_VALUE1, IntScalar(handle, LOC));
      }

   MORE_ERROR() << "Bad B in ⎕PNG B";
   VALENCE_ERROR;
}
//-----------------------------------------------------------------------------
Value_P
Quad_PNG::window_control(APL_Integer B0) const
{
   if (B0 == 0)                 // reset plot verbosity
       {
         verbosity = 0;
         CERR << "⎕PNG verbosity turned off" << endl;
         return Idx0_0(LOC);
       }

    if (B0 == -1)                // enable SHOW_EVENTS
       {
         verbosity |= SHOW_EVENTS;
         CERR << "⎕PNG will show X events " << endl;
         return Idx0_0(LOC);
       }

    if (B0 == -2)                // enable SHOW_DATA
       {
         verbosity |= SHOW_DATA;
         CERR << "⎕PNG will  show APL data " << endl;
         return Idx0_0(LOC);
       }

    if (B0 == -3)   // close all ⎕PNG windows
       {
         Value_P Z(all_PNG_contexts.size(), LOC);
         loop(p, all_PNG_contexts.size())
            {
              PNG_context * pctx = all_PNG_contexts[p];
              Z->next_ravel_Int(pctx->handle);

              gtk_window_close(GTK_WINDOW(pctx->window));
            }
         all_PNG_contexts.clear();
         Z->check_value(LOC);
         return Z;
       }

    if (B0 == -4)                // enable SHOW_DRAW
       {
         verbosity |= SHOW_DRAW;
         CERR << "⎕PNG will show rendering details " << endl;
         return Idx0_0(LOC);
       }

    // otherwise B0 is supposed to be a ⎕PNG window handle. Find it and
    // close (only) that window.

    loop(p, all_PNG_contexts.size())
        {
          PNG_context * pctx = all_PNG_contexts[p];
          if (B0 == pctx->handle)   // found
             {
               all_PNG_contexts[p] = all_PNG_contexts.back();
               all_PNG_contexts.pop_back();
               gtk_window_close(GTK_WINDOW(pctx->window));
               return IntScalar(B0, LOC);
             }

        }

    return Idx0_0(LOC);   // not found: return empty list of handles
}
//---------------------------------------------------------------------------
bool
Quad_PNG::valid_type_and_bits(int color_type, int bit_depth)
{
       //        11111110000000000
       //        65432109876543210
enum { valid = 0b10000000100010110 };  // 16, 8, 4, 2, and 1
       //        └───────┴───┴─┴┴──────────┴──┴──┴──┴──────┘

   switch(color_type)
      {
        case PNG_COLOR_TYPE_GRAY:              // type 0: 1, 2, 4, 8, or 16
             return valid & 1 << bit_depth;

        case PNG_COLOR_TYPE_RGB:                // type 2: 8 or 16
        case PNG_COLOR_TYPE_GRAY_ALPHA:         // type 4: 8 or 16
        case PNG_COLOR_TYPE_RGB_ALPHA:          // type 6: 8 or 16
             return bit_depth == 8 || bit_depth == 16;

        default: return false;
      }
}
//---------------------------------------------------------------------------
void
Quad_PNG::write_PNG_file(const char * filename, int bit_depth,
                         const Value & B)
{
const ShapeItem planes = B.get_shape_item(0);
const ShapeItem height = B.get_shape_item(1);
const ShapeItem width  = B.get_shape_item(2);
int color_type;
   if      (planes == 1)   color_type = PNG_COLOR_TYPE_GRAY;
   else if (planes == 2)   color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
   else if (planes == 3)   color_type = PNG_COLOR_TYPE_RGB;
   else if (planes == 4)   color_type = PNG_COLOR_TYPE_RGB_ALPHA;
   else
      {
        MORE_ERROR() << "Bad number of planes (" << planes
                     << ") in B of A ⎕PNG B";
        DOMAIN_ERROR;
      }

const bool palette_used = color_type & PNG_COLOR_MASK_PALETTE;   // 0x01
const bool color_used   = color_type & PNG_COLOR_MASK_COLOR;     // 0x02
const bool alpha_used   = color_type & PNG_COLOR_MASK_ALPHA;     // 0x04
   if (!valid_type_and_bits(color_type, bit_depth))
      {
        MORE_ERROR() << "Bad/unsupported combination of color type "
                     << color_type << " and bit depth " << bit_depth
                     << " in A ⎕PNG B";
        DOMAIN_ERROR;
      }

FILE * out = fopen(filename, "wb");
   if (out == 0)
       {
          MORE_ERROR() << "Cannot open PNG output file " << filename
                       << " in A ⎕PNG B: " << strerror(errno);
          DOMAIN_ERROR;
       }

png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                              0, PNG_err, PNG_warn);
   if (!png_ptr)   WS_FULL;

png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)   WS_FULL;

   png_init_io(png_ptr, out);

   // add a Software keyword indicating GNU APL's ⎕PNG as image producer
   //
png_text text_ptr[1];
   memset(text_ptr, 0, sizeof(text_ptr));
   text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;
   text_ptr[0].key  = const_cast<char *>("Software");
   text_ptr[0].text = const_cast<char *>("GNU APL, ⎕PNG");
   text_ptr[0].text_length = strlen(text_ptr[0].text);
   text_ptr[0].lang = const_cast<char *>("EN");

   png_set_text(png_ptr, info_ptr, text_ptr, 1);

   if (verbosity & SHOW_DATA)
      {
        CERR << "width:         " << width  << " pixels"           << endl
             << "height:        " << height << " pixels"           << endl
             << "color_type:    " << color_type                    << endl
             << " ├─── alpha:   " << (alpha_used ? "yes" : "no")   << endl
             << " ├─── color:   " << (color_used ? "yes" : "no")   << endl
             << " └─── palette: " << (palette_used ? "yes" : "no") << endl
             << "bit_depth:     " << bit_depth << " bits/color"    << endl
             << "planes:        " << planes                        << endl;
      }

UTF8 * RGB = new UTF8[2*B.element_count()];
UTF8 ** row_pointers = new UTF8 *[height];

UTF8 * scanline = RGB;
   loop(y, height)
       {
         row_pointers[y] = scanline;
         loop(x, width)
         loop(c, planes)
             {
               const ShapeItem APL_offset = x + (y + c*height)*width;
               const double val = B.get_cravel(APL_offset).get_real_value();
               if (val < 0.0)
                  {
                    MORE_ERROR() << "negative color component " << val
                                 << " in A ⎕PNG B";
                    DOMAIN_ERROR;
                  }

               if (val > 1.0)
                  {
                    MORE_ERROR() << " color component " << val
                                 << " too large in A ⎕PNG B";
                    DOMAIN_ERROR;
                  }

               if (bit_depth == 16)
                  {
                    const uint16_t word = val * 65535.5;
                    *scanline++ = word >> 8;
                    *scanline++ = word;
                  }
               else if (bit_depth == 8)
                  {
                    const uint8_t byte = val * 255.5;
                    *scanline++ = byte;
                  }
               else if (bit_depth == 4)
                  {
                    const uint8_t byte = 0x0F & uint8_t(val * 15.5);
                    if ((x & 1) == 0)   *scanline    = byte << 4;
                    else                *scanline++ |= byte;
                  }
               else if (bit_depth == 2)
                  {
                    const uint8_t byte = 0x03 & uint8_t(val * 3.5);
                    if      ((x & 3) == 0)   *scanline    = byte << 6;
                    else if ((x & 3) == 1)   *scanline   |= byte << 4;
                    else if ((x & 3) == 2)   *scanline   |= byte << 2;
                    else                     *scanline++ |= byte << 0;
                  }
               else if (bit_depth == 1)
                  {
                    const uint8_t byte = 0x01 & uint8_t(val * 1.5);
                    const int bit = x & 7;   // from the left
                    if      (bit == 0)   *scanline    = byte << 7;
                    else if (bit == 7)   *scanline++ |= byte << 0;
                    else                     *scanline   |= byte << (7 - bit);
                  }
               else
                  {
                    MORE_ERROR() << "Illegal bitdepth " << bit_depth
                                 << " in ⎕PNG B (file: " << filename << ")";
                    DOMAIN_ERROR;
                  }
             }
       }

   png_set_IHDR(png_ptr, info_ptr, width, height,
                bit_depth, color_type, PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
   png_set_rows(png_ptr, info_ptr, row_pointers);

   png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, 0);
   png_write_image(png_ptr, row_pointers);
   png_write_end(png_ptr, info_ptr);

   loop(y, height)   row_pointers[y] = 0;   // just in case
   delete [] row_pointers;
   delete [] RGB;

   png_destroy_write_struct(&png_ptr, &info_ptr);
   fclose(out);
}
//-----------------------------------------------------------------------------
Token
Quad_PNG::eval_AB(Value_P A, Value_P B) const
{
   // write pixels B to file A. A is either
   //
   // 1. a string for the filename, or
   // 2. a 2-element vector (nested filename, bit-depth)
   //
   if (A->is_apl_char_vector())   // case 1: write PNG-file A depth 8
      {
        if (B->get_rank() != 3)   RANK_ERROR;

        UCS_string filename_ucs(*A);
        UTF8_string filename_utf8(filename_ucs);
        write_PNG_file(filename_utf8.c_str(), 8, *B);

        return Token(TOK_APL_VALUE1, Idx0_0(LOC));
      }
   else if (A->element_count() == 2)   // case 2: PNG file A[1] depth A[2]
      {
        const APL_Integer A1 = A->get_cravel(1).get_int_value();   // bit depth
        if (A->get_cravel(0).is_pointer_cell())   // probably file name
           {
             const Value_P A0 = A->get_cravel(0).get_pointer_value();
             UCS_string filename_ucs(*A0);
             UTF8_string filename_utf8(filename_ucs);
             write_PNG_file(filename_utf8.c_str(), A1, *B);

        return Token(TOK_APL_VALUE1, Idx0_0(LOC));
           }
        else
           {
             MORE_ERROR() << "A1 is not a string (filename) in A1 A2 ⎕PNG B";
             DOMAIN_ERROR;
           }
      }
   else   // A is not a file name
      {
        MORE_ERROR() << "A is not a filename in A ⎕PNG B";
        LENGTH_ERROR;
      }
}
//-----------------------------------------------------------------------------
/// make gtk_main() suitable for pthread_create() and maybe tell when it is
/// finished. Executed in the thread named apl/⎕PNG.
static void *
gtk_main_wrapper(void * w_props)
{
   gtk_main();

   if (verbosity & SHOW_EVENTS)   CERR << "gtk_main() thread done" << endl;

   if (--plot_window_count == 0)   // last window closed
      {
        while (all_PNG_contexts.size())
           {
             all_PNG_contexts.pop_back();
           }
      }

   return 0;
}
//-----------------------------------------------------------------------------
/// callback when the plot window is destroyed
extern "C" gboolean
PNG_plot_destroyed(GtkWidget * top_level);

gboolean
PNG_plot_destroyed(GtkWidget * top_level)
{
   /* top_level is the window that was destroyed. Remove it from
      all_PNG_contexts to avoid 'invalid unclassed pointer in cast to
      'GtkWindow' assertions.

      Case 1: the window was closed from APL. Then PNG_stop() has already
              removed the window from all_PNG_contexts and we will not find it
              below.

      Case 2: the user has closed the window via the GUI. Then PNG_stop() was
              not yet called. We will then find top_level in all_PNG_contexts
              and remove it.
    */

   for (size_t th = 0; th < all_PNG_contexts.size(); ++th)
       {
         const PNG_context * pctx = all_PNG_contexts[th];
         if (top_level == pctx->window)   // case 2 (closed by user click)
            {
              all_PNG_contexts[th] = all_PNG_contexts.back();
              all_PNG_contexts.pop_back();
              if (verbosity & SHOW_EVENTS)
                 CERR << "PNG window DESTROYED (by user/GUI)" << endl;
              return true;
            }
       }

   // case 1 (closed from APL)

   if (verbosity & SHOW_EVENTS)
      CERR << "PNG window DESTROYED (from APL)" << endl;

  return TRUE;   // event handled by this handler
}
//-----------------------------------------------------------------------------
/// convert APL pixel data to a Gtk/Cairo surface
static cairo_surface_t *
paint_data(const PNG_context & pctx)
{
   /* the APL data B is organized in color planes, i.e. B[1;;] for red,
      B[2;;] for green, B[3;;] for blue, and optionally B[4;;] for alpha.
      In contrast, the surface bytes are interleaved with 4 consecutive
      bytes for each pixel with one byte per color component (or alpha).
    */

const Value * B = pctx.get_APL_value().get();
const ShapeItem width  = B->get_shape_item(2);
const ShapeItem height = B->get_shape_item(1);
const bool has_colors  = B->get_shape_item(0) > 2;   // RGB or RGBA
const bool has_alpha   = B->get_shape_item(0) > (has_colors ? 3 : 1);
const ShapeItem plane  = width*height;

cairo_surface_t * ret =
   cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    Assert(cairo_surface_status(ret) == CAIRO_STATUS_SUCCESS);

    cairo_surface_flush(ret);   // switch to direct access of the surface

unsigned char * current_row = cairo_image_surface_get_data(ret);
const int stride = cairo_image_surface_get_stride(ret);   // = row size (bytes)

   loop(y, height)
       {
         uint32_t * row = reinterpret_cast<uint32_t *>(current_row);
         loop(x, width)
             {
               const ShapeItem xy0 = x + width*y;   // red or grey
               const ShapeItem xy1 = xy0 + plane;   // green or grey ⍺
               const ShapeItem xy2 = xy1 + plane;   // blue
               const ShapeItem xy3 = xy2 + plane;   // ⍺

               // the GNU APL color values are 0.0 - 1.0
               double red = B->get_cravel(xy0).get_real_value();
               double green = red;
               double blue  = red;
               double alpha = 1.0;

               if (has_colors)   // RGB
                  {
                    green = B->get_cravel(xy1).get_real_value();
                    blue  = B->get_cravel(xy2).get_real_value();
                    if (has_alpha) alpha = B->get_cravel(xy3).get_real_value();
                  }
               else              // grey
                  {
                    if (has_alpha) alpha = B->get_cravel(xy1).get_real_value();
                  }

               /* row[x] is a pixel which, according to enum cairo_format_t:

                  each pixel is a 32-bit quantity, with alpha in the upper 8
                  bits, then red, then green, then blue. The 32-bit quantities
                  are stored native-endian. Pre-multiplied alpha is used.
                  (That is, 50% transparent red is 0x80800000, not 0x80ff0000)
                */
               row[x] = uint8_t(alpha*255.5)  << 24
                      | uint8_t(red   *255.5) << 16
                      | uint8_t(green *255.5) << 8
                      | uint8_t(blue  *255.5);
             }
         current_row += stride;
       }

   cairo_surface_mark_dirty(ret);   // end of direct access (commit caches)
   return ret;
}
//-----------------------------------------------------------------------------
extern "C" gboolean
PNG_draw_callback(GtkWidget * drawing_area, cairo_t * cr, gpointer user_data);

gboolean
PNG_draw_callback(GtkWidget * drawing_area, cairo_t * cr, gpointer user_data)
{
   // callback from GTK.

const int new_width  = gtk_widget_get_allocated_width(drawing_area);
const int new_height = gtk_widget_get_allocated_height(drawing_area);
   if (verbosity & SHOW_EVENTS)
      CERR << "PNG_draw_callback(drawing_area = " << drawing_area << ")  "
           << "width: " << new_width << ", height: " << new_height << endl;

   // find the PNG_context for this event...
   //
PNG_context * pctx = 0;
   for (size_t th = 0; th < all_PNG_contexts.size(); ++th)
       {
           if (all_PNG_contexts[th]->drawing_area == drawing_area)
              {
                pctx = all_PNG_contexts[th];
                break;
              }
       }

   if (pctx == 0)
      {
        CERR << "*** Could not find thread handling drawing_area "
             << reinterpret_cast<void *>(drawing_area) << endl;
        return false;
      }

cairo_surface_t * source_surfrace = paint_data(*pctx);

   // copy pctx->surface to the cr of the caller
   //
   cairo_set_source_surface(cr, source_surfrace, 0, 0);
   cairo_paint(cr);

   cairo_surface_destroy(source_surfrace);

   if (verbosity & SHOW_EVENTS)   CERR << "PNG_draw_callback() done." << endl;
   return TRUE;   // event handled by this handler
}
//-----------------------------------------------------------------------------
APL_Integer
Quad_PNG::display_PNG_main(Value_P B)
{
   CERR << "display_PNG_main()" << endl;

   if (getenv("DISPLAY") == 0)   // DISPLAY not set
      setenv("DISPLAY", ":0", true);

   XInitThreads();

   // we need one thread for processing all GTK events...
   //
   if (!gtk_init_done)
      {
        int argc = 0;              // no arguments for gtk_init()
        gtk_init(&argc, 0);
        pthread_t thread = 0;
        pthread_create(&thread, 0, gtk_main_wrapper, 0);
#if HAVE_PTHREAD_SETNAME_NP
         // show with e.g.   ps H -o 'pid tid cmd comm'
         pthread_setname_np(thread, "apl/⎕PNG");
#endif
        gtk_init_done = true;
      }

PNG_context * pctx = new PNG_context(B);
   Assert(pctx);
   all_PNG_contexts.push_back(pctx);
   ++plot_window_count;

   pctx->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   Assert(pctx->window);
   {
     const int height = B->get_shape_item(1);
     const int width  = B->get_shape_item(2);
     char cc[50];
     if (width < 500)   // small
        snprintf(cc, sizeof(cc), "⎕PNG");
     else
        snprintf(cc, sizeof(cc), "⎕PNG %d×%d", height, width);
     gtk_window_set_title(GTK_WINDOW(pctx->window), cc);
      }
   gtk_window_set_resizable(GTK_WINDOW(pctx->window), false);
   gtk_window_set_type_hint(GTK_WINDOW(pctx->window),
                            GDK_WINDOW_TYPE_HINT_DIALOG);

   pctx->drawing_area = gtk_drawing_area_new();
   gtk_container_add(GTK_CONTAINER(pctx->window), pctx->drawing_area);

   // resize the drawing_area BEFORE showing it so that PNG_draw_callback()
   // won't be called twice.
   //
   gtk_widget_set_size_request(GTK_WIDGET(pctx->drawing_area),
                                          pctx->get_total_width(),
                                          pctx->get_total_height());

   gtk_window_move(GTK_WINDOW(pctx->window), 100*(all_PNG_contexts.size()+1),
                                             100*(all_PNG_contexts.size()+1));


   g_signal_connect_object(pctx->window, "destroy",
                           G_CALLBACK(PNG_plot_destroyed),0, G_CONNECT_AFTER);

   g_signal_connect_object(pctx->drawing_area, "draw",
                           G_CALLBACK(PNG_draw_callback), 0, G_CONNECT_AFTER);

   gtk_widget_show_all(pctx->window);

   sem_post(Quad_PNG::PNG_window_sema);   // unleash the APL interpreter
   return pctx->handle;
}
//-----------------------------------------------------------------------------

#else // no GTK or no libz...

//----------------------------------------------------------------------------
Quad_PNG::Quad_PNG()
  : QuadFunction(TOK_Quad_PNG)
{
   verbosity = SHOW_NONE;
   if (verbosity)  {}   // prevent unused variable warning
}
//----------------------------------------------------------------------------
Quad_PNG::~Quad_PNG()
{
}
//-----------------------------------------------------------------------------
Token
Quad_PNG::eval_B(Value_P B) const
{
    MORE_ERROR() <<
"⎕PNG is not available because either no GTK3 libraries were found on this\n"
"system when GNU APL was compiled, or because GTK was disabled in ./configure.";

   SYNTAX_ERROR;
   return Token();
}
//-----------------------------------------------------------------------------
Token
Quad_PNG::eval_AB(Value_P A, Value_P B) const
{
    MORE_ERROR() <<
"⎕PNG is not available because either no libfftw3 library was found on this\n"
"system when GNU APL was compiled, or because it was disabled in ./configure.";

   SYNTAX_ERROR;
   return Token();
}
//-----------------------------------------------------------------------------

#endif // HAVE_PNG_H

