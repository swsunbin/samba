/*
   Unix SMB/CIFS implementation.
   Samba utility functions

   Copyright (C) 2009 Jelmer Vernooij <jelmer@samba.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _ACE_H_
#define _ACE_H_

#include "librpc/gen_ndr/security.h"
#include "librpc/ndr/libndr.h"

bool sec_ace_object(uint8_t type);
size_t ndr_subcontext_size_of_ace_coda(const struct security_ace *ace, size_t ace_size, libndr_flags flags);
bool sec_ace_callback(uint8_t type);
bool sec_ace_resource(uint8_t type);
bool sec_ace_has_extra_blob(uint8_t type);

void init_sec_ace(struct security_ace *t, const struct dom_sid *sid, enum security_ace_type type,
		  uint32_t mask, uint8_t flag);
int nt_ace_inherit_comp( const struct security_ace *a1, const struct security_ace *a2);
int nt_ace_canon_comp( const struct security_ace *a1, const struct security_ace *a2);
void dacl_sort_into_canonical_order(struct security_ace *srclist, unsigned int num_aces);

#endif /*_ACE_H_*/

