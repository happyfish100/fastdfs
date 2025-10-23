/** * Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.fastken.com/ for more detail.
**/

//storage_sync_func.c

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "fdfs_define.h"
#include "fdfs_global.h"
#include "fastcommon/logger.h"
#include "fastcommon/sockopt.h"
#include "tracker_types.h"
#include "tracker_proto.h"
#include "storage_global.h"
#include "storage_func.h"
#include "storage_sync_func.h"

int storage_sync_connect_storage_server_ex(const FDFSStorageBrief *pStorage,
        ConnectionInfo *conn, bool *check_flag)
{
    int nContinuousFail;
    int previousCodes[FDFS_MULTI_IP_MAX_COUNT];
    int conn_results[FDFS_MULTI_IP_MAX_COUNT];
    int result;
    int i;
    FDFSMultiIP ip_addrs;
    FDFSMultiIP *multi_ip;
    const char *bind_addr;
    char formatted_ip[FORMATTED_IP_SIZE];

    if (g_use_storage_id) {
        FDFSStorageIdInfo *idInfo;
        idInfo = fdfs_get_storage_by_id(pStorage->id);
        if (idInfo == NULL) {
            logWarning("file: "__FILE__", line: %d, "
                    "storage server id: %s not exist "
                    "in storage_ids.conf from tracker server, "
                    "storage ip: %s", __LINE__, pStorage->id,
                    pStorage->ip_addr);
            multi_ip = NULL;
        } else {
            multi_ip = &idInfo->ip_addrs;
        }
    } else {
        multi_ip = NULL;
    }

    if (multi_ip != NULL) {
        ip_addrs = *multi_ip;
    } else {
        ip_addrs.count = 1;
        ip_addrs.index = 0;
        ip_addrs.ips[0].type = fdfs_get_ip_type(pStorage->ip_addr);
        strcpy(ip_addrs.ips[0].address, pStorage->ip_addr);
    }

    conn->sock = -1;
    nContinuousFail = 0;
    memset(previousCodes, 0, sizeof(previousCodes));
    memset(conn_results, 0, sizeof(conn_results));
    do {
        for (i=0; i<ip_addrs.count; i++) {
            strcpy(conn->ip_addr, ip_addrs.ips[i].address);
            if (g_client_bind_addr) {
                bind_addr = is_ipv6_addr(conn->ip_addr) ?
                    SF_G_INNER_BIND_ADDR6 : SF_G_INNER_BIND_ADDR4;
            } else {
                bind_addr = NULL;
            }
            conn->sock = socketCreateExAuto(conn->ip_addr,
                    O_NONBLOCK, bind_addr, &result);
            if (conn->sock < 0) {
                logCrit("file: "__FILE__", line: %d, "
                        "socket create fail, program exit!", __LINE__);
                SF_G_CONTINUE_FLAG = false;
                break;
            }

            if ((conn_results[i]=connectserverbyip_nb(conn->sock,
                            conn->ip_addr, SF_G_INNER_PORT,
                            SF_G_CONNECT_TIMEOUT)) == 0)
            {
                char szFailPrompt[64];
                if (nContinuousFail == 0) {
                    *szFailPrompt = '\0';
                } else {
                    sprintf(szFailPrompt, ", continuous fail "
                            "count: %d", nContinuousFail);
                }

                format_ip_address(conn->ip_addr, formatted_ip);
                logInfo("file: "__FILE__", line: %d, "
                        "successfully connect to storage server "
                        "%s:%u%s", __LINE__, formatted_ip,
                        SF_G_INNER_PORT, szFailPrompt);
                return 0;
            }

            nContinuousFail++;
            if (previousCodes[i] != conn_results[i]) {
                format_ip_address(conn->ip_addr, formatted_ip);
                logError("file: "__FILE__", line: %d, "
                        "connect to storage server %s:%u fail, "
                        "errno: %d, error info: %s",
                        __LINE__, formatted_ip, SF_G_INNER_PORT,
                        conn_results[i], STRERROR(conn_results[i]));
                previousCodes[i] = conn_results[i];
            }

            close(conn->sock);
            conn->sock = -1;
        }

        if (!SF_G_CONTINUE_FLAG) {
            break;
        }

        sleep(1);
    } while (SF_G_CONTINUE_FLAG && *check_flag &&
            pStorage->status != FDFS_STORAGE_STATUS_DELETED &&
            pStorage->status != FDFS_STORAGE_STATUS_IP_CHANGED &&
            pStorage->status != FDFS_STORAGE_STATUS_NONE);

    if (nContinuousFail > 0) {
        int avg_fails;
        avg_fails = (nContinuousFail + ip_addrs.count - 1) / ip_addrs.count;
        for (i=0; i<ip_addrs.count; i++) {
            format_ip_address(ip_addrs.ips[i].address, formatted_ip);
            logError("file: "__FILE__", line: %d, "
                    "connect to storage server %s:%u fail, "
                    "try count: %d, errno: %d, error info: %s",
                    __LINE__, formatted_ip, SF_G_INNER_PORT, avg_fails,
                    conn_results[i], STRERROR(conn_results[i]));
        }
    }

    return conn_results[0] != 0 ? conn_results[0] : ECONNRESET;
}
