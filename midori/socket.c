/*
 Copyright 1999-2008 Hiroyuki Yamamoto
 Copyright 2006-2009 Enrico Tröger <enrico.troeger@uvena.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

/*
 This file is based on socket.c in libSylph.
 socket_init(), remove_socket_link_full() and send_open_command() were
 relicensed and taken from Geany and adjusted to be more generic.
*/

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#if !HAVE_UNIQUE

#include <glib.h>
#include <glib/gstdio.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef G_OS_WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  if HAVE_SYS_WAIT_H
#    include <sys/wait.h>
#  endif
#  include <sys/socket.h>
#  include <sys/un.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#endif /* G_OS_WIN32 */
#if HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#if HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif

#include "socket.h"

#define BUFFSIZE	8192

#ifdef G_OS_WIN32
#define SockDesc		SOCKET
#define SOCKET_IS_VALID(s)	((s) != INVALID_SOCKET)
#else
#define SockDesc		gint
#define SOCKET_IS_VALID(s)	((s) >= 0)
#define INVALID_SOCKET		(-1)
#endif

typedef gint (*SockAddrFunc)	(GList		*addr_list,
				 gpointer	 data);

typedef struct _SockConnectData	SockConnectData;
typedef struct _SockLookupData	SockLookupData;
typedef struct _SockAddrData	SockAddrData;
typedef struct _SockSource	SockSource;

struct _SockConnectData {
	gint id;
	gchar *hostname;
	gushort port;
	GList *addr_list;
	GList *cur_addr;
	SockLookupData *lookup_data;
	GIOChannel *channel;
	guint io_tag;
	SockConnectFunc func;
	gpointer data;
};

struct _SockLookupData {
	gchar *hostname;
	pid_t child_pid;
	GIOChannel *channel;
	guint io_tag;
	SockAddrFunc func;
	gpointer data;
};

struct _SockAddrData {
	gint family;
	gint socktype;
	gint protocol;
	gint addr_len;
	struct sockaddr *addr;
};

struct _SockSource {
	GSource parent;
	SockInfo *sock;
};

static guint io_timeout = 60;

static GList *sock_list = NULL;

#ifdef G_OS_UNIX
static GList *sock_connect_data_list = NULL;

static gchar *socket_filename = NULL;
#endif

static gboolean sock_prepare		(GSource	*source,
					 gint		*timeout);
static gboolean sock_check		(GSource	*source);
static gboolean sock_dispatch		(GSource	*source,
					 GSourceFunc	 callback,
					 gpointer	 user_data);

GSourceFuncs sock_watch_funcs = {
	sock_prepare,
	sock_check,
	sock_dispatch,
	NULL
};

gint fd_recv				(gint		 fd,
					 gchar		*buf,
					 gint		 len,
					 gint		 flags);

#ifdef G_OS_WIN32
static SockInfo *sock_find_from_fd	(gint	fd);
#endif

static gint sock_connect_with_timeout	(gint			 sock,
					 const struct sockaddr	*serv_addr,
					 gint			 addrlen,
					 guint			 timeout_secs);

#ifndef INET6
static gint sock_connect_by_hostname	(gint		 sock,
					 const gchar	*hostname,
					 gushort	 port);
#else
static SockDesc sock_connect_by_getaddrinfo	(const gchar	*hostname,
						 gushort	 port);
#endif

#ifdef G_OS_UNIX
static void sock_address_list_free		(GList		*addr_list);

static gboolean sock_connect_async_cb		(GIOChannel	*source,
						 GIOCondition	 condition,
						 gpointer	 data);
static gint sock_connect_async_get_address_info_cb
						(GList		*addr_list,
						 gpointer	 data);

static gint sock_connect_address_list_async	(SockConnectData *conn_data);

static gboolean sock_get_address_info_async_cb	(GIOChannel	*source,
						 GIOCondition	 condition,
						 gpointer	 data);
static SockLookupData *sock_get_address_info_async
						(const gchar	*hostname,
						 gushort	 port,
						 SockAddrFunc	 func,
						 gpointer	 data);
static gint sock_get_address_info_async_cancel	(SockLookupData	*lookup_data);
#endif /* G_OS_UNIX */

void send_open_command(gint sock, const gchar* command, gchar **args)
{
	gsize i;

	g_return_if_fail(sock > -1);
	g_return_if_fail(command != NULL);

	fd_write_all(sock, command, strlen(command));
	fd_write_all(sock, "\n", 1);

	if(!args)
		return;

	for (i = 0; args[i] != NULL; i++)
	{
		fd_write_all(sock, args[i], strlen(args[i]));
                fd_write_all(sock, "\n", 1);
	}
	fd_write_all(sock, ".\n", 2);
}

#ifndef G_OS_WIN32
static void remove_socket_link_full(void)
{
	gchar real_path[512];
	gsize len;

	real_path[0] = '\0';

	/* read the contents of the symbolic link socket_info.file_name and delete it
	 * readlink should return something like "/tmp/appname_socket.499602d2" */
	len = readlink(socket_filename, real_path, sizeof(real_path) - 1);
	if ((gint) len > 0)
	{
		real_path[len] = '\0';
		g_unlink(real_path);
	}
	g_unlink(socket_filename);
}
#endif

/* (Unix domain) socket support to replace the old FIFO code
 * Returns created socket, -1 if an error occurred */
gint socket_init(const gchar *instance_name, const gchar *config_dir, gboolean *exists)
{
	gint sock;
#ifdef G_OS_WIN32
	HANDLE hmutex;
	WSADATA wsadata;
	gint result;

	*exists = FALSE;
	result = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (result != NO_ERROR) {
		g_warning("WSAStartup() failed\n");
		return -1;
	}

	hmutex = CreateMutexA(NULL, FALSE, instance_name);
	if (! hmutex)
	{
		g_debug ("cannot create Mutex\n");
		return -1;
	}
	if (GetLastError() != ERROR_ALREADY_EXISTS)
	{
		/* To support multiple instances with different configuration directories (as we do on
		 * non-Windows systems) we would need to use different port number s but it might be
		 * difficult to get a port number which is unique for a configuration directory (path)
		 * and which is unused. This port number has to be guessed by the first and new instance
		 * and the only data is the configuration directory path.
		 * For now we use one port number, that is we support only one instance at all. */
		sock = fd_open_inet(49876);
		if (sock < 0)
			return 0;
		return sock;
	}

	sock = fd_connect_inet(49876);
	if (sock < 0)
		return -1;
#else
	*exists = FALSE;
	if (socket_filename == NULL)
		socket_filename = g_strdup_printf("%s%c%s_socket_%s_%s",
			config_dir, G_DIR_SEPARATOR, PACKAGE_NAME, g_get_host_name(), instance_name);

	sock = fd_connect_unix(socket_filename);
	if (sock < 0)
	{
		remove_socket_link_full(); /* deletes the socket file and the symlink */
		return fd_open_unix(socket_filename);
	}
#endif
	*exists = TRUE;
	return sock;
}

gint sock_cleanup(void)
{
#ifdef G_OS_WIN32
	WSACleanup();
#else
	if (socket_filename != NULL)
	{
		remove_socket_link_full(); /* deletes the socket file and the symlink */
		g_free(socket_filename);
	}
#endif
	return 0;
}

gint sock_set_io_timeout(guint sec)
{
	io_timeout = sec;
	return 0;
}

gint fd_connect_inet(gushort port)
{
	SockDesc sock;
	struct sockaddr_in addr;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (!SOCKET_IS_VALID(sock)) {
#ifdef G_OS_WIN32
		g_warning("fd_connect_inet(): socket() failed: %d\n",
			  WSAGetLastError());
#else
		perror("fd_connect_inet(): socket");
#endif
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fd_close(sock);
		return -1;
	}

	return sock;
}

gint fd_open_inet(gushort port)
{
	SockDesc sock;
	struct sockaddr_in addr;
	gint val;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (!SOCKET_IS_VALID(sock)) {
#ifdef G_OS_WIN32
		g_warning("fd_open_inet(): socket() failed: %d\n",
			  WSAGetLastError());
#else
		perror("fd_open_inet(): socket");
#endif
		return -1;
	}

	val = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
		perror("setsockopt");
		fd_close(sock);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		fd_close(sock);
		return -1;
	}

	if (listen(sock, 1) < 0) {
		perror("listen");
		fd_close(sock);
		return -1;
	}

	return sock;
}

gint fd_connect_unix(const gchar *path)
{
#ifdef G_OS_UNIX
	gint sock;
	struct sockaddr_un addr;

	sock = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("fd_connect_unix(): socket");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fd_close(sock);
		return -1;
	}

	return sock;
#else
	return -1;
#endif
}

gint fd_open_unix(const gchar *path)
{
#ifdef G_OS_UNIX
	gint sock;
	struct sockaddr_un addr;
	gint val;

	sock = socket(PF_UNIX, SOCK_STREAM, 0);

	if (sock < 0) {
		perror("sock_open_unix(): socket");
		return -1;
	}

	val = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
		perror("setsockopt");
		fd_close(sock);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		fd_close(sock);
		return -1;
	}

	if (listen(sock, 1) < 0) {
		perror("listen");
		fd_close(sock);
		return -1;
	}

	return sock;
#else
	return -1;
#endif
}

gint fd_accept(gint sock)
{
	struct sockaddr_in caddr;
	guint caddr_len;

	caddr_len = sizeof(caddr);
	return accept(sock, (struct sockaddr *)&caddr, &caddr_len);
}

#ifdef G_OS_WIN32
static SockInfo *sock_find_from_fd(gint fd)
{
	GList *cur;

	for (cur = sock_list; cur != NULL; cur = cur->next) {
		if (((SockInfo *)cur->data)->sock == fd)
			return (SockInfo *)cur->data;
	}

	return NULL;
}
#endif

static gint set_nonblocking_mode(gint fd, gboolean nonblock)
{
#ifdef G_OS_WIN32
	gulong val = nonblock ? 1 : 0;
	SockInfo *sock;

	if (!nonblock)
		WSAEventSelect(fd, NULL, 0);
	if (ioctlsocket(fd, FIONBIO, &val) == SOCKET_ERROR) {
		g_warning("set_nonblocking_mode(): ioctlsocket() failed: %d\n",
			  WSAGetLastError());
		return -1;
	}

	sock = sock_find_from_fd(fd);
	if (sock)
		sock->nonblock = nonblock;
	g_debug ("set nonblocking mode to %d\n", nonblock);

	return 0;
#else
	gint flags;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		perror("fcntl");
		return -1;
	}

	if (nonblock)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;

	return fcntl(fd, F_SETFL, flags);
#endif
}

gint sock_set_nonblocking_mode(SockInfo *sock, gboolean nonblock)
{
	gint ret;

	g_return_val_if_fail(sock != NULL, -1);

	ret = set_nonblocking_mode(sock->sock, nonblock);
	if (ret == 0)
		sock->nonblock = nonblock;

	return ret;
}

static gboolean is_nonblocking_mode(gint fd)
{
#ifdef G_OS_WIN32
	SockInfo *sock;

	sock = sock_find_from_fd(fd);
	if (sock)
		return sock->nonblock;

	return FALSE;
#else
	gint flags;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		perror("fcntl");
		return FALSE;
	}

	return ((flags & O_NONBLOCK) != 0);
#endif
}

gboolean sock_is_nonblocking_mode(SockInfo *sock)
{
	g_return_val_if_fail(sock != NULL, FALSE);

#ifdef G_OS_WIN32
	return sock->nonblock;
#else
	return is_nonblocking_mode(sock->sock);
#endif
}

gboolean sock_has_read_data(SockInfo *sock)
{
#ifdef G_OS_WIN32
	gulong val;

	if (ioctlsocket(sock->sock, FIONREAD, &val) < 0) {
		g_warning("sock_has_read_data(): ioctlsocket() failed: %d\n",
			  WSAGetLastError());
		return TRUE;
	}

	if (val == 0)
		return FALSE;
	else
		return TRUE;
#else
	return TRUE;
#endif
}


static gboolean sock_prepare(GSource *source, gint *timeout)
{
	*timeout = 1;
	return FALSE;
}

static gboolean sock_check(GSource *source)
{
	SockInfo *sock = ((SockSource *)source)->sock;
	struct timeval timeout = {0, 0};
	fd_set fds;
	GIOCondition condition = sock->condition;

	FD_ZERO(&fds);
	FD_SET(sock->sock, &fds);

	select(sock->sock + 1,
	       (condition & G_IO_IN)  ? &fds : NULL,
	       (condition & G_IO_OUT) ? &fds : NULL,
	       NULL, &timeout);

	return FD_ISSET(sock->sock, &fds) != 0;
}

static gboolean sock_dispatch(GSource *source, GSourceFunc callback,
			      gpointer user_data)
{
	SockInfo *sock = ((SockSource *)source)->sock;

	return sock->callback(sock, sock->condition, sock->data);
}

static gboolean sock_watch_cb(GIOChannel *source, GIOCondition condition,
			      gpointer data)
{
	SockInfo *sock = (SockInfo *)data;

	if ((condition & sock->condition) == 0)
		return TRUE;

	return sock->callback(sock, sock->condition, sock->data);
}

guint sock_add_watch(SockInfo *sock, GIOCondition condition, SockFunc func,
		     gpointer data)
{
	sock->callback = func;
	sock->condition = condition;
	sock->data = data;
	return g_io_add_watch(sock->sock_ch, condition, sock_watch_cb, sock);
}

static gint fd_check_io(gint fd, GIOCondition cond)
{
	struct timeval timeout;
	fd_set fds;

	if (is_nonblocking_mode(fd))
		return 0;

	timeout.tv_sec  = io_timeout;
	timeout.tv_usec = 0;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	if (cond == G_IO_IN) {
		select(fd + 1, &fds, NULL, NULL,
		       io_timeout > 0 ? &timeout : NULL);
	} else {
		select(fd + 1, NULL, &fds, NULL,
		       io_timeout > 0 ? &timeout : NULL);
	}

	if (FD_ISSET(fd, &fds)) {
		return 0;
	} else {
		g_warning("Socket IO timeout\n");
		return -1;
	}
}

#ifdef G_OS_UNIX
static sigjmp_buf jmpenv;

static void G_GNUC_NORETURN timeout_handler(gint sig)
{
	siglongjmp(jmpenv, 1);
}
#endif

static gint sock_connect_with_timeout(gint sock,
				      const struct sockaddr *serv_addr,
				      gint addrlen,
				      guint timeout_secs)
{
	gint ret;
#ifdef G_OS_UNIX
	void (*prev_handler)(gint);

	alarm(0);
	prev_handler = signal(SIGALRM, timeout_handler);
	if (sigsetjmp(jmpenv, 1)) {
		alarm(0);
		signal(SIGALRM, prev_handler);
		errno = ETIMEDOUT;
		return -1;
	}
	alarm(timeout_secs);
#endif

	ret = connect(sock, serv_addr, addrlen);

#ifdef G_OS_UNIX
	alarm(0);
	signal(SIGALRM, prev_handler);
#endif

	return ret;
}

struct hostent *my_gethostbyname(const gchar *hostname)
{
	struct hostent *hp;
#ifdef G_OS_UNIX
	void (*prev_handler)(gint);

	alarm(0);
	prev_handler = signal(SIGALRM, timeout_handler);
	if (sigsetjmp(jmpenv, 1)) {
		alarm(0);
		signal(SIGALRM, prev_handler);
		fprintf(stderr, "%s: host lookup timed out.\n", hostname);
		errno = 0;
		return NULL;
	}
	alarm(io_timeout);
#endif

	if ((hp = gethostbyname(hostname)) == NULL) {
#ifdef G_OS_UNIX
		alarm(0);
		signal(SIGALRM, prev_handler);
#endif
		fprintf(stderr, "%s: unknown host.\n", hostname);
		errno = 0;
		return NULL;
	}

#ifdef G_OS_UNIX
	alarm(0);
	signal(SIGALRM, prev_handler);
#endif

	return hp;
}

#ifndef INET6
static gint my_inet_aton(const gchar *hostname, struct in_addr *inp)
{
#ifdef HAVE_INET_ATON
	return inet_aton(hostname, inp);
#else
#ifdef HAVE_INET_ADDR
	guint32 inaddr;

	inaddr = inet_addr(hostname);
	if (inaddr != -1) {
		memcpy(inp, &inaddr, sizeof(inaddr));
		return 1;
	} else
		return 0;
#else
	return 0;
#endif
#endif /* HAVE_INET_ATON */
}

static gint sock_connect_by_hostname(gint sock, const gchar *hostname,
				     gushort port)
{
	struct hostent *hp;
	struct sockaddr_in ad;

	memset(&ad, 0, sizeof(ad));
	ad.sin_family = AF_INET;
	ad.sin_port = htons(port);

	if (!my_inet_aton(hostname, &ad.sin_addr)) {
		if ((hp = my_gethostbyname(hostname)) == NULL) {
			fprintf(stderr, "%s: unknown host.\n", hostname);
			errno = 0;
			return -1;
		}

		if (hp->h_length != 4 && hp->h_length != 8) {
			fprintf(stderr, "illegal address length received for host %s\n", hostname);
			errno = 0;
			return -1;
		}

		memcpy(&ad.sin_addr, hp->h_addr, hp->h_length);
	}

	return sock_connect_with_timeout(sock, (struct sockaddr *)&ad,
					 sizeof(ad), io_timeout);
}

#else /* INET6 */

#ifdef G_OS_WIN32
/* MinGW defines gai_strerror() in ws2tcpip.h, but it is not implemented. */
#undef gai_strerror
const gchar *gai_strerror(gint errcode)
{
	static gchar str[32];

	g_snprintf(str, sizeof(str), "gai errcode: (%d)", errcode);
	return str;
}
#endif

static SockDesc sock_connect_by_getaddrinfo(const gchar *hostname, gushort port)
{
	SockDesc sock = INVALID_SOCKET;
	gint gai_error;
	struct addrinfo hints, *res, *ai;
	gchar port_str[6];

	memset(&hints, 0, sizeof(hints));
	/* hints.ai_flags = AI_CANONNAME; */
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	/* convert port from integer to string. */
	g_snprintf(port_str, sizeof(port_str), "%d", port);

	if ((gai_error = getaddrinfo(hostname, port_str, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo for %s:%s failed: %s\n",
			hostname, port_str, gai_strerror(gai_error));
		return INVALID_SOCKET;
	}

	for (ai = res; ai != NULL; ai = ai->ai_next) {
		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (!SOCKET_IS_VALID(sock))
			continue;

		if (sock_connect_with_timeout
			(sock, ai->ai_addr, ai->ai_addrlen, io_timeout) == 0)
			break;

		fd_close(sock);
	}

	if (res != NULL)
		freeaddrinfo(res);

	if (ai == NULL)
		return INVALID_SOCKET;

	return sock;
}
#endif /* !INET6 */

SockInfo *sock_connect(const gchar *hostname, gushort port)
{
	SockDesc sock;
	SockInfo *sockinfo;

#ifdef INET6
	sock = sock_connect_by_getaddrinfo(hostname, port);
	if (!SOCKET_IS_VALID(sock))
		return NULL;
#else
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (!SOCKET_IS_VALID(sock)) {
#ifdef G_OS_WIN32
		g_warning("socket() failed: %d\n", WSAGetLastError());
#else
		perror("socket");
#endif /* G_OS_WIN32 */
		return NULL;
	}

	if (sock_connect_by_hostname(sock, hostname, port) < 0) {
		if (errno != 0) perror("connect");
		fd_close(sock);
		return NULL;
	}
#endif /* INET6 */

	sockinfo = g_new0(SockInfo, 1);
	sockinfo->sock = sock;
	sockinfo->sock_ch = g_io_channel_unix_new(sock);
	sockinfo->hostname = g_strdup(hostname);
	sockinfo->port = port;
	sockinfo->state = CONN_ESTABLISHED;
	sockinfo->nonblock = FALSE;

	sock_list = g_list_prepend(sock_list, sockinfo);

	g_usleep(100000);

	return sockinfo;
}

#ifdef G_OS_UNIX
static void sock_address_list_free(GList *addr_list)
{
	GList *cur;

	for (cur = addr_list; cur != NULL; cur = cur->next) {
		SockAddrData *addr_data = (SockAddrData *)cur->data;
		g_free(addr_data->addr);
		g_free(addr_data);
	}

	g_list_free(addr_list);
}

/* asynchronous TCP connection */

static gboolean sock_connect_async_cb(GIOChannel *source,
				      GIOCondition condition, gpointer data)
{
	SockConnectData *conn_data = (SockConnectData *)data;
	gint fd;
	gint val;
	guint len;
	SockInfo *sockinfo;

	fd = g_io_channel_unix_get_fd(source);

	conn_data->io_tag = 0;
	conn_data->channel = NULL;
	g_io_channel_unref(source);

	if (condition & (G_IO_ERR | G_IO_HUP)) {
		g_debug ("sock_connect_async_cb: condition = %d\n",
			    condition);
		fd_close(fd);
		sock_connect_address_list_async(conn_data);
		return FALSE;
	}

	len = sizeof(val);
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &val, &len) < 0) {
		perror("getsockopt");
		fd_close(fd);
		sock_connect_address_list_async(conn_data);
		return FALSE;
	}

	if (val != 0) {
		fd_close(fd);
		sock_connect_address_list_async(conn_data);
		return FALSE;
	}

	sockinfo = g_new0(SockInfo, 1);
	sockinfo->sock = fd;
	sockinfo->sock_ch = g_io_channel_unix_new(fd);
	sockinfo->hostname = g_strdup(conn_data->hostname);
	sockinfo->port = conn_data->port;
	sockinfo->state = CONN_ESTABLISHED;
	sockinfo->nonblock = TRUE;

	sock_list = g_list_prepend(sock_list, sockinfo);

	conn_data->func(sockinfo, conn_data->data);

	sock_connect_async_cancel(conn_data->id);

	return FALSE;
}

static gint sock_connect_async_get_address_info_cb(GList *addr_list,
						   gpointer data)
{
	SockConnectData *conn_data = (SockConnectData *)data;

	conn_data->addr_list = addr_list;
	conn_data->cur_addr = addr_list;
	conn_data->lookup_data = NULL;

	return sock_connect_address_list_async(conn_data);
}

gint sock_connect_async(const gchar *hostname, gushort port,
			SockConnectFunc func, gpointer data)
{
	static gint id = 1;
	SockConnectData *conn_data;

	conn_data = g_new0(SockConnectData, 1);
	conn_data->id = id++;
	conn_data->hostname = g_strdup(hostname);
	conn_data->port = port;
	conn_data->addr_list = NULL;
	conn_data->cur_addr = NULL;
	conn_data->io_tag = 0;
	conn_data->func = func;
	conn_data->data = data;

	conn_data->lookup_data = sock_get_address_info_async
		(hostname, port, sock_connect_async_get_address_info_cb,
		 conn_data);

	if (conn_data->lookup_data == NULL) {
		g_free(conn_data->hostname);
		g_free(conn_data);
		return -1;
	}

	sock_connect_data_list = g_list_append(sock_connect_data_list,
					       conn_data);

	return conn_data->id;
}

gint sock_connect_async_cancel(gint id)
{
	SockConnectData *conn_data = NULL;
	GList *cur;

	for (cur = sock_connect_data_list; cur != NULL; cur = cur->next) {
		if (((SockConnectData *)cur->data)->id == id) {
			conn_data = (SockConnectData *)cur->data;
			break;
		}
	}

	if (conn_data) {
		sock_connect_data_list = g_list_remove(sock_connect_data_list,
						       conn_data);

		if (conn_data->lookup_data)
			sock_get_address_info_async_cancel
				(conn_data->lookup_data);

		if (conn_data->io_tag > 0)
			g_source_remove(conn_data->io_tag);
		if (conn_data->channel) {
			g_io_channel_shutdown(conn_data->channel, FALSE, NULL);
			g_io_channel_unref(conn_data->channel);
		}

		sock_address_list_free(conn_data->addr_list);
		g_free(conn_data->hostname);
		g_free(conn_data);
	} else {
		g_warning("sock_connect_async_cancel: id %d not found.\n", id);
		return -1;
	}

	return 0;
}

static gint sock_connect_address_list_async(SockConnectData *conn_data)
{
	SockAddrData *addr_data;
	gint sock = -1;

	for (; conn_data->cur_addr != NULL;
	     conn_data->cur_addr = conn_data->cur_addr->next) {
		addr_data = (SockAddrData *)conn_data->cur_addr->data;

		if ((sock = socket(addr_data->family, addr_data->socktype,
				   addr_data->protocol)) < 0) {
			perror("socket");
			continue;
		}

		set_nonblocking_mode(sock, TRUE);

		if (connect(sock, addr_data->addr, addr_data->addr_len) < 0) {
			if (EINPROGRESS == errno) {
				break;
			} else {
				perror("connect");
				fd_close(sock);
			}
		} else
			break;
	}

	if (conn_data->cur_addr == NULL) {
		g_warning("sock_connect_address_list_async: "
			  "connection to %s:%d failed\n",
			  conn_data->hostname, conn_data->port);
		conn_data->func(NULL, conn_data->data);
		sock_connect_async_cancel(conn_data->id);
		return -1;
	}

	g_debug ("sock_connect_address_list_async: waiting for connect\n");

	conn_data->cur_addr = conn_data->cur_addr->next;

	conn_data->channel = g_io_channel_unix_new(sock);
	conn_data->io_tag = g_io_add_watch(conn_data->channel,
					   G_IO_OUT | G_IO_ERR | G_IO_HUP,
					   sock_connect_async_cb, conn_data);

	return 0;
}

static gint sock_kill_process(pid_t pid)
{
	pid_t ret = (pid_t)-1;

	kill(pid, SIGKILL);

	while (ret == (pid_t)-1) {
		if ((ret = waitpid(pid, NULL, 0)) != pid) {
			perror("sock_kill_process(): waitpid");
			if (ret == (pid_t)-1 && errno != EINTR)
				break;
		}
	}

	return (gint)pid;
}

/* asynchronous DNS lookup */

static gboolean sock_get_address_info_async_cb(GIOChannel *source,
					       GIOCondition condition,
					       gpointer data)
{
	SockLookupData *lookup_data = (SockLookupData *)data;
	GList *addr_list = NULL;
	SockAddrData *addr_data;
	gsize bytes_read;
	gint ai_member[4];
	struct sockaddr *addr;

	for (;;) {
		if (g_io_channel_read(source, (gchar *)ai_member,
				      sizeof(ai_member), &bytes_read)
		    != G_IO_ERROR_NONE) {
			g_warning("sock_get_address_info_async_cb: "
				  "address length read error\n");
			break;
		}

		if (bytes_read == 0 || bytes_read != sizeof(ai_member))
			break;

		if (ai_member[0] == AF_UNSPEC) {
			g_warning("DNS lookup failed\n");
			break;
		}

		addr = g_malloc(ai_member[3]);
		if (g_io_channel_read(source, (gchar *)addr, ai_member[3],
				      &bytes_read)
		    != G_IO_ERROR_NONE) {
			g_warning("sock_get_address_info_async_cb: "
				  "address data read error\n");
			g_free(addr);
			break;
		}

		if (bytes_read != (gsize)ai_member[3]) {
			g_warning("sock_get_address_info_async_cb: "
				  "incomplete address data\n");
			g_free(addr);
			break;
		}

		addr_data = g_new0(SockAddrData, 1);
		addr_data->family = ai_member[0];
		addr_data->socktype = ai_member[1];
		addr_data->protocol = ai_member[2];
		addr_data->addr_len = ai_member[3];
		addr_data->addr = addr;

		addr_list = g_list_append(addr_list, addr_data);
	}

	g_io_channel_shutdown(source, FALSE, NULL);
	g_io_channel_unref(source);

	sock_kill_process(lookup_data->child_pid);

	lookup_data->func(addr_list, lookup_data->data);

	g_free(lookup_data->hostname);
	g_free(lookup_data);

	return FALSE;
}

static SockLookupData *sock_get_address_info_async(const gchar *hostname,
						   gushort port,
						   SockAddrFunc func,
						   gpointer data)
{
	SockLookupData *lookup_data = NULL;
	gint pipe_fds[2];
	pid_t pid;

	if (pipe(pipe_fds) < 0) {
		perror("pipe");
		func(NULL, data);
		return NULL;
	}

	if ((pid = fork()) < 0) {
		perror("fork");
		func(NULL, data);
		return NULL;
	}

	/* child process */
	if (pid == 0) {
#ifdef INET6
		gint gai_err;
		struct addrinfo hints, *res, *ai;
		gchar port_str[6];
#else /* !INET6 */
		struct hostent *hp;
		gchar **addr_list_p;
		struct sockaddr_in ad;
#endif /* INET6 */
		gint ai_member[4] = {AF_UNSPEC, 0, 0, 0};

		close(pipe_fds[0]);

#ifdef INET6
		memset(&hints, 0, sizeof(hints));
		/* hints.ai_flags = AI_CANONNAME; */
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		g_snprintf(port_str, sizeof(port_str), "%d", port);

		gai_err = getaddrinfo(hostname, port_str, &hints, &res);
		if (gai_err != 0) {
			g_warning("getaddrinfo for %s:%s failed: %s\n",
				  hostname, port_str, gai_strerror(gai_err));
			fd_write_all(pipe_fds[1], (gchar *)ai_member,
				     sizeof(ai_member));
			close(pipe_fds[1]);
			_exit(1);
		}

		for (ai = res; ai != NULL; ai = ai->ai_next) {
			ai_member[0] = ai->ai_family;
			ai_member[1] = ai->ai_socktype;
			ai_member[2] = ai->ai_protocol;
			ai_member[3] = ai->ai_addrlen;

			fd_write_all(pipe_fds[1], (gchar *)ai_member,
				     sizeof(ai_member));
			fd_write_all(pipe_fds[1], (gchar *)ai->ai_addr,
				     ai->ai_addrlen);
		}

		if (res != NULL)
			freeaddrinfo(res);
#else /* !INET6 */
		hp = my_gethostbyname(hostname);
		if (hp == NULL || hp->h_addrtype != AF_INET) {
			fd_write_all(pipe_fds[1], (gchar *)ai_member,
				     sizeof(ai_member));
			close(pipe_fds[1]);
			_exit(1);
		}

		ai_member[0] = AF_INET;
		ai_member[1] = SOCK_STREAM;
		ai_member[2] = IPPROTO_TCP;
		ai_member[3] = sizeof(ad);

		memset(&ad, 0, sizeof(ad));
		ad.sin_family = AF_INET;
		ad.sin_port = htons(port);

		for (addr_list_p = hp->h_addr_list; *addr_list_p != NULL;
		     addr_list_p++) {
			memcpy(&ad.sin_addr, *addr_list_p, hp->h_length);
			fd_write_all(pipe_fds[1], (gchar *)ai_member,
				     sizeof(ai_member));
			fd_write_all(pipe_fds[1], (gchar *)&ad, sizeof(ad));
		}
#endif /* INET6 */

		close(pipe_fds[1]);

		_exit(0);
	} else {
		close(pipe_fds[1]);

		lookup_data = g_new0(SockLookupData, 1);
		lookup_data->hostname = g_strdup(hostname);
		lookup_data->child_pid = pid;
		lookup_data->func = func;
		lookup_data->data = data;

		lookup_data->channel = g_io_channel_unix_new(pipe_fds[0]);
		lookup_data->io_tag = g_io_add_watch
			(lookup_data->channel, G_IO_IN,
			 sock_get_address_info_async_cb, lookup_data);
	}

	return lookup_data;
}

static gint sock_get_address_info_async_cancel(SockLookupData *lookup_data)
{
	if (lookup_data->io_tag > 0)
		g_source_remove(lookup_data->io_tag);
	if (lookup_data->channel) {
		g_io_channel_shutdown(lookup_data->channel, FALSE, NULL);
		g_io_channel_unref(lookup_data->channel);
	}

	if (lookup_data->child_pid > 0)
		sock_kill_process(lookup_data->child_pid);

	g_free(lookup_data->hostname);
	g_free(lookup_data);

	return 0;
}
#endif /* G_OS_UNIX */


gint sock_printf(SockInfo *sock, const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE];

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	return sock_write_all(sock, buf, strlen(buf));
}

#ifdef G_OS_WIN32
static void sock_set_errno_from_last_error(gint error)
{
	switch (error) {
	case WSAEWOULDBLOCK:
		errno = EAGAIN;
		break;
	default:
		g_debug ("last error = %d\n", error);
		errno = 0;
		break;
	}
}
#endif

gint sock_read(SockInfo *sock, gchar *buf, gint len)
{
	g_return_val_if_fail(sock != NULL, -1);

	return fd_read(sock->sock, buf, len);
}

gint fd_read(gint fd, gchar *buf, gint len)
{
#ifdef G_OS_WIN32
	return fd_recv(fd, buf, len, 0);
#else
	if (fd_check_io(fd, G_IO_IN) < 0)
		return -1;

	return read(fd, buf, len);
#endif
}

gint sock_write(SockInfo *sock, const gchar *buf, gint len)
{
	g_return_val_if_fail(sock != NULL, -1);

	return fd_write(sock->sock, buf, len);
}

gint fd_write(gint fd, const gchar *buf, gint len)
{
#ifdef G_OS_WIN32
	gint ret;
#endif
	if (fd_check_io(fd, G_IO_OUT) < 0)
		return -1;

#ifdef G_OS_WIN32
	ret = send(fd, buf, len, 0);
	if (ret == SOCKET_ERROR) {
		gint err;
		err = WSAGetLastError();
		sock_set_errno_from_last_error(err);
		if (err != WSAEWOULDBLOCK)
			g_warning("fd_write() failed with %d (errno = %d)\n",
				  err, errno);
	}
	return ret;
#else
	return write(fd, buf, len);
#endif
}

gint sock_write_all(SockInfo *sock, const gchar *buf, gint len)
{
	g_return_val_if_fail(sock != NULL, -1);

	return fd_write_all(sock->sock, buf, len);
}

gint fd_write_all(gint fd, const gchar *buf, gint len)
{
	gint n, wrlen = 0;

	while (len) {
		n = fd_write(fd, buf, len);
		if (n <= 0)
			return -1;
		len -= n;
		wrlen += n;
		buf += n;
	}

	return wrlen;
}

gint fd_recv(gint fd, gchar *buf, gint len, gint flags)
{
#ifdef G_OS_WIN32
	gint ret;
#endif
	if (fd_check_io(fd, G_IO_IN) < 0)
		return -1;

#ifdef G_OS_WIN32
	ret = recv(fd, buf, len, flags);
	if (ret == SOCKET_ERROR) {
		gint err;
		err = WSAGetLastError();
		sock_set_errno_from_last_error(err);
		if (err != WSAEWOULDBLOCK)
			g_warning("fd_recv(): failed with %d (errno = %d)\n",
				  err, errno);
	}
	return ret;
#else
	return recv(fd, buf, len, flags);
#endif
}

gint fd_gets(gint fd, gchar *buf, gint len)
{
	gchar *newline, *bp = buf;
	gint n;

	if (--len < 1)
		return -1;
	do {
		if ((n = fd_recv(fd, bp, len, MSG_PEEK)) <= 0)
			return -1;
		if ((newline = memchr(bp, '\n', n)) != NULL)
			n = newline - bp + 1;
		if ((n = fd_read(fd, bp, n)) < 0)
			return -1;
		bp += n;
		len -= n;
	} while (!newline && len);

	*bp = '\0';
	return bp - buf;
}

gint sock_gets(SockInfo *sock, gchar *buf, gint len)
{
	g_return_val_if_fail(sock != NULL, -1);

	return fd_gets(sock->sock, buf, len);
}

gint fd_getline(gint fd, gchar **line)
{
	gchar buf[BUFFSIZE];
	gchar *str = NULL;
	gint len;
	gulong size = 0;
	gulong cur_offset = 0;

	while ((len = fd_gets(fd, buf, sizeof(buf))) > 0) {
		size += len;
		str = g_realloc(str, size + 1);
		memcpy(str + cur_offset, buf, len + 1);
		cur_offset += len;
		if (buf[len - 1] == '\n')
			break;
	}

	*line = str;

	if (!str)
		return -1;
	else
		return (gint)size;
}

gint sock_getline(SockInfo *sock, gchar **line)
{
	g_return_val_if_fail(sock != NULL, -1);
	g_return_val_if_fail(line != NULL, -1);

	return fd_getline(sock->sock, line);
}

gint sock_puts(SockInfo *sock, const gchar *buf)
{
	gint ret;

	if ((ret = sock_write_all(sock, buf, strlen(buf))) < 0)
		return ret;
	return sock_write_all(sock, "\r\n", 2);
}

/* peek at the socket data without actually reading it */
gint sock_peek(SockInfo *sock, gchar *buf, gint len)
{
	g_return_val_if_fail(sock != NULL, -1);

	return fd_recv(sock->sock, buf, len, MSG_PEEK);
}

gint sock_close(SockInfo *sock)
{
	GList *cur;

	if (!sock)
		return 0;

	if (sock->sock_ch) {
		g_io_channel_shutdown(sock->sock_ch, FALSE, NULL);
		g_io_channel_unref(sock->sock_ch);
	}

	for (cur = sock_list; cur != NULL; cur = cur->next) {
		if ((SockInfo *)cur->data == sock) {
			sock_list = g_list_remove(sock_list, sock);
			break;
		}
	}

	g_free(sock->hostname);
	g_free(sock);

	return 0;
}

gint fd_close(gint fd)
{
#ifdef G_OS_WIN32
	return closesocket(fd);
#else
	return close(fd);
#endif
}

#endif
