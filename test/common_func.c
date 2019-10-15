#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "fastcommon/shared_func.h"
#include "fastcommon/logger.h"
#include "common_func.h"

int my_daemon_init()
{
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        logError("file: "__FILE__", line: %d, "
                "getcwd fail, errno: %d, error info: %s",
                __LINE__, errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
    }
#ifndef WIN32
	daemon_init(false);
#endif

	if (chdir(cwd) != 0)
	{
        logError("file: "__FILE__", line: %d, "
                "chdir to %s fail, errno: %d, error info: %s",
                __LINE__, cwd, errno, STRERROR(errno));
		return errno != 0 ? errno : EPERM;
	}

    return 0;
}
