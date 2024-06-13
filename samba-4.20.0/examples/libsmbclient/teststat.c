#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <libsmbclient.h>
#include <stdbool.h>
#include "get_auth_data_fn.h"

static const char *filetypestr(mode_t mode)
{
	if (S_ISREG(mode)) {
		return "regular file";
	}
	if (S_ISDIR(mode)) {
		return "directory";
	}
	if (S_ISFIFO(mode)) {
		return "fifo";
	}
	if (S_ISLNK(mode)) {
		return "symbolic link";
	}
	if (S_ISSOCK(mode)) {
		return "socket";
	}
	if (S_ISCHR(mode)) {
		return "character special file";
	}
	if (S_ISBLK(mode)) {
		return "block special file";
	}
	return "unknown file type";
}

int main(int argc, char * argv[])
{
	SMBCCTX *ctx = NULL;
	int             debug = 0;
	char            m_time[32];
	char            c_time[32];
	char            a_time[32];
	const char *          pSmbPath = NULL;
	const char *          pLocalPath = NULL;
	struct stat     st;
	int ret;

	if (argc == 1) {
		pSmbPath = "smb://RANDOM/Public/small";
		pLocalPath = "/random/home/samba/small";
	}
	else if (argc == 2) {
		pSmbPath = argv[1];
		pLocalPath = NULL;
	}
	else if (argc == 3) {
		pSmbPath = argv[1];
		pLocalPath = argv[2];
	} else {
		printf("usage: %s [ smb://path/to/file "
		       "[ /nfs/or/local/path/to/file ] ]\n",
		       argv[0]);
		return 1;
	}

	ctx = smbc_new_context();
	if (ctx == NULL) {
		perror("smbc_new_context failed");
		return 1;
	}

	smbc_setOptionDebugToStderr(ctx, 1);
	smbc_setDebug(ctx, debug);
	smbc_init_context(ctx);
	smbc_setFunctionAuthData(ctx, get_auth_data_fn);
	smbc_setOptionPosixExtensions(ctx, true);

	ret = smbc_getFunctionStat(ctx)(ctx, pSmbPath, &st);
	if (ret < 0) {
		perror("smbc_stat");
		return 1;
	}

	printf("\nSAMBA\n mtime:%jd/%s ctime:%jd/%s atime:%jd/%s %s\n",
	       (intmax_t)st.st_mtime,
	       ctime_r(&st.st_mtime, m_time),
	       (intmax_t)st.st_ctime,
	       ctime_r(&st.st_ctime, c_time),
	       (intmax_t)st.st_atime,
	       ctime_r(&st.st_atime, a_time),
	       filetypestr(st.st_mode));

	if (pLocalPath != NULL) {
		ret = stat(pLocalPath, &st);
		if (ret < 0) {
			perror("stat");
			return 1;
		}

		printf("LOCAL\n mtime:%jd/%s ctime:%jd/%s atime:%jd/%s %s\n",
		       (intmax_t)st.st_mtime,
		       ctime_r(&st.st_mtime, m_time),
		       (intmax_t)st.st_ctime,
		       ctime_r(&st.st_ctime, c_time),
		       (intmax_t)st.st_atime,
		       ctime_r(&st.st_atime, a_time),
		       filetypestr(st.st_mode));
	}

	return 0;
}
