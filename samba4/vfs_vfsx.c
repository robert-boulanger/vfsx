/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is VFSX (Samba VFS External Bridge).
 *
 * The Initial Developer of the Original Code is
 * Steven R. Farley.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Copyright (C) 2009 Alexander Duscheleit
 * Copyright (C) 2013 Nurahmadie
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * VFSX - External VFS Bridge
 * This transparent VFS module sends VFS operations over a Unix domain
 * socket for external handling.
 */

#include "includes.h"
#include "smbd/proto.h"
#include "syslog.h"
#include "fcntl.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

#define VFSX_MSG_OUT_SIZE 512
#define VFSX_MSG_IN_SIZE 3
#define VFSX_FAIL_ERROR -1
#define VFSX_FAIL_AUTHORIZATION -2
#define VFSX_SUCCESS_TRANSPARENT 0
#define VFSX_SOCKET_FILE "/tmp/vfsx-socket"
#define VFSX_LOG_FILE "/tmp/vfsx.log"

 /* VFSX communication functions */

static void vfsx_write_file(const char *str)
{
	int fd;

	fd = open(VFSX_LOG_FILE, O_RDWR | O_APPEND);
	if (fd != -1) {
		write(fd, str, strlen(str));
		write(fd, "\n", 1);
		close(fd);
	}
	else {
		syslog(LOG_NOTICE, "vfsx_write_file can't write");
	}
}

static int vfsx_write_socket(const char *str, int close_socket)
{
	static int connected = 0;
	static int sd = -1;
	static int count = 0;
	char out[VFSX_MSG_OUT_SIZE];
	char in[VFSX_MSG_IN_SIZE];
	int ret;
	struct sockaddr_un sa;
	// Assume the operation is success
	int result = VFSX_SUCCESS_TRANSPARENT;

	if (!connected) {
		sd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (sd != -1) {
			strncpy(sa.sun_path, VFSX_SOCKET_FILE, strlen(VFSX_SOCKET_FILE) + 1);
			sa.sun_family = AF_UNIX;
			ret = connect(sd, (struct sockaddr *) &sa, sizeof(sa));
			if (ret != -1) {
				syslog(LOG_NOTICE, "vfsx_write_socket connect succeeded");
				connected = 1;
			}
			else {
				syslog(LOG_NOTICE, "vfsx_write_socket connect failed");
				close(sd);
			}
		}
		else {
			syslog(LOG_NOTICE, "vfsx_write_socket open failed");
		}
	}

	if (connected) {
		memset(out, 0, VFSX_MSG_OUT_SIZE);
		strncpy(out, str, strlen(str) + 1);
		ret = write(sd, out, VFSX_MSG_OUT_SIZE);
		if (ret != -1) {
			memset(in, 0, VFSX_MSG_IN_SIZE);
			ret = read(sd, in, VFSX_MSG_IN_SIZE);
			if (ret != -1) {
				result = atoi(in);
				if (close_socket) {
					syslog(LOG_NOTICE, "vfsx_write_socket closing normally");
					close(sd);
					connected = 0;
				}
			}
			else {
				syslog(LOG_NOTICE, "vfsx_write_socket read failed");
				close(sd);
				connected = 0;
			}
		}
		else {
			syslog(LOG_NOTICE, "vfsx_write_socket write failed");
			close(sd);
			connected = 0;
		}
	}

	if (result == VFSX_FAIL_ERROR) {
		// TODO: Correct error code?
		errno = EIO;
	}
	else if (result == VFSX_FAIL_AUTHORIZATION) {
		errno = EPERM;
	}
	return result;
}

static int vfsx_execute(const char *buf, int count)
{
	int close_sock = 0;

	// buf = "user:operation:origpath:arg1,arg2,arg3"

	if (strncmp(buf, "disconnect", 10) == 0) {
		close_sock = 1;
	}

	if (count > 0) {
		//vfsx_write_file(buf);
		return vfsx_write_socket(buf, close_sock);
	}
	else {
		return VFSX_FAIL_ERROR;
	}
}

/* VFS handler functions */

static int vfsx_connect(vfs_handle_struct *handle, const char *svc, const char *user)
{
	int result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "connect:%s", handle->conn->origpath);
	result = SMB_VFS_NEXT_CONNECT(handle, svc, user);
	if (result >= 0 ) vfsx_execute(buf, count);
	return result;
}

static void vfsx_disconnect(vfs_handle_struct *handle)
{
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "disconnect:%s", handle->conn->origpath);
	SMB_VFS_NEXT_DISCONNECT(handle);
	vfsx_execute(buf, count);
}

static DIR *vfsx_opendir(vfs_handle_struct *handle, const char *fname, const char *mask, uint32_t attr)
{
	// TODO: Is this the correct error value?
	DIR *result = NULL;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "opendir:%s:%s", handle->conn->origpath, fname);
	result = SMB_VFS_NEXT_OPENDIR(handle, fname, mask, attr);
	if (result >= 0) vfsx_execute(buf, count);
	return result;
}

static int vfsx_mkdir(vfs_handle_struct *handle, const char *path, mode_t mode)
{
	int result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "mkdir:%s:%s,%d", handle->conn->origpath, path, mode);
	result = SMB_VFS_NEXT_MKDIR(handle, path, mode);
	if (result >= 0) vfsx_execute(buf, count);
	return result;
}

static int vfsx_rmdir(vfs_handle_struct *handle, const char *path)
{
	int result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "rmdir:%s:%s", handle->conn->origpath, path);
	result = SMB_VFS_NEXT_RMDIR(handle, path);
	if (result >= 0) vfsx_execute(buf, count);
	return result;
}

static int vfsx_open(vfs_handle_struct *handle, struct smb_filename *fname, files_struct *fsp, int flags, mode_t mode)
{
	int result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "open:%s:%s,%d,%d", handle->conn->origpath, fname->base_name, flags, mode);
	result = SMB_VFS_NEXT_OPEN(handle, fname, fsp, flags, mode);
	if (result >= 0) vfsx_execute(buf, count);
	return result;
}

static int vfsx_close(vfs_handle_struct *handle, files_struct *fsp)
{
	int result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "close:%s:%s", fsp->conn->origpath, fsp->fsp_name->base_name);
	result = SMB_VFS_NEXT_CLOSE(handle, fsp);
	if (result >= 0) vfsx_execute(buf, count);
	return result;
}

static NTSTATUS vfsx_createfile( vfs_handle_struct *handle,
				    struct smb_request *req,
				    uint16_t root_dir_fid,
				    struct smb_filename *smb_fname,
				    uint32_t access_mask,
				    uint32_t share_access,
				    uint32_t create_disposition,
				    uint32_t create_options,
				    uint32_t file_attributes,
				    uint32_t oplock_request,
				    struct smb2_lease *lease,
				    uint64_t allocation_size,
				    uint32_t private_flags,
				    struct security_descriptor *sd,
				    struct ea_list *ea_list,
				    files_struct **result,
				    int *pinfo,
				    const struct smb2_create_blobs *in_context_blobs,
				    struct smb2_create_blobs *out_context_blobs)
{
    int count;
    char buf[VFSX_MSG_OUT_SIZE];

    count = snprintf(buf, VFSX_MSG_OUT_SIZE, "create:%s:%s", handle->conn->origpath, smb_fname->base_name);
    vfsx_execute(buf, count);
    return create_file_default(handle->conn, req, root_dir_fid, smb_fname,
				   access_mask, share_access,
				   create_disposition, create_options,
				   file_attributes, oplock_request, lease,
				   allocation_size, private_flags,
				   sd, ea_list, result,
				   pinfo, in_context_blobs, out_context_blobs);
	/*
    return SMB_VFS_NEXT_CREATE_FILE(handle->conn, req, root_dir_fid, smb_fname,
				   access_mask, share_access,
				   create_disposition, create_options,
				   file_attributes, oplock_request, lease,
				   allocation_size, private_flags,
				   sd, ea_list, result,
				   pinfo, in_context_blobs, out_context_blobs);
    */
}

static int vfsx_mknod(vfs_handle_struct *handle,  const char *path, mode_t mode, SMB_DEV_T dev)
{
	int result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "create:%s:%s", handle->conn->origpath, path);
	result = SMB_VFS_NEXT_MKNOD(handle, path, mode, dev);
	if (result >= 0) vfsx_execute(buf, count);
	return result;
}

static ssize_t vfsx_read(vfs_handle_struct *handle, files_struct *fsp, void *data, size_t n)
{
	ssize_t result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "read:%s:%s", fsp->conn->origpath, fsp->fsp_name->base_name);
	result = SMB_VFS_NEXT_READ(handle, fsp, data, n);
	if (result >= 0) vfsx_execute(buf, count);
	return result;
}

static ssize_t vfsx_write(vfs_handle_struct *handle, files_struct *fsp, const void *data, size_t n)
{
	ssize_t result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "write:%s:%s", fsp->conn->origpath, fsp->fsp_name->base_name);
	result = SMB_VFS_NEXT_WRITE(handle, fsp, data, n);
	if (result >= 0) vfsx_execute(buf, count);
	return result;
}

static ssize_t vfsx_pread(vfs_handle_struct *handle, files_struct *fsp, void *data, size_t n, off_t offset)
{
	ssize_t result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "pread:%s:%s", fsp->conn->origpath, fsp->fsp_name->base_name);
	result = SMB_VFS_NEXT_PREAD(handle, fsp, data, n, offset);
	if (result >= 0) vfsx_execute(buf, count);
	return result;
}

static ssize_t vfsx_pwrite(vfs_handle_struct *handle, files_struct *fsp, const void *data, size_t n, off_t offset)
{
	ssize_t result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "pwrite:%s:%s", fsp->conn->origpath, fsp->fsp_name->base_name);
	result = SMB_VFS_NEXT_PWRITE(handle, fsp, data, n, offset);
	if (result >= 0) vfsx_execute(buf, count);
	return result;
}

static off_t vfsx_lseek(vfs_handle_struct *handle, files_struct *fsp, off_t offset, int whence)
{
	off_t result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "lseek:%s:%s", fsp->conn->origpath, fsp->fsp_name->base_name);
	result = SMB_VFS_NEXT_LSEEK(handle, fsp, offset, whence);
	if (result >= 0) vfsx_execute(buf, count);
	return result;
}

static int vfsx_rename(vfs_handle_struct *handle,
                       const struct smb_filename *old,
                       const struct smb_filename *new)
{
	int result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "rename:%s:%s,%s", handle->conn->origpath, old->base_name, new->base_name);
	result = SMB_VFS_NEXT_RENAME(handle, old, new);
	if (result >= 0) vfsx_execute(buf, count);
	return result;
}

static int vfsx_unlink(vfs_handle_struct *handle, const struct smb_filename *path)
{
	int result = -1;
	int count;
	char buf[VFSX_MSG_OUT_SIZE];

	count = snprintf(buf, VFSX_MSG_OUT_SIZE, "unlink:%s:%s", handle->conn->origpath, path->base_name);
	result = SMB_VFS_NEXT_UNLINK(handle, path);
	if (result >= 0) vfsx_execute(buf, count);
	return result;
}

/*
static int vfsx_chmod(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode)
{
	int result;

	result = SMB_VFS_NEXT_CHMOD(handle, conn, path, mode);
	return result;
}

static int vfsx_chmod_acl(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode)
{
	int result;

	result = SMB_VFS_NEXT_CHMOD_ACL(handle, conn, path, mode);
	return result;
}

static int vfsx_fchmod(vfs_handle_struct *handle, files_struct *fsp, int fd, mode_t mode)
{
	int result;

	result = SMB_VFS_NEXT_FCHMOD(handle, fsp, fd, mode);
	return result;
}

static int vfsx_fchmod_acl(vfs_handle_struct *handle, files_struct *fsp, int fd, mode_t mode)
{
	int result;

	result = SMB_VFS_NEXT_FCHMOD_ACL(handle, fsp, fd, mode);
	return result;
}
*/

/* VFS operations */

struct vfs_fn_pointers vfsx_fns = {

    /* Disk operations */
    .connect_fn = vfsx_connect,
    .disconnect_fn = vfsx_disconnect,

    /* Directory operations */
    .opendir_fn = vfsx_opendir,
    .mkdir_fn = vfsx_mkdir,
    .rmdir_fn = vfsx_rmdir,

    /* File operations */
    .open_fn = vfsx_open,
    .close_fn = vfsx_close,
    .create_file_fn = vfsx_createfile,
    .mknod_fn = vfsx_mknod,
    .read_fn = vfsx_read,
    .write_fn = vfsx_write,
    .pread_fn = vfsx_pread,
    .pwrite_fn = vfsx_pwrite,
    .lseek_fn = vfsx_lseek,
    .rename_fn = vfsx_rename,
    .unlink_fn = vfsx_unlink,
};


/* VFS module registration */

NTSTATUS vfs_vfsx_init(void);
NTSTATUS vfs_vfsx_init(void)
{
        return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "vfsx", &vfsx_fns);
}


