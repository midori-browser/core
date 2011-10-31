/*
 Copyright 1999-2008 Hiroyuki Yamamoto

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#ifndef __SYLPH_SOCKET_H__
#define __SYLPH_SOCKET_H__

#include <glib.h>
#include "config.h"
#if HAVE_NETDB_H
#  include <netdb.h>
#endif

typedef struct _SockInfo	SockInfo;

#if USE_SSL
#  include <openssl/ssl.h>
#endif

typedef enum
{
	CONN_READY,
	CONN_LOOKUPSUCCESS,
	CONN_ESTABLISHED,
	CONN_LOOKUPFAILED,
	CONN_FAILED
} ConnectionState;

typedef gint (*SockConnectFunc)		(SockInfo	*sock,
					 gpointer	 data);
typedef gboolean (*SockFunc)		(SockInfo	*sock,
					 GIOCondition	 condition,
					 gpointer	 data);

struct _SockInfo
{
	gint sock;
#if USE_SSL
	SSL *ssl;
#else
	gpointer ssl;
#endif
	GIOChannel *sock_ch;

	gchar *hostname;
	gushort port;
	ConnectionState state;
	gboolean nonblock;
	gpointer data;

	SockFunc callback;
	GIOCondition condition;
};

void send_open_command			(gint sock, const gchar *command,
					 gchar **args);
gint socket_init			(const gchar *instance_name,
					 const gchar *config_dir, gboolean *exists);

gint sock_cleanup			(void);

gint sock_set_io_timeout		(guint sec);

gint sock_set_nonblocking_mode		(SockInfo *sock, gboolean nonblock);
gboolean sock_is_nonblocking_mode	(SockInfo *sock);

gboolean sock_has_read_data		(SockInfo *sock);

guint sock_add_watch			(SockInfo *sock, GIOCondition condition,
					 SockFunc func, gpointer data);

struct hostent *my_gethostbyname	(const gchar *hostname);

SockInfo *sock_connect			(const gchar *hostname, gushort port);
#ifdef G_OS_UNIX
gint sock_connect_async			(const gchar *hostname, gushort port,
					 SockConnectFunc func, gpointer data);
gint sock_connect_async_cancel		(gint id);
#endif

/* Basic I/O functions */
gint sock_printf	(SockInfo *sock, const gchar *format, ...)
			 G_GNUC_PRINTF(2, 3);
gint sock_read		(SockInfo *sock, gchar *buf, gint len);
gint sock_write		(SockInfo *sock, const gchar *buf, gint len);
gint sock_write_all	(SockInfo *sock, const gchar *buf, gint len);
gint sock_gets		(SockInfo *sock, gchar *buf, gint len);
gint sock_getline	(SockInfo *sock, gchar **line);
gint sock_puts		(SockInfo *sock, const gchar *buf);
gint sock_peek		(SockInfo *sock, gchar *buf, gint len);
gint sock_close		(SockInfo *sock);

/* Functions to directly work on FD.  They are needed for pipes */
gint fd_connect_inet	(gushort port);
gint fd_open_inet	(gushort port);
gint fd_connect_unix	(const gchar *path);
gint fd_open_unix	(const gchar *path);
gint fd_accept		(gint sock);

gint fd_read		(gint sock, gchar *buf, gint len);
gint fd_write		(gint sock, const gchar *buf, gint len);
gint fd_write_all	(gint sock, const gchar *buf, gint len);
gint fd_gets		(gint sock, gchar *buf, gint len);
gint fd_getline		(gint sock, gchar **line);
gint fd_close		(gint sock);

/* Functions for SSL */
#if USE_SSL
gint ssl_read		(SSL *ssl, gchar *buf, gint len);
gint ssl_write		(SSL *ssl, const gchar *buf, gint len);
gint ssl_write_all	(SSL *ssl, const gchar *buf, gint len);
gint ssl_gets		(SSL *ssl, gchar *buf, gint len);
gint ssl_getline	(SSL *ssl, gchar **line);
gint ssl_peek		(SSL *ssl, gchar *buf, gint len);
void ssl_done_socket	(SockInfo	*sockinfo);
#endif

#endif /* __SYLPH_SOCKET_H__ */
