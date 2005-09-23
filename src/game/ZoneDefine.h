/* ZoneDefine.h
 *
 * Copyright (C) 2005 MaNGOS <https://opensvn.csie.org/traccgi/MaNGOS/trac.cgi/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MANGOS_ZONEDEFINE_H
#define MANGOS_ZONEDEFINE_H

/*
 * Perphaps this is more quicker than loading in at run time
 */

#include "Common.h"

enum zone_t
    {
	ZONE_UNKNOWN = 0,
	ZONE_141 = 141,
	ZONE_12 = 12,
	ZONE_406 = 406,
	ZONE_15 = 15
    };

union
{
    uint32 uint32Value;
    float float32Value;
} ufields;

template<zone_t T> 
struct ZoneDefinition
{
};

template<> struct ZoneDefinition<ZONE_406>
{
    enum { y2=1162534229, y1=3301748735, x2=1161185962, x1=3282684586 };
};

template<> struct ZoneDefinition<ZONE_15>
{
    enum { y2=3295920127, y1=3317860352, x2=3304991402, x1=3316443818 };
};

#endif
