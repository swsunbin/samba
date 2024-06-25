/*
 * Unix SMB/CIFS implementation.
 * Util functions valid in the SMB1 server
 *
 * Copyright (C) Volker Lendecke 2019
 * Copyright by the authors of the functions moved here eventually
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "libcli/security/security.h"
#include "lib/util/sys_rw_data.h"
#include "smbd/fd_handle.h"

/****************************************************************************
 Special FCB or DOS processing in the case of a sharing violation.
 Try and find a duplicated file handle.
****************************************************************************/

struct files_struct *fcb_or_dos_open(
	struct smb_request *req,
	const struct smb_filename *smb_fname,
	uint32_t access_mask,
	uint32_t create_options,
	uint32_t private_flags)
{
	struct connection_struct *conn = req->conn;
	struct file_id id = vfs_file_id_from_sbuf(conn, &smb_fname->st);
	struct files_struct *fsp = NULL, *new_fsp = NULL;
	NTSTATUS status;

	if ((private_flags &
	     (NTCREATEX_FLAG_DENY_DOS|
	      NTCREATEX_FLAG_DENY_FCB))
	    == 0) {
		return NULL;
	}

	for(fsp = file_find_di_first(conn->sconn, id, true);
	    fsp != NULL;
	    fsp = file_find_di_next(fsp, true)) {

		DBG_DEBUG("Checking file %s, fd = %d, vuid = %"PRIu64", "
			  "file_pid = %"PRIu16", "
			  "private_options = 0x%"PRIx32", "
			  "access_mask = 0x%"PRIx32"\n",
			  fsp_str_dbg(fsp),
			  fsp_get_pathref_fd(fsp),
			  fsp->vuid,
			  fsp->file_pid,
			  fh_get_private_options(fsp->fh),
			  fsp->access_mask);

		if (fsp_get_pathref_fd(fsp) != -1 &&
		    fsp->vuid == req->vuid &&
		    fsp->file_pid == req->smbpid &&
		    (fh_get_private_options(fsp->fh) &
		     (NTCREATEX_FLAG_DENY_DOS |
		      NTCREATEX_FLAG_DENY_FCB)) &&
		    (fsp->access_mask & FILE_WRITE_DATA) &&
		    strequal(fsp->fsp_name->base_name, smb_fname->base_name) &&
		    strequal(fsp->fsp_name->stream_name,
			     smb_fname->stream_name)) {
			DBG_DEBUG("file match\n");
			break;
		}
	}

	if (fsp == NULL) {
		return NULL;
	}

	/* quite an insane set of semantics ... */
	if (is_executable(smb_fname->base_name) &&
	    (fh_get_private_options(fsp->fh) & NTCREATEX_FLAG_DENY_DOS)) {
		DBG_DEBUG("file fail due to is_executable.\n");
		return NULL;
	}

	status = file_new(req, conn, &new_fsp);
	if (!NT_STATUS_IS_OK(status)) {
		DBG_DEBUG("file_new failed: %s\n", nt_errstr(status));
		return NULL;
	}

	status = dup_file_fsp(fsp, access_mask, new_fsp);

	if (!NT_STATUS_IS_OK(status)) {
		DBG_DEBUG("dup_file_fsp failed: %s\n", nt_errstr(status));
		file_free(req, new_fsp);
		return NULL;
	}

	return new_fsp;
}

/****************************************************************************
 Send a keepalive packet (rfc1002).
****************************************************************************/

bool send_keepalive(int client)
{
	unsigned char buf[4];

	buf[0] = NBSSkeepalive;
	buf[1] = buf[2] = buf[3] = 0;

	return(write_data(client,(char *)buf,4) == 4);
}

/*******************************************************************
 Add a string to the end of a smb_buf, adjusting bcc and smb_len.
 Return the bytes added
********************************************************************/

ssize_t message_push_string(uint8_t **outbuf, const char *str, int flags)
{
	size_t buf_size = smb_len(*outbuf) + 4;
	size_t grow_size;
	size_t result = 0;
	uint8_t *tmp;
	NTSTATUS status;

	/*
	 * We need to over-allocate, now knowing what srvstr_push will
	 * actually use. This is very generous by incorporating potential
	 * padding, the terminating 0 and at most 4 chars per UTF-16 code
	 * point.
	 */
	grow_size = (strlen(str) + 2) * 4;

	if (!(tmp = talloc_realloc(NULL, *outbuf, uint8_t,
					 buf_size + grow_size))) {
		DEBUG(0, ("talloc failed\n"));
		return -1;
	}

	status = srvstr_push((char *)tmp, SVAL(tmp, smb_flg2),
			     tmp + buf_size, str, grow_size, flags, &result);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("srvstr_push failed\n"));
		return -1;
	}

	/*
	 * Ensure we clear out the extra data we have
	 * grown the buffer by, but not written to.
	 */
	if (buf_size + result < buf_size) {
		return -1;
	}
	if (grow_size < result) {
		return -1;
	}

	memset(tmp + buf_size + result, '\0', grow_size - result);

	set_message_bcc((char *)tmp, smb_buflen(tmp) + result);

	*outbuf = tmp;

	return result;
}

/*
 * Deal with the SMB1 semantics of sending a pathname with a
 * wildcard as the terminal component for a SMB1search or
 * trans2 findfirst.
 */

NTSTATUS filename_convert_smb1_search_path(TALLOC_CTX *ctx,
					   connection_struct *conn,
					   char *name_in,
					   uint32_t ucf_flags,
					   struct files_struct **_dirfsp,
					   struct smb_filename **_smb_fname_out,
					   char **_mask_out)
{
	NTSTATUS status;
	char *p = NULL;
	char *mask = NULL;
	struct smb_filename *smb_fname = NULL;
	NTTIME twrp = 0;

	*_smb_fname_out = NULL;
	*_dirfsp = NULL;
	*_mask_out = NULL;

	DBG_DEBUG("name_in: %s\n", name_in);

	if (ucf_flags & UCF_GMT_PATHNAME) {
		extract_snapshot_token(name_in, &twrp);
		ucf_flags &= ~UCF_GMT_PATHNAME;
	}

	/* Get the original lcomp. */
	mask = get_original_lcomp(ctx, conn, name_in, ucf_flags);
	if (mask == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (mask[0] == '\0') {
		/* Windows and OS/2 systems treat search on the root as * */
		TALLOC_FREE(mask);
		mask = talloc_strdup(ctx, "*");
		if (mask == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}

	DBG_DEBUG("mask = %s\n", mask);

	/*
	 * Remove the terminal component so
	 * filename_convert_dirfsp never sees the mask.
	 */
	p = strrchr_m(name_in, '/');
	if (p == NULL) {
		/* filename_convert_dirfsp handles a '\0' name. */
		name_in[0] = '\0';
	} else {
		*p = '\0';
	}

	DBG_DEBUG("For filename_convert_dirfsp: name_in = %s\n", name_in);

	/* Convert the parent directory path. */
	status = filename_convert_dirfsp(ctx,
					 conn,
					 name_in,
					 ucf_flags,
					 twrp,
					 _dirfsp,
					 &smb_fname);

	if (!NT_STATUS_IS_OK(status)) {
		DBG_DEBUG("filename_convert error for %s: %s\n",
			  name_in,
			  nt_errstr(status));
	}

	*_smb_fname_out = talloc_move(ctx, &smb_fname);
	*_mask_out = talloc_move(ctx, &mask);

	return status;
}
