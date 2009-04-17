/* LiquidRescaling Library
 * Copyright (C) 2007-2009 Carlo Baldassi (the "Author") <carlobaldassi@gmail.com>.
 * All Rights Reserved.
 *
 * This library implements the algorithm described in the paper
 * "Seam Carving for Content-Aware Image Resizing"
 * by Shai Avidan and Ariel Shamir
 * which can be found at http://www.faculty.idc.ac.il/arik/imret.pdf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3 dated June, 2007.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/> 
 */

#include <glib.h>
#include <math.h>
#include <lqr/lqr_base.h>
#include <lqr/lqr_gradient.h>
#include <lqr/lqr_energy.h>
#include <lqr/lqr_progress_pub.h>
#include <lqr/lqr_cursor_pub.h>
#include <lqr/lqr_vmap.h>
#include <lqr/lqr_vmap_list.h>
#include <lqr/lqr_carver_list.h>
#include <lqr/lqr_carver.h>

#ifdef __LQR_DEBUG__
#include <stdio.h>
#include <assert.h>
#endif /* __LQR_DEBUG__ */


/* read normalised pixel value from
 * rgb buffer at the given index */
gdouble
lqr_pixel_get_norm (void * rgb, gint rgb_ind, LqrColDepth col_depth) 
{
  switch (col_depth)
    {
      case LQR_COLDEPTH_8I:
        return (gdouble) AS_8I(rgb)[rgb_ind] / 0xFF;
      case LQR_COLDEPTH_16I:
        return (gdouble) AS_16I(rgb)[rgb_ind] / 0xFFFF;
      case LQR_COLDEPTH_32F:
        return (gdouble) AS_32F(rgb)[rgb_ind];
      case LQR_COLDEPTH_64F:
        return (gdouble) AS_64F(rgb)[rgb_ind];
      default:
#ifdef __LQR_DEBUG__
        assert(0);
#endif /* __LQR_DEBUG__ */
        return 0;
    }
}

inline gdouble
lqr_pixel_get_rgbcol (void *rgb, gint rgb_ind, LqrColDepth col_depth, LqrImageType image_type, gint channel)
{
  gdouble black_fact = 0;

  switch (image_type)
    {
      case LQR_RGB_IMAGE: 
      case LQR_RGBA_IMAGE: 
        return lqr_pixel_get_norm (rgb, rgb_ind + channel, col_depth);
      case LQR_CMY_IMAGE: 
        return 1. - lqr_pixel_get_norm (rgb, rgb_ind + channel, col_depth);
      case LQR_CMYK_IMAGE: 
      case LQR_CMYKA_IMAGE: 
        black_fact = 1 - lqr_pixel_get_norm(rgb, rgb_ind + 3, col_depth);
        return black_fact * (1. - (lqr_pixel_get_norm (rgb, rgb_ind + channel, col_depth)));
      default:
#ifdef __LQR_DEBUG__
        assert(0);
#endif /* __LQR_DEBUG__ */
        return 0;
    }
}

inline gdouble
lqr_carver_read_brightness_grey (LqrCarver * r, gint x, gint y)
{
  gint now = r->raw[y][x];
  gint rgb_ind = now * r->channels;
  return lqr_pixel_get_norm (r->rgb, rgb_ind, r->col_depth);
}

inline gdouble
lqr_carver_read_brightness_std (LqrCarver * r, gint x, gint y)
{
  gdouble red, green, blue;
  gint now = r->raw[y][x];
  gint rgb_ind = now * r->channels;

  red = lqr_pixel_get_rgbcol (r->rgb, rgb_ind, r->col_depth, r->image_type, 0);
  green = lqr_pixel_get_rgbcol (r->rgb, rgb_ind, r->col_depth, r->image_type, 1);
  blue = lqr_pixel_get_rgbcol (r->rgb, rgb_ind, r->col_depth, r->image_type, 2);
  return (red + green + blue) / 3;
}

gdouble
lqr_carver_read_brightness_custom (LqrCarver * r, gint x, gint y)
{
  gdouble sum = 0;
  gint k;
  gchar has_alpha = (r->alpha_channel >= 0 ? 1 : 0);
  gchar has_black = (r->black_channel >= 0 ? 1 : 0);
  guint col_channels = r->channels - has_alpha - has_black;

  gdouble black_fact = 0;

  gint now = r->raw[y][x];

  if (has_black)
    {
      black_fact = lqr_pixel_get_norm(r->rgb, now * r->channels + r->black_channel, r->col_depth);
    }

  for (k = 0; k < r->channels; k++) if ((k != r->alpha_channel) && (k != r->black_channel))
    {
      gdouble col = lqr_pixel_get_norm(r->rgb, now * r->channels + k, r->col_depth);
      sum += 1. - (1. - col) * (1. - black_fact);
    }

  sum /= col_channels;

  if (has_black)
    {
      sum = 1 - sum;
    }

  return sum;
}

/* read average pixel value at x, y 
 * for energy computation */
gdouble
lqr_carver_read_brightness (LqrCarver * r, gint x, gint y)
{
  gchar has_alpha = (r->alpha_channel >= 0 ? 1 : 0);
  gdouble alpha_fact = 1;

  gint now = r->raw[y][x];

  gdouble bright = 0;

  switch (r->image_type)
    {
      case LQR_GREY_IMAGE:
      case LQR_GREYA_IMAGE:
        bright = lqr_carver_read_brightness_grey (r, x, y);
        break;
      case LQR_RGB_IMAGE: 
      case LQR_RGBA_IMAGE: 
      case LQR_CMY_IMAGE: 
      case LQR_CMYK_IMAGE: 
      case LQR_CMYKA_IMAGE: 
        bright = lqr_carver_read_brightness_std (r, x, y);
        break;
      case LQR_CUSTOM_IMAGE:
        bright = lqr_carver_read_brightness_custom (r, x, y);
        break;
    }

  if (has_alpha)
    {
      alpha_fact = lqr_pixel_get_norm(r->rgb, now * r->channels + r->alpha_channel, r->col_depth);
    }

  return bright * alpha_fact;
}

inline gdouble
lqr_carver_read_luma_std (LqrCarver * r, gint x, gint y)
{
  gdouble red, green, blue;
  gint now = r->raw[y][x];
  gint rgb_ind = now * r->channels;

  red = lqr_pixel_get_rgbcol (r->rgb, rgb_ind, r->col_depth, r->image_type, 0);
  green = lqr_pixel_get_rgbcol (r->rgb, rgb_ind, r->col_depth, r->image_type, 1);
  blue = lqr_pixel_get_rgbcol (r->rgb, rgb_ind, r->col_depth, r->image_type, 2);
  return 0.2126 * red + 0.7152 * green + 0.0722 * blue;
}

gdouble
lqr_carver_read_luma (LqrCarver * r, gint x, gint y)
{
  gchar has_alpha = (r->alpha_channel >= 0 ? 1 : 0);
  gdouble alpha_fact = 1;

  gint now = r->raw[y][x];

  gdouble bright = 0;

  switch (r->image_type)
    {
      case LQR_GREY_IMAGE:
      case LQR_GREYA_IMAGE:
        bright = lqr_carver_read_brightness_grey (r, x, y);
        break;
      case LQR_RGB_IMAGE: 
      case LQR_RGBA_IMAGE: 
      case LQR_CMY_IMAGE: 
      case LQR_CMYK_IMAGE: 
      case LQR_CMYKA_IMAGE: 
        bright = lqr_carver_read_luma_std (r, x, y);
        break;
      case LQR_CUSTOM_IMAGE:
        bright = lqr_carver_read_brightness_custom (r, x, y);
        break;
    }

  if (has_alpha)
    {
      alpha_fact = lqr_pixel_get_norm(r->rgb, now * r->channels + r->alpha_channel, r->col_depth);
    }

  return bright * alpha_fact;
}



#if 0
/* read average pixel value at x, y 
 * for energy computation */
inline gdouble
lqr_carver_read_brightness (LqrCarver * r, gint x, gint y)
{
  gdouble sum = 0;
  gint k;
  gchar has_alpha = (r->alpha_channel >= 0 ? 1 : 0);
  gchar has_black = (r->black_channel >= 0 ? 1 : 0);
  guint col_channels = r->channels - has_alpha - has_black;

  gdouble alpha_fact = 1;
  gdouble black_fact = 0;

  gint now = r->raw[y][x];

  if (has_alpha)
    {
      alpha_fact = lqr_pixel_get_norm(r->rgb, now * r->channels + r->alpha_channel, r->col_depth);
    }

  if (has_black)
    {
      black_fact = lqr_pixel_get_norm(r->rgb, now * r->channels + r->black_channel, r->col_depth);
    }

  for (k = 0; k < r->channels; k++) if ((k != r->alpha_channel) && (k != r->black_channel))
    {
      gdouble col = lqr_pixel_get_norm(r->rgb, now * r->channels + k, r->col_depth);
      sum += 1. - (1. - col) * (1. - black_fact);
    }

  sum /= col_channels;

  switch (r->image_type)
    {
      case LQR_RGB_IMAGE:
      case LQR_RGBA_IMAGE:
      case LQR_GREY_IMAGE:
      case LQR_GREYA_IMAGE:
        break;
      case LQR_CMY_IMAGE:
      case LQR_CMYK_IMAGE:
      case LQR_CMYKA_IMAGE:
        sum = 1 - sum;
        break;
      case LQR_CUSTOM_IMAGE:
        if (has_black)
          {
            sum = 1 - sum;
          }
#ifdef __LQR_DEBUG__
      default:
        assert (0);
#endif /* __LQR_DEBUG__ */
    }

  if (has_alpha)
    {
      sum *= alpha_fact;
    }

  return sum;
}
#endif

#if 0
inline gdouble
lqr_carver_read_brightness_abs (LqrCarver * r, gint x1, gint y1, gint x2, gint y2)
{
  gchar has_alpha = (r->alpha_channel > 0 ? 1 : 0);
  gint p1, p2;
  gdouble a1, a2;
  gint k;
  gdouble sum = 0;
  p1 = r->raw[y1][x1];
  p2 = r->raw[y2][x2];
  if (has_alpha)
    {
      a1 = R_RGB(r->rgb, p1 * r->channels + r->alpha_channel) / R_RGB_MAX;
      a2 = R_RGB(r->rgb, p2 * r->channels + r->alpha_channel) / R_RGB_MAX;
    }
  else
    {
      a1 = a2 = 1;
    }
  for (k = 0; k < r->channels; k++) if (k != r->alpha_channel)
    {
      sum += fabs(R_RGB(r->rgb, p1 * r->channels + 0) * a1 - R_RGB(r->rgb, p2 * r->channels + 0) * a2);
    }
  return sum / (R_RGB_MAX * (r->channels - has_alpha));
}

inline gdouble
lqr_carver_read_luma_abs (LqrCarver * r, gint x1, gint y1, gint x2, gint y2)
{
  gint p1, p2;
  gdouble a1, a2;
  p1 = r->raw[y1][x1];
  p2 = r->raw[y2][x2];
  if (r->image_type == LQR_RGBA_IMAGE)
    {
      a1 = R_RGB(r->rgb, p1 * r->channels + 3) / R_RGB_MAX;
      a2 = R_RGB(r->rgb, p2 * r->channels + 3) / R_RGB_MAX;
    }
  else
    {
      a1 = a2 = 1;
    }
  return (0.2126 * fabs(R_RGB(r->rgb, p1 * r->channels + 0) * a1 - R_RGB(r->rgb, p2 * r->channels + 0) * a2) +
          0.7152 * fabs(R_RGB(r->rgb, p1 * r->channels + 1) * a1 - R_RGB(r->rgb, p2 * r->channels + 1) * a2) +
          0.0722 * fabs(R_RGB(r->rgb, p1 * r->channels + 2) * a1 - R_RGB(r->rgb, p2 * r->channels + 2) * a2)) /
          R_RGB_MAX;
}
#endif


/* compute energy at x, y */
double
lqr_energy_std (LqrCarver * r, gint x, gint y)
{
  gdouble gx, gy;

  if (y == 0)
    {
      gy = (*(r->nrg->rf)) (r, x, y + 1) - (*(r->nrg->rf)) (r, x, y);
    }
  else if (y < r->h - 1)
    {
      gy =
        ((*(r->nrg->rf)) (r, x, y + 1) - (*(r->nrg->rf)) (r, x, y - 1)) / 2;
    }
  else
    {
      gy = (*(r->nrg->rf)) (r, x, y) - (*(r->nrg->rf)) (r, x, y - 1);
    }

  if (x == 0)
    {
      gx = (*(r->nrg->rf)) (r, x + 1, y) - (*(r->nrg->rf)) (r, x, y);
    }
  else if (x < r->w - 1)
    {
      gx =
        ((*(r->nrg->rf)) (r, x + 1, y) - (*(r->nrg->rf)) (r, x - 1, y)) / 2;
    }
  else
    {
      gx = (*(r->nrg->rf)) (r, x, y) - (*(r->nrg->rf)) (r, x - 1, y);
    }

  return (*(r->nrg->gf))(gx, gy);
}

double
lqr_energy_null (LqrCarver * r, gint x, gint y)
{
  return 0;
}

#if 0
/* compute energy at x, y */
double
lqr_energy_abs (LqrCarver * r, gint x, gint y)
{
  gdouble gx, gy;

  if (y == 0)
    {
      gy = (*(r->nrg->rfabs))(r, x, y + 1, x, y);
    }
  else if (y < r->h - 1)
    {
      gy = 0.5 * (*(r->nrg->rfabs))(r, x, y + 1, x, y - 1);
    }
  else
    {
      gy = (*(r->nrg->rfabs))(r, x, y, x, y - 1);
    }

  if (x == 0)
    {
      gx = (*(r->nrg->rfabs))(r, x + 1, y, x, y);
    }
  else if (x < r->w - 1)
    {
      gx = 0.5 * (*(r->nrg->rfabs))(r, x + 1, y, x - 1, y);
    }
  else
    {
      gx = (*(r->nrg->rfabs))(r, x, y, x - 1, y);
    }
  return (*(r->nrg->gf))(gx, gy);
}
#endif

/* gradient function for energy computation */
LQR_PUBLIC
LqrRetVal
lqr_carver_set_energy_function (LqrCarver * r, LqrEnergyFuncType ef_ind)
{
  switch (ef_ind)
    {
      case LQR_EF_GRAD_NORM:
        r->nrg->ef = &lqr_energy_std;
        r->nrg->rf = &lqr_carver_read_brightness;
        r->nrg->gf = &lqr_grad_norm;
        break;
      case LQR_EF_GRAD_SUMABS:
        r->nrg->ef = &lqr_energy_std;
        r->nrg->rf = &lqr_carver_read_brightness;
        r->nrg->gf = &lqr_grad_sumabs;
        break;
      case LQR_EF_GRAD_XABS:
        r->nrg->ef = &lqr_energy_std;
        r->nrg->rf = &lqr_carver_read_brightness;
        r->nrg->gf = &lqr_grad_xabs;
        break;
      case LQR_EF_LUMA_GRAD_NORM:
        r->nrg->ef = &lqr_energy_std;
        r->nrg->rf = &lqr_carver_read_luma;
        r->nrg->gf = &lqr_grad_norm;
        break;
      case LQR_EF_LUMA_GRAD_SUMABS:
        r->nrg->ef = &lqr_energy_std;
        r->nrg->rf = &lqr_carver_read_luma;
        r->nrg->gf = &lqr_grad_sumabs;
        break;
      case LQR_EF_LUMA_GRAD_XABS:
        r->nrg->ef = &lqr_energy_std;
        r->nrg->rf = &lqr_carver_read_luma;
        r->nrg->gf = &lqr_grad_xabs;
        break;
      case LQR_EF_NULL:
        r->nrg->ef = &lqr_energy_null;
        return LQR_OK;
      default:
        return LQR_ERROR;
    }

  return LQR_OK;
}
