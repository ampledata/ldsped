/*
 *  aprs.h - agwpe linux replacement
 *
 *  Copyright (C) 2009, Lieven De Samblanx, ON7LDS. All rights reserved
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef APRS_HEADERS
#define APRS_HEADERS 1


#define STATIONTEXT "ldsped V%s, the agwpe compatible daemon for linux (www.on7lds.net)"

char station_text[70];



void check_APRS_message(char *, char *);
void check_childs();


#endif
