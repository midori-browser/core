/*
 Copyright (C) 2013 Stephan Haller <nomad@froevel.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "cookie-permission-manager.h"

#include <errno.h>

/* Remove next line if we found a way to show details in infobar */
#define NO_INFOBAR_DETAILS

/* Define this class in GObject system */
G_DEFINE_TYPE(CookiePermissionManager,
				cookie_permission_manager,
				G_TYPE_OBJECT)

/* Properties */
enum
{
	PROP_0,

	PROP_EXTENSION,
	PROP_APPLICATION,

	PROP_DATABASE,
	PROP_DATABASE_FILENAME,
	PROP_UNKNOWN_POLICY,

	PROP_LAST
};

static GParamSpec* CookiePermissionManagerProperties[PROP_LAST]={ 0, };

/* Private structure - access only by public API if needed */
#define COOKIE_PERMISSION_MANAGER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), TYPE_COOKIE_PERMISSION_MANAGER, CookiePermissionManagerPrivate))

struct _CookiePermissionManagerPrivate
{
	/* Extension related */
	MidoriExtension					*extension;
	MidoriApp						*application;
	sqlite3							*database;
	gchar							*databaseFilename;
	CookiePermissionManagerPolicy	unknownPolicy;

	/* Cookie jar related */
	SoupSession						*session;
	SoupCookieJar					*cookieJar;
	SoupSessionFeatureInterface		*featureIface;
	gint							cookieJarChangedID;
};

enum
{
	DOMAIN_COLUMN,
	PATH_COLUMN,
	NAME_COLUMN,
	VALUE_COLUMN,
	EXPIRE_DATE_COLUMN,
	N_COLUMN
};

struct _CookiePermissionManagerModalInfobar
{
	GMainLoop						*mainLoop;
	gint							response;
};

typedef struct _CookiePermissionManagerModalInfobar		CookiePermissionManagerModalInfobar;

/* IMPLEMENTATION: Private variables and methods */

/* Show common error dialog */
static void _cookie_permission_manager_error(CookiePermissionManager *self, const gchar *inReason)
{
	GtkWidget		*dialog;

	/* Show confirmation dialog for undetermined cookies */
	dialog=gtk_message_dialog_new(NULL,
									GTK_DIALOG_MODAL,
									GTK_MESSAGE_ERROR,
									GTK_BUTTONS_OK,
									_("A fatal error occurred which prevents "
									  "the cookie permission manager extension "
									  "to continue. You should disable it."));

	gtk_window_set_title(GTK_WINDOW(dialog), _("Error in cookie permission manager extension"));
	gtk_window_set_icon_name(GTK_WINDOW (dialog), "midori");

	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
												"%s:\n%s",
												_("Reason"),
												inReason);

	gtk_dialog_run(GTK_DIALOG(dialog));

	/* Free up allocated resources */
	gtk_widget_destroy(dialog);
}

/* Open database containing policies for cookie domains.
 * Create database and setup table structure if it does not exist yet.
 */
static void _cookie_permission_manager_open_database(CookiePermissionManager *self)
{
	CookiePermissionManagerPrivate	*priv=self->priv;
	const gchar						*configDir;
	gchar							*error=NULL;
	gint							success;
	sqlite3_stmt					*statement=NULL;

	/* Close any open database */
	if(priv->database)
	{
		g_free(priv->databaseFilename);
		priv->databaseFilename=NULL;

		sqlite3_close(priv->database);
		priv->database=NULL;

		g_object_notify_by_pspec(G_OBJECT(self), CookiePermissionManagerProperties[PROP_DATABASE]);
		g_object_notify_by_pspec(G_OBJECT(self), CookiePermissionManagerProperties[PROP_DATABASE_FILENAME]);
	}

	/* Build path to database file */
	configDir=midori_extension_get_config_dir(priv->extension);
	if(!configDir)
		return;
	
	if(katze_mkdir_with_parents(configDir, 0700))
	{
		g_warning(_("Could not create configuration folder for extension: %s"), g_strerror(errno));

		_cookie_permission_manager_error(self, _("Could not create configuration folder for extension."));
		return;
	}

	/* Open database */
	priv->databaseFilename=g_build_filename(configDir, COOKIE_PERMISSION_DATABASE, NULL);
	success=sqlite3_open(priv->databaseFilename, &priv->database);
	if(success!=SQLITE_OK)
	{
		g_warning(_("Could not open database of extenstion: %s"), sqlite3_errmsg(priv->database));

		g_free(priv->databaseFilename);
		priv->databaseFilename=NULL;

		if(priv->database) sqlite3_close(priv->database);
		priv->database=NULL;

		_cookie_permission_manager_error(self, _("Could not open database of extension."));
		return;
	}

	/* Create table structure if it does not exist */
	success=sqlite3_exec(priv->database,
							"CREATE TABLE IF NOT EXISTS "
							"policies(domain text, value integer);",
							NULL,
							NULL,
							&error);

	if(success==SQLITE_OK)
	{
		success=sqlite3_exec(priv->database,
								"CREATE UNIQUE INDEX IF NOT EXISTS "
								"domain ON policies (domain);",
								NULL,
								NULL,
								&error);
	}

	if(success==SQLITE_OK)
	{
		success=sqlite3_exec(priv->database,
								"PRAGMA journal_mode=TRUNCATE;",
								NULL,
								NULL,
								&error);
	}

	if(success!=SQLITE_OK || error)
	{
		_cookie_permission_manager_error(self, _("Could not set up database structure of extension."));

		if(error)
		{
			g_critical(_("Failed to execute database statement: %s"), error);
			sqlite3_free(error);
		}

		g_free(priv->databaseFilename);
		priv->databaseFilename=NULL;

		sqlite3_close(priv->database);
		priv->database=NULL;
		return;
	}

	// Delete all cookies allowed only in one session
	success=sqlite3_prepare_v2(priv->database,
								"SELECT domain FROM policies WHERE value=? ORDER BY domain DESC;",
								-1,
								&statement,
								NULL);
	if(statement && success==SQLITE_OK) success=sqlite3_bind_int(statement, 1, COOKIE_PERMISSION_MANAGER_POLICY_ACCEPT_FOR_SESSION);
	if(statement && success==SQLITE_OK)
	{
		while(sqlite3_step(statement)==SQLITE_ROW)
		{
			gchar		*domain=(gchar*)sqlite3_column_text(statement, 0);
			GSList		*cookies, *cookie;

#ifdef HAVE_LIBSOUP_2_40_0
			SoupURI		*uri;

			uri=soup_uri_new(NULL);
			soup_uri_set_host(uri, domain);
			soup_uri_set_path(uri, "/");
			cookies=soup_cookie_jar_get_cookie_list(priv->cookieJar, uri, TRUE);
			for(cookie=cookies; cookie; cookie=cookie->next)
			{
				soup_cookie_jar_delete_cookie(priv->cookieJar, (SoupCookie*)cookie->data);
			}
			soup_cookies_free(cookies);
			soup_uri_free(uri);
#else
			cookies=soup_cookie_jar_all_cookies(priv->cookieJar);
			for(cookie=cookies; cookie; cookie=cookie->next)
			{
				if(soup_cookie_domain_matches((SoupCookie*)cookie->data, domain))
				{
					soup_cookie_jar_delete_cookie(priv->cookieJar, (SoupCookie*)cookie->data);
				}
			}
			soup_cookies_free(cookies);
#endif
		}
	}
		else g_warning(_("SQL fails: %s"), sqlite3_errmsg(priv->database));

	sqlite3_finalize(statement);

	g_object_notify_by_pspec(G_OBJECT(self), CookiePermissionManagerProperties[PROP_DATABASE]);
	g_object_notify_by_pspec(G_OBJECT(self), CookiePermissionManagerProperties[PROP_DATABASE_FILENAME]);
}

/* Get policy for cookies from domain */
static gint _cookie_permission_manager_get_policy(CookiePermissionManager *self, SoupCookie *inCookie)
{
	CookiePermissionManagerPrivate	*priv=self->priv;
	sqlite3_stmt					*statement=NULL;
	gchar							*domain;
	gint							error;
	gint							policy=COOKIE_PERMISSION_MANAGER_POLICY_UNDETERMINED;
	gboolean						foundPolicy=FALSE;

	/* Check for open database */
	g_return_val_if_fail(priv->database, COOKIE_PERMISSION_MANAGER_POLICY_UNDETERMINED);

	/* Lookup policy for cookie domain in database */
	domain=g_strdup(soup_cookie_get_domain(inCookie));
	if(*domain=='.') *domain='%';

	error=sqlite3_prepare_v2(priv->database,
								"SELECT domain, value FROM policies WHERE domain LIKE ? ORDER BY domain DESC;",
								-1,
								&statement,
								NULL);
	if(statement && error==SQLITE_OK) error=sqlite3_bind_text(statement, 1, domain, -1, NULL);
	if(statement && error==SQLITE_OK)
	{
		while(policy==COOKIE_PERMISSION_MANAGER_POLICY_UNDETERMINED &&
				sqlite3_step(statement)==SQLITE_ROW)
		{
			gchar		*policyDomain=(gchar*)sqlite3_column_text(statement, 0);

			if(soup_cookie_domain_matches(inCookie, policyDomain))
			{
				policy=sqlite3_column_int(statement, 1);
				foundPolicy=TRUE;
			}
		}
	}
		else g_warning(_("SQL fails: %s"), sqlite3_errmsg(priv->database));

	sqlite3_finalize(statement);

	/* Check if policy is undetermined. If it is then check if this policy was set by user.
	 * If it was not set by user, check what to do.
	 */
	if(!foundPolicy)
	{
		/* A SoupCookieJar that doesn't want to accept any cookies should override the user's
		 * choice, in case of e.g. private mode, to err on the side of caution. */
		SoupCookieJarAcceptPolicy soup_policy=soup_cookie_jar_get_accept_policy(priv->cookieJar);

		if(soup_policy==SOUP_COOKIE_JAR_ACCEPT_ALWAYS || soup_policy==SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY)
		{
			policy=priv->unknownPolicy;
		}
		else
		{
			if(soup_policy!=SOUP_COOKIE_JAR_ACCEPT_NEVER)
				g_critical(_("Could not determine global cookie policy to set for domain: %s"), domain);
			policy=COOKIE_PERMISSION_MANAGER_POLICY_BLOCK;
		}
	}

	/* Release allocated resources */
	g_free(domain);

	return(policy);
}

/* Ask user what to do with cookies from domain(s) which were neither marked accepted nor blocked */
static gint _cookie_permission_manager_sort_cookies_by_domain(SoupCookie *inLeft, SoupCookie *inRight)
{
	const gchar		*domainLeft=soup_cookie_get_domain(inLeft);
	const gchar		*domainRight=soup_cookie_get_domain(inRight);

	if(*domainLeft=='.') domainLeft++;
	if(*domainRight=='.') domainRight++;

	return(g_ascii_strcasecmp(domainLeft, domainRight));
}

static GSList* _cookie_permission_manager_get_number_domains_and_cookies(CookiePermissionManager *self,
																			GSList *inCookies,
																			gint *ioNumberDomains,
																			gint *ioNumberCookies)
{
	GSList			*sortedList, *iter;
	gint			domains, cookies;
	const gchar		*lastDomain=NULL;
	const gchar		*cookieDomain;

	/* Make copy and sort cookies in new list */
	sortedList=g_slist_copy(inCookies);

	/* Sort cookies by domain to prevent a doman counted multiple times */
	sortedList=g_slist_sort(sortedList, (GCompareFunc)_cookie_permission_manager_sort_cookies_by_domain);

	/* Iterate through list and count domains and cookies */
	domains=cookies=0;
	for(iter=sortedList; iter; iter=iter->next)
	{
		cookieDomain=soup_cookie_get_domain((SoupCookie*)iter->data);

		if(!lastDomain || g_ascii_strcasecmp(lastDomain, cookieDomain)!=0)
		{
			domains++;
			lastDomain=cookieDomain;
		}

		cookies++;
	}

	/* Store counted numbers to final variables */
	if(ioNumberDomains) *ioNumberDomains=domains;
	if(ioNumberCookies) *ioNumberCookies=cookies;

	/* Return the copied but sorted cookie list. Caller is responsible to free
	 * this list with g_slist_free
	 */
	return(sortedList);
}

/* FIXME: Find a way to add "details" widget */
#ifndef NO_INFOBAR_DETAILS
static void _cookie_permission_manager_when_ask_expander_changed(CookiePermissionManager *self,
																	GParamSpec *inSpec,
																	gpointer inUserData)
{
	GtkExpander			*expander=GTK_EXPANDER(inUserData);

	midori_extension_set_boolean(self->priv->extension, "show-details-when-ask", gtk_expander_get_expanded(expander));
}
#endif

static gboolean _cookie_permission_manager_on_infobar_webview_navigate(WebKitWebView *inView,
																		WebKitWebFrame *inFrame,
																		WebKitNetworkRequest *inRequest,
																		WebKitWebNavigationAction *inAction,
																		WebKitWebPolicyDecision *inDecision,
																		gpointer inUserData)
{
	/* Destroy info bar - that calls another callback which quits main loop */
	GtkWidget		*infobar=GTK_WIDGET(inUserData);

	gtk_widget_destroy(infobar);

	/* Let the default handler decide */
	return(FALSE);
}

static void _cookie_permission_manager_on_infobar_destroy(GtkWidget* inInfobar,
															gpointer inUserData)
{
	CookiePermissionManagerModalInfobar		*modalInfo=(CookiePermissionManagerModalInfobar*)inUserData;

	/* Quit main loop */
	if(g_main_loop_is_running(modalInfo->mainLoop)) g_main_loop_quit(modalInfo->mainLoop);
}

static void _cookie_permission_manager_on_infobar_policy_decision(GtkWidget* inInfobar,
																	gint inResponse,
																	gpointer inUserData)
{
	CookiePermissionManagerModalInfobar		*modalInfo;

	/* Get modal info struct */
	modalInfo=(CookiePermissionManagerModalInfobar*)g_object_get_data(G_OBJECT(inInfobar), "cookie-permission-manager-infobar-data");

	/* Store response */
	modalInfo->response=inResponse;

	/* Quit main loop */
	if(g_main_loop_is_running(modalInfo->mainLoop)) g_main_loop_quit(modalInfo->mainLoop);
}

static gint _cookie_permission_manager_ask_for_policy(CookiePermissionManager *self,
														MidoriView *inView,
														SoupMessage *inMessage,
														GSList *inUnknownCookies)
{
	/* Ask user for policy of unkndown domains in an undistracting way.
	 * The idea is to put the message not in a modal window but into midori's info bar.
	 * Then we'll set up our own GMainLoop to simulate a modal info bar. We need to
	 * connect to all possible signals of info bar, web view and so on to handle user's
	 * decision and to get out of our own GMainLoop. After that webkit resumes processing
	 * data.
	 */
	CookiePermissionManagerPrivate			*priv=self->priv;
	GtkWidget								*infobar;
/* FIXME: Find a way to add "details" widget */
#ifndef NO_INFOBAR_DETAILS
	GtkWidget								*widget;
	GtkWidget								*contentArea;
	GtkWidget								*vbox, *hbox;
	GtkWidget								*expander;
	GtkListStore							*listStore;
	GtkTreeIter								listIter;
	GtkWidget								*scrolled;
	GtkWidget								*list;
	GtkCellRenderer							*renderer;
	GtkTreeViewColumn						*column;
#endif
	gchar									*text;
	gint									numberDomains, numberCookies;
	GSList									*sortedCookies, *cookies;
	WebKitWebView							*webkitView;
	CookiePermissionManagerModalInfobar		*modalInfo;

	/* Get webkit view of midori view */
	webkitView=WEBKIT_WEB_VIEW(midori_view_get_web_view(inView));
	modalInfo=g_new0(CookiePermissionManagerModalInfobar, 1);

	/* Create a copy of cookies and sort them */
	sortedCookies=_cookie_permission_manager_get_number_domains_and_cookies(self,
																			inUnknownCookies,
																			&numberDomains,
																			&numberCookies);

/* FIXME: Find a way to add "details" widget */
#ifndef NO_INFOBAR_DETAILS
	/* Create list model and fill in data */
	listStore=gtk_list_store_new(N_COLUMN,
									G_TYPE_STRING,	/* DOMAIN_COLUMN */
									G_TYPE_STRING,	/* PATH_COLUMN */
									G_TYPE_STRING,	/* NAME_COLUMN */
									G_TYPE_STRING,	/* VALUE_COLUMN */
									G_TYPE_STRING	/* EXPIRE_DATE_COLUMN */);

	for(cookies=sortedCookies; cookies; cookies=cookies->next)
	{
		SoupCookie				*cookie=(SoupCookie*)cookies->data;
		SoupDate				*cookieDate=soup_cookie_get_expires(cookie);

		if(cookieDate) text=soup_date_to_string(cookieDate, SOUP_DATE_HTTP);
			else text=g_strdup(_("Till session end"));

		gtk_list_store_append(listStore, &listIter);
		gtk_list_store_set(listStore,
							&listIter,
							DOMAIN_COLUMN, soup_cookie_get_domain(cookie),
							PATH_COLUMN, soup_cookie_get_path(cookie),
							NAME_COLUMN, soup_cookie_get_name(cookie),
							VALUE_COLUMN, soup_cookie_get_value(cookie),
							EXPIRE_DATE_COLUMN, text,
							-1);

		g_free(text);
	}
#endif

	/* Create description text */
	if(numberDomains==1)
	{
		const gchar					*cookieDomain=soup_cookie_get_domain((SoupCookie*)sortedCookies->data);

		if(*cookieDomain=='.') cookieDomain++;
		
		if(numberCookies>1)
			text=g_strdup_printf(_("The website %s wants to store %d cookies."), cookieDomain, numberCookies);
		else
			text=g_strdup_printf(_("The website %s wants to store a cookie."), cookieDomain);
	}
		else
		{
			text=g_strdup_printf(_("Multiple websites want to store %d cookies in total."), numberCookies);
		}

	/* Create info bar message and buttons */
	infobar=midori_view_add_info_bar(inView,
										GTK_MESSAGE_QUESTION,
										text,
										G_CALLBACK(_cookie_permission_manager_on_infobar_policy_decision),
										NULL,
										_("_Accept"), COOKIE_PERMISSION_MANAGER_POLICY_ACCEPT,
										_("Accept for this _session"), COOKIE_PERMISSION_MANAGER_POLICY_ACCEPT_FOR_SESSION,
										_("De_ny"), COOKIE_PERMISSION_MANAGER_POLICY_BLOCK,
										_("Deny _this time"), COOKIE_PERMISSION_MANAGER_POLICY_UNDETERMINED,
										NULL);
	g_free(text);

	/* midori_view_add_info_bar() in version 0.4.8 expects a GObject as user data
	 * but I don't want to create an GObject just for a simple struct. So set object
	 * data by our own
	 */
	g_object_set_data_full(G_OBJECT(infobar), "cookie-permission-manager-infobar-data", modalInfo, (GDestroyNotify)g_free);

/* FIXME: Find a way to add "details" widget */
#ifndef NO_INFOBAR_DETAILS
	/* Get content area of infobar */
	contentArea=gtk_info_bar_get_content_area(GTK_INFO_BAR(infobar));

	/* Create list and set up columns of list */
	list=gtk_tree_view_new_with_model(GTK_TREE_MODEL(listStore));
#ifndef HAVE_GTK3
	gtk_widget_set_size_request(list, -1, 100);
#endif

	renderer=gtk_cell_renderer_text_new();
	column=gtk_tree_view_column_new_with_attributes(_("Domain"),
													renderer,
													"text", DOMAIN_COLUMN,
													NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

	renderer=gtk_cell_renderer_text_new();
	column=gtk_tree_view_column_new_with_attributes(_("Path"),
													renderer,
													"text", PATH_COLUMN,
													NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

	renderer=gtk_cell_renderer_text_new();
	column=gtk_tree_view_column_new_with_attributes(_("Name"),
													renderer,
													"text", NAME_COLUMN,
													NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

	renderer=gtk_cell_renderer_text_new();
	column=gtk_tree_view_column_new_with_attributes(_("Value"),
													renderer,
													"text", VALUE_COLUMN,
													NULL);
	g_object_set(G_OBJECT(renderer),
					"ellipsize", PANGO_ELLIPSIZE_END,
					"width-chars", 30,
					NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

	renderer=gtk_cell_renderer_text_new();
	column=gtk_tree_view_column_new_with_attributes(_("Expire date"),
													renderer,
													"text", EXPIRE_DATE_COLUMN,
													NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

	scrolled=gtk_scrolled_window_new(NULL, NULL);
#ifdef HAVE_GTK3
	gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 100);
#endif
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolled), list);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(expander), scrolled);

	gtk_widget_show_all(vbox);
	gtk_container_add(GTK_CONTAINER(contentArea), vbox);

	/* Set state of expander based on config 'show-details-when-ask' */
	gtk_expander_set_expanded(GTK_EXPANDER(expander),
								midori_extension_get_boolean(priv->extension, "show-details-when-ask"));
	g_signal_connect_swapped(expander, "notify::expanded", G_CALLBACK(_cookie_permission_manager_when_ask_expander_changed), self);
#endif

	/* Show all widgets of info bar */
	gtk_widget_show_all(infobar);

	/* Connect signals to quit main loop */
	g_signal_connect(webkitView, "navigation-policy-decision-requested", G_CALLBACK(_cookie_permission_manager_on_infobar_webview_navigate), infobar);
	g_signal_connect(infobar, "destroy", G_CALLBACK(_cookie_permission_manager_on_infobar_destroy), modalInfo);

	/* Let info bar be modal and set response to default */
	modalInfo->response=COOKIE_PERMISSION_MANAGER_POLICY_UNDETERMINED;
	modalInfo->mainLoop=g_main_loop_new(NULL, FALSE);

	GDK_THREADS_LEAVE();
	g_main_loop_run(modalInfo->mainLoop);
	GDK_THREADS_ENTER();

	g_main_loop_unref(modalInfo->mainLoop);

	modalInfo->mainLoop=NULL;

	/* Disconnect signal handler to webkit's web view  */
	g_signal_handlers_disconnect_by_func(webkitView, G_CALLBACK(_cookie_permission_manager_on_infobar_webview_navigate), infobar);

	/* Store user's decision in database if it is not a temporary block.
	 * We use the already sorted list of cookies to prevent multiple
	 * updates of database for the same domain. This sorted list is a copy
	 * to avoid a reorder of cookies
	 */
	if(modalInfo->response!=COOKIE_PERMISSION_MANAGER_POLICY_UNDETERMINED)
	{
		const gchar					*lastDomain=NULL;

		/* Iterate through cookies and store decision for each domain once */
		for(cookies=sortedCookies; cookies; cookies=cookies->next)
		{
			SoupCookie				*cookie=(SoupCookie*)cookies->data;
			const gchar				*cookieDomain=soup_cookie_get_domain(cookie);

			if(*cookieDomain=='.') cookieDomain++;

			/* Store decision if new domain found while iterating through cookies */
			if(!lastDomain || g_ascii_strcasecmp(lastDomain, cookieDomain)!=0)
			{
				gchar	*sql;
				gchar	*error=NULL;
				gint	success;

				sql=sqlite3_mprintf("INSERT OR REPLACE INTO policies (domain, value) VALUES ('%q', %d);",
										cookieDomain,
										modalInfo->response);
				success=sqlite3_exec(priv->database, sql, NULL, NULL, &error);
				if(success!=SQLITE_OK) g_warning(_("SQL fails: %s"), error);
				if(error) sqlite3_free(error);
				sqlite3_free(sql);

				lastDomain=cookieDomain;
			}
		}
	}

	/* Free up allocated resources */
	g_slist_free(sortedCookies);

	/* Return response */
	return(modalInfo->response==COOKIE_PERMISSION_MANAGER_POLICY_UNDETERMINED ?
			COOKIE_PERMISSION_MANAGER_POLICY_BLOCK : modalInfo->response);
}

/* A cookie was changed outside a request (e.g. Javascript) */
static void _cookie_permission_manager_on_cookie_changed(CookiePermissionManager *self,
															SoupCookie *inOldCookie,
															SoupCookie *inNewCookie,
															SoupCookieJar *inCookieJar)
{
	/* Do not check changed cookies because they must have been allowed before.
	 * Also do not check removed cookies because they are removed ;)
	 */
	if(inNewCookie==NULL || inOldCookie) return;

	/* New cookie is a new cookie so check */
	switch(_cookie_permission_manager_get_policy(self, inNewCookie))
	{
		case COOKIE_PERMISSION_MANAGER_POLICY_ACCEPT:
		case COOKIE_PERMISSION_MANAGER_POLICY_ACCEPT_FOR_SESSION:
			break;

		case COOKIE_PERMISSION_MANAGER_POLICY_UNDETERMINED:
			/* Fallthrough!
			 * The problem here is that we don't know the view to ask user
			 * for policy to follow for this cookie domain. Therefore we
			 * delete the cookie from jar and assume that we will be asked
			 * again in _cookie_permission_manager_on_response_received().
			 */

		default:
			soup_cookie_jar_delete_cookie(inCookieJar, inNewCookie);
			break;
	}
}

/* We received the HTTP headers of the request and it contains cookie-managing headers */
static void _cookie_permission_manager_on_response_received(WebKitWebView *inView,
															WebKitWebFrame *inFrame,
															WebKitWebResource *inResource,
															WebKitNetworkResponse *inResponse,
															gpointer inUserData)
{
	g_return_if_fail(IS_COOKIE_PERMISSION_MANAGER(inUserData));

	CookiePermissionManager			*self=COOKIE_PERMISSION_MANAGER(inUserData);
	CookiePermissionManagerPrivate	*priv=self->priv;
	GSList							*newCookies, *cookie;
	GSList							*unknownCookies=NULL, *acceptedCookies=NULL;
	SoupURI							*firstParty;
	SoupCookieJarAcceptPolicy		cookiePolicy;
	gint							unknownCookiesPolicy;
	SoupMessage						*message;

	/* If policy is to deny all cookies return immediately */
	cookiePolicy=soup_cookie_jar_get_accept_policy(priv->cookieJar);
	if(cookiePolicy==SOUP_COOKIE_JAR_ACCEPT_NEVER) return;

	/* Get SoupMessage */
	message=webkit_network_response_get_message(inResponse);
	if(!message || !SOUP_IS_MESSAGE(message)) return;

	/* Iterate through cookies in response and check if they should be
	 * blocked (remove from cookies list) or accepted (added to cookie jar).
	 * If we could not determine what to do collect these cookies and
	 * ask user
	 */
	newCookies=soup_cookies_from_response(message);
	firstParty=soup_message_get_first_party(message);
	for(cookie=newCookies; cookie; cookie=cookie->next)
	{
		switch(_cookie_permission_manager_get_policy(self, cookie->data))
		{
			case COOKIE_PERMISSION_MANAGER_POLICY_BLOCK:
				soup_cookie_free(cookie->data);
				break;

			case COOKIE_PERMISSION_MANAGER_POLICY_ACCEPT:
			case COOKIE_PERMISSION_MANAGER_POLICY_ACCEPT_FOR_SESSION:
				if((cookiePolicy==SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY &&
						firstParty!=NULL &&
						firstParty->host &&
						soup_cookie_domain_matches(cookie->data, firstParty->host)) ||
						cookiePolicy==SOUP_COOKIE_JAR_ACCEPT_ALWAYS)
				{
					acceptedCookies=g_slist_prepend(acceptedCookies, cookie->data);
				}
					else soup_cookie_free(cookie->data);
				break;

			case COOKIE_PERMISSION_MANAGER_POLICY_UNDETERMINED:
			default:
				if((cookiePolicy==SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY &&
						firstParty!=NULL &&
						firstParty->host &&
						soup_cookie_domain_matches(cookie->data, firstParty->host)) ||
						cookiePolicy==SOUP_COOKIE_JAR_ACCEPT_ALWAYS)
				{
					unknownCookies=g_slist_prepend(unknownCookies, cookie->data);
				}
					else soup_cookie_free(cookie->data);
				break;
		}
	}

	/* Prepending an item to list is the fastest method but the order of cookies
	 * is reversed now and may be added to cookie jar in the wrong order. So we
	 * need to reverse list now of both - undetermined and accepted cookies
	 */
	unknownCookies=g_slist_reverse(unknownCookies);
	acceptedCookies=g_slist_reverse(acceptedCookies);

	/* Ask user for his decision what to do with cookies whose policy is undetermined
	 * But only ask if there is any undetermined one
	 */
	if(g_slist_length(unknownCookies)>0)
	{
		/* Get view */
		MidoriView					*view;

		view=MIDORI_VIEW(g_object_get_data(G_OBJECT(inView), "midori-view"));

		/* Ask for user's decision */
		unknownCookiesPolicy=_cookie_permission_manager_ask_for_policy(self, view, message, unknownCookies);
		if(unknownCookiesPolicy==COOKIE_PERMISSION_MANAGER_POLICY_ACCEPT ||
			unknownCookiesPolicy==COOKIE_PERMISSION_MANAGER_POLICY_ACCEPT_FOR_SESSION)
		{
			/* Add accepted undetermined cookies to cookie jar */
			for(cookie=unknownCookies; cookie; cookie=cookie->next)
			{
				soup_cookie_jar_add_cookie(priv->cookieJar, (SoupCookie*)cookie->data);
			}
		}
			else
			{
				/* Free cookies because they should be blocked */
				for(cookie=unknownCookies; cookie; cookie=cookie->next)
				{
					soup_cookie_free((SoupCookie*)cookie->data);
				}
			}
	}

	/* Add accepted cookies to cookie jar */
	for(cookie=acceptedCookies; cookie; cookie=cookie->next)
	{
		soup_cookie_jar_add_cookie(priv->cookieJar, (SoupCookie*)cookie->data);
	}

	/* Free list of cookies */
	g_slist_free(unknownCookies);
	g_slist_free(acceptedCookies);
	g_slist_free(newCookies);
}

/* A tab to a browser was added */
static void _cookie_permission_manager_on_add_tab(CookiePermissionManager *self, MidoriView *inView, gpointer inUserData)
{
	/* Listen to starting network requests */
	WebKitWebView	*webkitView=WEBKIT_WEB_VIEW(midori_view_get_web_view(inView));

	g_object_set_data(G_OBJECT(webkitView), "midori-view", inView);
	g_signal_connect(webkitView, "resource-response-received", G_CALLBACK(_cookie_permission_manager_on_response_received), self);
}

/* A browser window was added */
static void _cookie_permission_manager_on_add_browser(CookiePermissionManager *self,
														MidoriBrowser *inBrowser,
														gpointer inUserData)
{
	GList	*tabs, *iter;

	/* Set up all current available tabs in browser */
	tabs=midori_browser_get_tabs(inBrowser);
	for(iter=tabs; iter; iter=g_list_next(iter))
	{
		_cookie_permission_manager_on_add_tab(self, iter->data, inBrowser);
	}
	g_list_free(tabs);

	/* Listen to new tabs opened in browser and existing ones closed */
	g_signal_connect_swapped(inBrowser, "add-tab", G_CALLBACK(_cookie_permission_manager_on_add_tab), self);
}

/* Application property has changed */
static void _cookie_permission_manager_on_application_changed(CookiePermissionManager *self)
{
	CookiePermissionManagerPrivate		*priv=COOKIE_PERMISSION_MANAGER(self)->priv;
	GList								*browsers, *iter;

	/* Set up all current open browser windows */
	browsers=midori_app_get_browsers(priv->application);
	for(iter=browsers; iter; iter=g_list_next(iter))
	{
		_cookie_permission_manager_on_add_browser(self, MIDORI_BROWSER(iter->data), priv->application);
	}
	g_list_free(browsers);

	/* Listen to new browser windows opened and existing ones closed */
	g_signal_connect_swapped(priv->application, "add-browser", G_CALLBACK(_cookie_permission_manager_on_add_browser), self);
}

/* IMPLEMENTATION: GObject */

/* Finalize this object */
static void cookie_permission_manager_finalize(GObject *inObject)
{
	CookiePermissionManager			*self=COOKIE_PERMISSION_MANAGER(inObject);
	CookiePermissionManagerPrivate	*priv=self->priv;
	GList							*browsers, *browser;
	GList							*tabs, *tab;
	WebKitWebView					*webkitView;

	/* Dispose allocated resources */
	if(priv->databaseFilename)
	{
		g_free(priv->databaseFilename);
		priv->databaseFilename=NULL;
		g_object_notify_by_pspec(inObject, CookiePermissionManagerProperties[PROP_DATABASE_FILENAME]);
	}

	if(priv->database)
	{
		sqlite3_close(priv->database);
		priv->database=NULL;
		g_object_notify_by_pspec(inObject, CookiePermissionManagerProperties[PROP_DATABASE]);
	}

	g_signal_handler_disconnect(priv->cookieJar, priv->cookieJarChangedID);
	g_object_steal_data(G_OBJECT(priv->cookieJar), "cookie-permission-manager");

	g_signal_handlers_disconnect_by_data(priv->application, self);

	browsers=midori_app_get_browsers(priv->application);
	for(browser=browsers; browser; browser=g_list_next(browser))
	{
		g_signal_handlers_disconnect_by_data(browser->data, self);

		tabs=midori_browser_get_tabs(MIDORI_BROWSER(browser->data));
		for(tab=tabs; tab; tab=g_list_next(tab))
		{
			webkitView=WEBKIT_WEB_VIEW(midori_view_get_web_view(MIDORI_VIEW(tab->data)));
			g_signal_handlers_disconnect_by_data(webkitView, self);
		}
		g_list_free(tabs);
	}
	g_list_free(browsers);

	/* Call parent's class finalize method */
	G_OBJECT_CLASS(cookie_permission_manager_parent_class)->finalize(inObject);
}

/* Set/get properties */
static void cookie_permission_manager_set_property(GObject *inObject,
													guint inPropID,
													const GValue *inValue,
													GParamSpec *inSpec)
{
	CookiePermissionManager		*self=COOKIE_PERMISSION_MANAGER(inObject);
	
	switch(inPropID)
	{
		/* Construct-only properties */
		case PROP_EXTENSION:
			self->priv->extension=g_value_get_object(inValue);
			_cookie_permission_manager_open_database(self);
			break;

		case PROP_APPLICATION:
			self->priv->application=g_value_get_object(inValue);
			_cookie_permission_manager_on_application_changed(self);
			break;

		case PROP_UNKNOWN_POLICY:
			cookie_permission_manager_set_unknown_policy(self, g_value_get_int(inValue));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(inObject, inPropID, inSpec);
			break;
	}
}

static void cookie_permission_manager_get_property(GObject *inObject,
													guint inPropID,
													GValue *outValue,
													GParamSpec *inSpec)
{
	CookiePermissionManager		*self=COOKIE_PERMISSION_MANAGER(inObject);

	switch(inPropID)
	{
		case PROP_EXTENSION:
			g_value_set_object(outValue, self->priv->extension);
			break;

		case PROP_APPLICATION:
			g_value_set_object(outValue, self->priv->application);
			break;

		case PROP_DATABASE:
			g_value_set_pointer(outValue, self->priv->database);
			break;

		case PROP_DATABASE_FILENAME:
			g_value_set_string(outValue, self->priv->databaseFilename);
			break;

		case PROP_UNKNOWN_POLICY:
			g_value_set_int(outValue, self->priv->unknownPolicy);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(inObject, inPropID, inSpec);
			break;
	}
}

/* Class initialization
 * Override functions in parent classes and define properties and signals
 */
static void cookie_permission_manager_class_init(CookiePermissionManagerClass *klass)
{
	GObjectClass		*gobjectClass=G_OBJECT_CLASS(klass);

	/* Override functions */
	gobjectClass->finalize=cookie_permission_manager_finalize;
	gobjectClass->set_property=cookie_permission_manager_set_property;
	gobjectClass->get_property=cookie_permission_manager_get_property;

	/* Set up private structure */
	g_type_class_add_private(klass, sizeof(CookiePermissionManagerPrivate));

	/* Define properties */
	CookiePermissionManagerProperties[PROP_EXTENSION]=
		g_param_spec_object("extension",
								_("Extension instance"),
								_("The Midori extension instance for this extension"),
								MIDORI_TYPE_EXTENSION,
								G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	CookiePermissionManagerProperties[PROP_APPLICATION]=
		g_param_spec_object("application",
								_("Application instance"),
								_("The Midori application instance this extension belongs to"),
								MIDORI_TYPE_APP,
								G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	CookiePermissionManagerProperties[PROP_DATABASE]=
		g_param_spec_pointer("database",
								_("Database instance"),
								_("Pointer to sqlite database instance used by this extension"),
								G_PARAM_READABLE);

	CookiePermissionManagerProperties[PROP_DATABASE_FILENAME]=
		g_param_spec_string("database-filename",
								_("Database path"),
								_("Path to sqlite database instance used by this extension"),
								NULL,
								G_PARAM_READABLE);

	CookiePermissionManagerProperties[PROP_UNKNOWN_POLICY]=
		g_param_spec_int("unknown-policy",
								_("Unknown domain policy"),
								_("The policy to use for domains not individually configured."
								  " This only acts to further restrict the global cookie policy set in Midori settings."),
								COOKIE_PERMISSION_MANAGER_POLICY_UNDETERMINED,
								COOKIE_PERMISSION_MANAGER_POLICY_BLOCK,
								COOKIE_PERMISSION_MANAGER_POLICY_UNDETERMINED,
								G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties(gobjectClass, PROP_LAST, CookiePermissionManagerProperties);
}

/* Object initialization
 * Create private structure and set up default values
 */
static void cookie_permission_manager_init(CookiePermissionManager *self)
{
	CookiePermissionManagerPrivate	*priv;

	priv=self->priv=COOKIE_PERMISSION_MANAGER_GET_PRIVATE(self);

	/* Set up default values */
	priv->database=NULL;
	priv->databaseFilename=NULL;
	priv->unknownPolicy=COOKIE_PERMISSION_MANAGER_POLICY_UNDETERMINED;

	/* Hijack session's cookie jar to handle cookies requests on our own in HTTP streams
	 * but remember old handlers to restore them on deactivation
	 */
	priv->session=webkit_get_default_session();
	priv->cookieJar=SOUP_COOKIE_JAR(soup_session_get_feature(priv->session, SOUP_TYPE_COOKIE_JAR));
	priv->featureIface=SOUP_SESSION_FEATURE_GET_CLASS(priv->cookieJar);
	g_object_set_data(G_OBJECT(priv->cookieJar), "cookie-permission-manager", self);

	/* Listen to changed cookies set or changed by other sources like javascript */
	priv->cookieJarChangedID=g_signal_connect_swapped(priv->cookieJar, "changed", G_CALLBACK(_cookie_permission_manager_on_cookie_changed), self);
}

/* Implementation: Public API */

/* Create new object */
CookiePermissionManager* cookie_permission_manager_new(MidoriExtension *inExtension, MidoriApp *inApp)
{
	return(g_object_new(TYPE_COOKIE_PERMISSION_MANAGER,
							"extension", inExtension,
							"application", inApp,
							NULL));
}

/* Get/set policy to ask for policy if unknown for a domain */
CookiePermissionManagerPolicy cookie_permission_manager_get_unknown_policy(CookiePermissionManager *self)
{
	g_return_val_if_fail(IS_COOKIE_PERMISSION_MANAGER(self), COOKIE_PERMISSION_MANAGER_POLICY_UNDETERMINED);

	return(self->priv->unknownPolicy);
}

void cookie_permission_manager_set_unknown_policy(CookiePermissionManager *self, CookiePermissionManagerPolicy inPolicy)
{
	g_return_if_fail(IS_COOKIE_PERMISSION_MANAGER(self));

	if(inPolicy!=self->priv->unknownPolicy)
	{
		self->priv->unknownPolicy=inPolicy;
		midori_extension_set_integer(self->priv->extension, "unknown-policy", inPolicy);
		g_object_notify_by_pspec(G_OBJECT(self), CookiePermissionManagerProperties[PROP_UNKNOWN_POLICY]);
	}
}

/************************************************************************************/

/* Implementation: Enumeration */
GType cookie_permission_manager_policy_get_type(void)
{
	static volatile gsize	g_define_type_id__volatile=0;

	if(g_once_init_enter(&g_define_type_id__volatile))
	{
		static const GEnumValue values[]=
		{
			{ COOKIE_PERMISSION_MANAGER_POLICY_UNDETERMINED, "COOKIE_PERMISSION_MANAGER_POLICY_UNDETERMINED", N_("Undetermined") },
			{ COOKIE_PERMISSION_MANAGER_POLICY_ACCEPT, "COOKIE_PERMISSION_MANAGER_POLICY_ACCEPT", N_("Accept") },
			{ COOKIE_PERMISSION_MANAGER_POLICY_ACCEPT_FOR_SESSION, "COOKIE_PERMISSION_MANAGER_POLICY_ACCEPT_FOR_SESSION", N_("Accept for session") },
			{ COOKIE_PERMISSION_MANAGER_POLICY_BLOCK, "COOKIE_PERMISSION_MANAGER_POLICY_BLOCK", N_("Block") },
			{ 0, NULL, NULL }
		};

		GType	g_define_type_id=g_enum_register_static(g_intern_static_string("CookiePermissionManagerPolicy"), values);
		g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
	}

	return(g_define_type_id__volatile);
}
