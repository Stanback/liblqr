/* LiquidRescaling Library
 * Copyright (C) 2007 Carlo Baldassi (the "Author") <carlobaldassi@yahoo.it>.
 * All Rights Reserved.
 *
 * This library implements the algorithm described in the paper
 * "Seam Carving for Content-Aware Image Resizing"
 * by Shai Avidan and Ariel Shamir
 * which can be found at http://www.faculty.idc.ac.il/arik/imret.pdf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3 dated June, 2007.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/> 
 */

#ifndef __LQR_CARVER_RIGMASK_PUB_H__
#define __LQR_CARVER_RIGMASK_PUB_H__

#ifndef __LQR_BASE_H__
#error "lqr_base.h must be included prior to lqr_carver_rigmask_pub.h"
#endif /* __LQR_BASE_H__ */

/* PUBLIC RIGMASK-RELATED FUNCTIONS */

LqrRetVal lqr_carver_rigmask_add_rgb_area(LqrCarver *r, guchar *buffer, gint bpp, gint width, gint height, gint x_off, gint y_off);
LqrRetVal lqr_carver_rigmask_add_rgb(LqrCarver *r, guchar *buffer, gint bpp);
LqrRetVal lqr_carver_rigmask_add_area(LqrCarver *r, gdouble *buffer, gint width, gint height, gint x_off, gint y_off);
LqrRetVal lqr_carver_rigmask_add(LqrCarver *r, gdouble *buffer);


#endif /* __LQR_CARVER_RIGMASK_PUB_H__ */