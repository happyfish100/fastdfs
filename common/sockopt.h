/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//socketopt.h

#ifndef _SOCKETOPT_H_
#define _SOCKETOPT_H_

#include "common_define.h"

#define FDFS_WRITE_BUFF_SIZE  256 * 1024

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*getnamefunc)(int socket, struct sockaddr *address, \
		socklen_t *address_len);

typedef int (*tcpsenddatafunc)(int sock, void* data, const int size, \
		const int timeout);

typedef int (*tcprecvdata_exfunc)(int sock, void *data, const int size, \
		const int timeout, int *count);

#define getSockIpaddr(sock, buff, bufferSize) \
	getIpaddr(getsockname, sock, buff, bufferSize)

#define getPeerIpaddr(sock, buff, bufferSize) \
	getIpaddr(getpeername, sock, buff, bufferSize)

/** get a line from socket
 *  parameters:
 *          sock: the socket
 *          s: the buffer
 *          size: buffer size (max bytes can receive)
 *          timeout: read timeout
 *  return: error no, 0 success, != 0 fail
*/
int tcpgets(int sock, char *s, const int size, const int timeout);

/** recv data (block mode)
 *  parameters:
 *          sock: the socket
 *          data: the buffer
 *          size: buffer size (max bytes can receive)
 *          timeout: read timeout
 *          count: store the bytes recveived
 *  return: error no, 0 success, != 0 fail
*/
int tcprecvdata_ex(int sock, void *data, const int size, \
		const int timeout, int *count);

/** recv data (non-block mode)
 *  parameters:
 *          sock: the socket
 *          data: the buffer
 *          size: buffer size (max bytes can receive)
 *          timeout: read timeout
 *          count: store the bytes recveived
 *  return: error no, 0 success, != 0 fail
*/
int tcprecvdata_nb_ex(int sock, void *data, const int size, \
		const int timeout, int *count);

/** send data (block mode)
 *  parameters:
 *          sock: the socket
 *          data: the buffer to send
 *          size: buffer size
 *          timeout: write timeout
 *  return: error no, 0 success, != 0 fail
*/
int tcpsenddata(int sock, void* data, const int size, const int timeout);

/** send data (non-block mode)
 *  parameters:
 *          sock: the socket
 *          data: the buffer to send
 *          size: buffer size
 *          timeout: write timeout
 *  return: error no, 0 success, != 0 fail
*/
int tcpsenddata_nb(int sock, void* data, const int size, const int timeout);

/** connect to server by block mode
 *  parameters:
 *          sock: the socket
 *          server_ip: ip address of the server
 *          server_port: port of the server
 *  return: error no, 0 success, != 0 fail
*/
int connectserverbyip(int sock, const char *server_ip, const short server_port);

/** connect to server by non-block mode
 *  parameters:
 *          sock: the socket
 *          server_ip: ip address of the server
 *          server_port: port of the server
 *          timeout: connect timeout in seconds
 *          auto_detect: if detect and adjust the block mode of the socket
 *  return: error no, 0 success, != 0 fail
*/
int connectserverbyip_nb_ex(int sock, const char *server_ip, \
		const short server_port, const int timeout, \
		const bool auto_detect);

/** connect to server by non-block mode, the socket must be set to non-block
 *  parameters:
 *          sock: the socket,  must be set to non-block
 *          server_ip: ip address of the server
 *          server_port: port of the server
 *          timeout: connect timeout in seconds
 *  return: error no, 0 success, != 0 fail
*/
#define connectserverbyip_nb(sock, server_ip, server_port, timeout) \
	connectserverbyip_nb_ex(sock, server_ip, server_port, timeout, false)

/** connect to server by non-block mode, auto detect socket block mode
 *  parameters:
 *          sock: the socket, can be block mode
 *          server_ip: ip address of the server
 *          server_port: port of the server
 *          timeout: connect timeout in seconds
 *  return: error no, 0 success, != 0 fail
*/
#define connectserverbyip_nb_auto(sock, server_ip, server_port, timeout) \
	connectserverbyip_nb_ex(sock, server_ip, server_port, timeout, true)

/** accept client connect request
 *  parameters:
 *          sock: the server socket
 *          timeout: read timeout
 *          err_no: store the error no, 0 for success
 *  return: client socket, < 0 for error
*/
int nbaccept(int sock, const int timeout, int *err_no);

/** set socket options
 *  parameters:
 *          sock: the socket
 *          timeout: read & write timeout
 *  return: error no, 0 success, != 0 fail
*/
int tcpsetserveropt(int fd, const int timeout);

/** set socket non-block options
 *  parameters:
 *          sock: the socket
 *  return: error no, 0 success, != 0 fail
*/
int tcpsetnonblockopt(int fd);

/** set socket no delay on send data
 *  parameters:
 *          sock: the socket
 *          timeout: read & write timeout
 *  return: error no, 0 success, != 0 fail
*/
int tcpsetnodelay(int fd, const int timeout);

/** set socket keep-alive
 *  parameters:
 *          sock: the socket
 *          idleSeconds: max idle time (seconds)
 *  return: error no, 0 success, != 0 fail
*/
int tcpsetkeepalive(int fd, const int idleSeconds);

/** print keep-alive related parameters
 *  parameters:
 *          sock: the socket
 *  return: error no, 0 success, != 0 fail
*/
int tcpprintkeepalive(int fd);

/** get ip address
 *  parameters:
 *          getname: the function name, should be getpeername or getsockname
 *          sock: the socket
 *          buff: buffer to store the ip address
 *          bufferSize: the buffer size (max bytes)
 *  return: in_addr_t, INADDR_NONE for fail
*/
in_addr_t getIpaddr(getnamefunc getname, int sock, \
		char *buff, const int bufferSize);

/** get hostname by it's ip address
 *  parameters:
 *          szIpAddr: the ip address
 *          buff: buffer to store the hostname
 *          bufferSize: the buffer size (max bytes)
 *  return: hostname, empty buffer for error
*/
char *getHostnameByIp(const char *szIpAddr, char *buff, const int bufferSize);

/** get by ip address by it's hostname
 *  parameters:
 *          name: the hostname 
 *          buff: buffer to store the ip address
 *          bufferSize: the buffer size (max bytes)
 *  return: in_addr_t, INADDR_NONE for fail
*/
in_addr_t getIpaddrByName(const char *name, char *buff, const int bufferSize);

/** bind wrapper 
 *  parameters:
 *          sock: the socket
 *          bind_ipaddr: the ip address to bind
 *          port: the port to bind
 *  return: error no, 0 success, != 0 fail
*/
int socketBind(int sock, const char *bind_ipaddr, const int port);

/** start a socket server (socket, bind and listen)
 *  parameters:
 *          sock: the socket
 *          bind_ipaddr: the ip address to bind
 *          port: the port to bind
 *          err_no: store the error no
 *  return: >= 0 server socket, < 0 fail
*/
int socketServer(const char *bind_ipaddr, const int port, int *err_no);

#define tcprecvdata(sock, data, size, timeout) \
	tcprecvdata_ex(sock, data, size, timeout, NULL)

#define tcpsendfile(sock, filename, file_bytes, timeout, total_send_bytes) \
	tcpsendfile_ex(sock, filename, 0, file_bytes, timeout, total_send_bytes)

#define tcprecvdata_nb(sock, data, size, timeout) \
	tcprecvdata_nb_ex(sock, data, size, timeout, NULL)

/** send a file
 *  parameters:
 *          sock: the socket
 *          filename: the file to send
 *          file_offset: file offset, start position
 *          file_bytes: send file length
 *          timeout: write timeout
 *          total_send_bytes: store the send bytes
 *  return: error no, 0 success, != 0 fail
*/
int tcpsendfile_ex(int sock, const char *filename, const int64_t file_offset, \
	const int64_t file_bytes, const int timeout, int64_t *total_send_bytes);

/** receive data to a file
 *  parameters:
 *          sock: the socket
 *          filename: the file to write
 *          file_bytes: file size (bytes) 
 *          fsync_after_written_bytes: call fsync every x bytes
 *          timeout: read/recv timeout
 *          true_file_bytes: store the true file bytes
 *  return: error no, 0 success, != 0 fail
*/
int tcprecvfile(int sock, const char *filename, const int64_t file_bytes, \
		const int fsync_after_written_bytes, const int timeout, \
		int64_t *true_file_bytes);


#define tcprecvinfinitefile(sock, filename, fsync_after_written_bytes, \
			timeout, file_bytes) \
	tcprecvfile(sock, filename, INFINITE_FILE_SIZE, \
		fsync_after_written_bytes, timeout, file_bytes)


/** receive data to a file
 *  parameters:
 *          sock: the socket
 *          filename: the file to write
 *          file_bytes: file size (bytes)
 *          fsync_after_written_bytes: call fsync every x bytes
 *          hash_codes: return hash code of file content
 *          timeout: read/recv timeout
 *  return: error no, 0 success, != 0 fail
*/
int tcprecvfile_ex(int sock, const char *filename, const int64_t file_bytes, \
		const int fsync_after_written_bytes, \
		unsigned int *hash_codes, const int timeout);

/** receive specified data and discard
 *  parameters:
 *          sock: the socket
 *          bytes: data bytes to discard
 *          timeout: read timeout
 *          total_recv_bytes: store the total recv bytes
 *  return: error no, 0 success, != 0 fail
*/
int tcpdiscard(int sock, const int bytes, const int timeout, \
		int64_t *total_recv_bytes);

/** get local host ip addresses
 *  parameters:
 *          ip_addrs: store the ip addresses
 *          max_count: max ip address (max ip_addrs elements)
 *          count: store the ip address count
 *  return: error no, 0 success, != 0 fail
*/
int getlocaladdrs(char ip_addrs[][IP_ADDRESS_SIZE], \
	const int max_count, int *count);

/** get local host ip addresses
 *  parameters:
 *          ip_addrs: store the ip addresses
 *          max_count: max ip address (max ip_addrs elements)
 *          count: store the ip address count
 *  return: error no, 0 success, != 0 fail
*/
int getlocaladdrs1(char ip_addrs[][IP_ADDRESS_SIZE], \
	const int max_count, int *count);

/** get local host ip addresses by if alias prefix
 *  parameters:
 *          if_alias_prefixes: if alias prefixes, such as eth, bond etc.
 *          prefix_count: if alias prefix count
 *          ip_addrs: store the ip addresses
 *          max_count: max ip address (max ip_addrs elements)
 *          count: store the ip address count
 *  return: error no, 0 success, != 0 fail
*/
int gethostaddrs(char **if_alias_prefixes, const int prefix_count, \
	char ip_addrs[][IP_ADDRESS_SIZE], const int max_count, int *count);

#ifdef __cplusplus
}
#endif

#endif

