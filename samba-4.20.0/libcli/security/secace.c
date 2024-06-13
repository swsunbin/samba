/*
 *  Unix SMB/Netbios implementation.
 *  struct security_ace handling functions
 *  Copyright (C) Andrew Tridgell              1992-1998,
 *  Copyright (C) Jeremy R. Allison            1995-2003.
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1998,
 *  Copyright (C) Paul Ashton                  1997-1998.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "replace.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "libcli/security/security.h"
#include "lib/util/tsort.h"

/**
 * Check if ACE has OBJECT type.
 */
bool sec_ace_object(uint8_t type)
{
	if (type == SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT ||
	    type == SEC_ACE_TYPE_ACCESS_DENIED_OBJECT ||
	    type == SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT ||
	    type == SEC_ACE_TYPE_SYSTEM_ALARM_OBJECT ||
	    type == SEC_ACE_TYPE_ACCESS_ALLOWED_CALLBACK_OBJECT ||
	    type == SEC_ACE_TYPE_ACCESS_DENIED_CALLBACK_OBJECT ||
	    type == SEC_ACE_TYPE_SYSTEM_AUDIT_CALLBACK_OBJECT) {
		/*
		 * MS-DTYP has a reserved value for
		 * SEC_ACE_TYPE_SYSTEM_ALARM_CALLBACK_OBJECT, but we
		 * don't assume that it will be an object ACE just
		 * because it sounds like one.
		 */
		return true;
	}
	return false;
}

/**
 * Check if ACE is a CALLBACK type, which means it will have a blob of data at
 * the end.
 */
bool sec_ace_callback(uint8_t type)
{
	if (type == SEC_ACE_TYPE_ACCESS_ALLOWED_CALLBACK ||
	    type == SEC_ACE_TYPE_ACCESS_DENIED_CALLBACK ||
	    type == SEC_ACE_TYPE_ACCESS_ALLOWED_CALLBACK_OBJECT ||
	    type == SEC_ACE_TYPE_ACCESS_DENIED_CALLBACK_OBJECT ||
	    type == SEC_ACE_TYPE_SYSTEM_AUDIT_CALLBACK ||
	    type == SEC_ACE_TYPE_SYSTEM_AUDIT_CALLBACK_OBJECT) {
	    /*
	     * While SEC_ACE_TYPE_SYSTEM_ALARM_CALLBACK and
	     * SEC_ACE_TYPE_SYSTEM_ALARM_CALLBACK_OBJECT sound like
	     * callback types, they are reserved values in MS-DTYP,
	     * and their eventual use is not defined.
	     */
		return true;
	}
	return false;
}

/**
 * Check if an ACE type is resource attribute, which means it will
 * have a blob of data at the end defining an attribute on the object.
 * Resource attribute ACEs should only occur in SACLs.
 */
bool sec_ace_resource(uint8_t type)
{
	return type == SEC_ACE_TYPE_SYSTEM_RESOURCE_ATTRIBUTE;
}

bool sec_ace_has_extra_blob(uint8_t type)
{
	return sec_ace_callback(type) || sec_ace_resource(type);
}


/*******************************************************************
 Sets up a struct security_ace structure.
********************************************************************/

void init_sec_ace(struct security_ace *t, const struct dom_sid *sid, enum security_ace_type type,
		  uint32_t mask, uint8_t flag)
{
	t->type = type;
	t->flags = flag;
	t->size = ndr_size_dom_sid(sid, 0) + 8;
	t->access_mask = mask;

	t->trustee = *sid;
	t->coda.ignored.data = NULL;
	t->coda.ignored.length = 0;
}

int nt_ace_inherit_comp(const struct security_ace *a1, const struct security_ace *a2)
{
	int a1_inh = a1->flags & SEC_ACE_FLAG_INHERITED_ACE;
	int a2_inh = a2->flags & SEC_ACE_FLAG_INHERITED_ACE;

	if (a1_inh == a2_inh)
		return 0;

	if (!a1_inh && a2_inh)
		return -1;
	return 1;
}

/*******************************************************************
  Comparison function to apply the order explained below in a group.
*******************************************************************/

int nt_ace_canon_comp( const struct security_ace *a1,  const struct security_ace *a2)
{
	if ((a1->type == SEC_ACE_TYPE_ACCESS_DENIED) &&
				(a2->type != SEC_ACE_TYPE_ACCESS_DENIED))
		return -1;

	if ((a2->type == SEC_ACE_TYPE_ACCESS_DENIED) &&
				(a1->type != SEC_ACE_TYPE_ACCESS_DENIED))
		return 1;

	/* Both access denied or access allowed. */

	/* 1. ACEs that apply to the object itself */

	if (!(a1->flags & SEC_ACE_FLAG_INHERIT_ONLY) &&
			(a2->flags & SEC_ACE_FLAG_INHERIT_ONLY))
		return -1;
	else if (!(a2->flags & SEC_ACE_FLAG_INHERIT_ONLY) &&
			(a1->flags & SEC_ACE_FLAG_INHERIT_ONLY))
		return 1;

	/* 2. ACEs that apply to a subobject of the object, such as
	 * a property set or property. */

	if (a1->flags & (SEC_ACE_FLAG_CONTAINER_INHERIT|SEC_ACE_FLAG_OBJECT_INHERIT) &&
			!(a2->flags & (SEC_ACE_FLAG_CONTAINER_INHERIT|SEC_ACE_FLAG_OBJECT_INHERIT)))
		return -1;
	else if (a2->flags & (SEC_ACE_FLAG_CONTAINER_INHERIT|SEC_ACE_FLAG_OBJECT_INHERIT) &&
			!(a1->flags & (SEC_ACE_FLAG_CONTAINER_INHERIT|SEC_ACE_FLAG_OBJECT_INHERIT)))
		return 1;

	return 0;
}

/*******************************************************************
 Functions to convert a SEC_DESC ACE DACL list into canonical order.
 JRA.

--- from http://msdn.microsoft.com/library/default.asp?url=/library/en-us/security/security/order_of_aces_in_a_dacl.asp

The following describes the preferred order:

 To ensure that noninherited ACEs have precedence over inherited ACEs,
 place all noninherited ACEs in a group before any inherited ACEs.
 This ordering ensures, for example, that a noninherited access-denied ACE
 is enforced regardless of any inherited ACE that allows access.

 Within the groups of noninherited ACEs and inherited ACEs, order ACEs according to ACE type, as the following shows:
	1. Access-denied ACEs that apply to the object itself
	2. Access-denied ACEs that apply to a subobject of the object, such as a property set or property
	3. Access-allowed ACEs that apply to the object itself
	4. Access-allowed ACEs that apply to a subobject of the object"

********************************************************************/

void dacl_sort_into_canonical_order(struct security_ace *srclist, unsigned int num_aces)
{
	unsigned int i;

	if (!srclist || num_aces == 0)
		return;

	/* Sort so that non-inherited ACE's come first. */
	TYPESAFE_QSORT(srclist, num_aces, nt_ace_inherit_comp);

	/* Find the boundary between non-inherited ACEs. */
	for (i = 0; i < num_aces; i++ ) {
		struct security_ace *curr_ace = &srclist[i];

		if (curr_ace->flags & SEC_ACE_FLAG_INHERITED_ACE)
			break;
	}

	/* i now points at entry number of the first inherited ACE. */

	/* Sort the non-inherited ACEs. */
	TYPESAFE_QSORT(srclist, i, nt_ace_canon_comp);

	/* Now sort the inherited ACEs. */
	TYPESAFE_QSORT(&srclist[i], num_aces - i, nt_ace_canon_comp);
}
