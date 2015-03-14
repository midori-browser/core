/*
 Copyright (C) 2013 Stephan Haller <nomad@froevel.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "nojs.h"
#include "nojs-view.h"

#include <errno.h>

/* Define this class in GObject system */
G_DEFINE_TYPE(NoJS,
				nojs,
				G_TYPE_OBJECT)

/* Properties */
enum
{
	PROP_0,

	PROP_EXTENSION,
	PROP_APPLICATION,

	PROP_DATABASE,
	PROP_DATABASE_FILENAME,
	PROP_ALLOW_LOCAL_PAGES,
	PROP_ONLY_SECOND_LEVEL,
	PROP_UNKNOWN_DOMAIN_POLICY,

	PROP_LAST
};

static GParamSpec* NoJSProperties[PROP_LAST]={ 0, };

/* Signals */
enum
{
	URI_LOAD_POLICY_STATUS,
	POLICY_CHANGED,

	SIGNAL_LAST
};

static guint NoJSSignals[SIGNAL_LAST]={ 0, };

/* Private structure - access only by public API if needed */
#define NOJS_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), TYPE_NOJS, NoJSPrivate))

struct _NoJSPrivate
{
	/* Extension related */
	MidoriExtension					*extension;
	MidoriApp						*application;
	sqlite3							*database;
	gchar							*databaseFilename;
	gboolean						allowLocalPages;
	gboolean						checkOnlySecondLevel;
	NoJSPolicy						unknownDomainPolicy;

	guint							requestStartedSignalID;
};

/* Taken from http://www.w3.org/html/wg/drafts/html/master/scripting-1.html#scriptingLanguages
 * A list of javascript mime types
 */
static const gchar*				javascriptTypes[]=	{
														"application/ecmascript",
														"application/javascript",
														"application/x-ecmascript",
														"application/x-javascript",
														"text/ecmascript",
														"text/javascript",
														"text/javascript1.0",
														"text/javascript1.1",
														"text/javascript1.2",
														"text/javascript1.3",
														"text/javascript1.4",
														"text/javascript1.5",
														"text/jscript",
														"text/livescript",
														"text/x-ecmascript",
														"text/x-javascript",
														NULL
													};

/* IMPLEMENTATION: Private variables and methods */

/* Closure for: void (*closure)(NoJS *self, gchar *inURI, NoJSPolicy inPolicy) */
static void _nojs_closure_VOID__STRING_ENUM(GClosure *inClosure,
											GValue *ioReturnValue G_GNUC_UNUSED,
											guint inNumberValues,
											const GValue *inValues,
											gpointer inInvocationHint G_GNUC_UNUSED,
											gpointer inMarshalData)
{
	typedef void (*GMarshalFunc_VOID__STRING_ENUM)(gpointer inObject, gpointer inArg1, gint inArg2, gpointer inUserData);

	register GMarshalFunc_VOID__STRING_ENUM		callback;
	register GCClosure							*closure=(GCClosure*)inClosure;
	register gpointer							object, userData;

	g_return_if_fail(inNumberValues==3);

	if(G_CCLOSURE_SWAP_DATA(inClosure))
	{
		object=inClosure->data;
		userData=g_value_peek_pointer(inValues+0);
	}
		else
		{
			object=g_value_peek_pointer(inValues+0);
			userData=inClosure->data;
		}

	callback=(GMarshalFunc_VOID__STRING_ENUM)(inMarshalData ? inMarshalData : closure->callback);

	callback(object,
				(gchar*)g_value_get_string(inValues+1),
				g_value_get_enum(inValues+2),
				userData);
}

/* Show common error dialog */
static void _nojs_error(NoJS *self, const gchar *inReason)
{
	g_return_if_fail(IS_NOJS(self));
	g_return_if_fail(inReason);

	GtkWidget		*dialog;

	/* Show confirmation dialog for undetermined cookies */
	dialog=gtk_message_dialog_new(NULL,
									GTK_DIALOG_MODAL,
									GTK_MESSAGE_ERROR,
									GTK_BUTTONS_OK,
									_("A fatal error occurred which prevents "
									  "the NoJS extension to continue. "
									  "You should disable it."));

	gtk_window_set_title(GTK_WINDOW(dialog), _("Error in NoJS extension"));
	gtk_window_set_icon_name(GTK_WINDOW (dialog), "midori");

	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
												"%s:\n%s",
												_("Reason"),
												inReason);

	gtk_dialog_run(GTK_DIALOG(dialog));

	/* Free up allocated resources */
	gtk_widget_destroy(dialog);
}

/* Open database containing policies for javascript sites.
 * Create database and setup table structure if it does not exist yet.
 */
static void _nojs_open_database(NoJS *self)
{
	g_return_if_fail(IS_NOJS(self));

	NoJSPrivate		*priv=self->priv;
	const gchar		*configDir;
	gchar			*sql;
	gchar			*error=NULL;
	gint			success;

	/* Close any open database */
	if(priv->database)
	{
		priv->databaseFilename=NULL;

		sqlite3_close(priv->database);
		priv->database=NULL;

		g_object_notify_by_pspec(G_OBJECT(self), NoJSProperties[PROP_DATABASE]);
		g_object_notify_by_pspec(G_OBJECT(self), NoJSProperties[PROP_DATABASE_FILENAME]);
	}

	/* Build path to database file */
	configDir=midori_extension_get_config_dir(priv->extension);
	if(!configDir)
		return;
	
	if(katze_mkdir_with_parents(configDir, 0700))
	{
		g_warning(_("Could not create configuration folder for extension: %s"), g_strerror(errno));

		_nojs_error(self, _("Could not create configuration folder for extension."));
		return;
	}

	/* Open database */
	priv->databaseFilename=g_build_filename(configDir, NOJS_DATABASE, NULL);
	success=sqlite3_open(priv->databaseFilename, &priv->database);
	if(success!=SQLITE_OK)
	{
		g_warning(_("Could not open database of extension: %s"), sqlite3_errmsg(priv->database));

		g_free(priv->databaseFilename);
		priv->databaseFilename=NULL;

		if(priv->database) sqlite3_close(priv->database);
		priv->database=NULL;

		_nojs_error(self, _("Could not open database of extension."));
		return;
	}

	/* Create table structure if it does not exist */
	success=sqlite3_exec(priv->database,
							"CREATE TABLE IF NOT EXISTS "
							"policies(site text, value integer);",
							NULL,
							NULL,
							&error);

	if(success==SQLITE_OK)
	{
		success=sqlite3_exec(priv->database,
								"CREATE UNIQUE INDEX IF NOT EXISTS "
								"site ON policies (site);",
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
		_nojs_error(self, _("Could not set up database structure of extension."));

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

	/* Delete all temporarily allowed sites */
	sql=sqlite3_mprintf("DELETE FROM policies WHERE value=%d;", NOJS_POLICY_ACCEPT_TEMPORARILY);
	success=sqlite3_exec(priv->database, sql, NULL, NULL, &error);
	if(success!=SQLITE_OK) g_warning(_("SQL fails: %s"), error);
	if(error) sqlite3_free(error);
	sqlite3_free(sql);

	g_object_notify_by_pspec(G_OBJECT(self), NoJSProperties[PROP_DATABASE]);
	g_object_notify_by_pspec(G_OBJECT(self), NoJSProperties[PROP_DATABASE_FILENAME]);
}

/* A request through libsoup is going to start and http headers must be
 * checked for content type
 */
static void _nojs_on_got_headers(NoJS *self, gpointer inUserData)
{
	g_return_if_fail(IS_NOJS(self));
	g_return_if_fail(SOUP_IS_MESSAGE(inUserData));

	NoJSPrivate				*priv=self->priv;
	SoupMessage				*message=SOUP_MESSAGE(inUserData);
	SoupSession				*session=webkit_get_default_session();
	SoupMessageHeaders		*headers;
	SoupMessageBody			*body;
	const gchar				*contentType;
	SoupURI					*uri;
	gchar					*uriText;
	NoJSPolicy				policy;
	gboolean				isJS;
	const gchar				**iter;

	/* Get headers from message to retrieve content type */
	g_object_get(message, "response-headers", &headers, NULL);
	if(!headers)
	{
		g_warning("Could not get headers from message to check for javascript.");
		return;
	}

	/* Get content type of uri and check if it is a javascript resource */
	contentType=soup_message_headers_get_content_type(headers, NULL);

	isJS=FALSE;
	iter=javascriptTypes;
	while(*iter && !isJS)
	{
		isJS=(g_strcmp0(contentType, *iter)==0);
		iter++;
	}

	if(!isJS) return;

	/* The document being loaded is javascript so get URI from message,
	 * get policy for domain of URI and emit signal
	 */
	uri=soup_message_get_uri(message);
	policy=nojs_get_policy(self, uri);

	if(policy==NOJS_POLICY_UNDETERMINED)
	{
		g_warning("Got invalid policy. Using default policy for unknown domains.");
		policy=priv->unknownDomainPolicy;
	}

	uriText=soup_uri_to_string(uri, FALSE);

	g_signal_emit(self, NoJSSignals[URI_LOAD_POLICY_STATUS], 0, uriText, policy==NOJS_POLICY_UNDETERMINED ? NOJS_POLICY_BLOCK : policy);

	g_free(uriText);

	/* Return here if policy is any type of accept */
	if(policy!=NOJS_POLICY_UNDETERMINED && policy!=NOJS_POLICY_BLOCK) return;

	/* Cancel this message */
	soup_session_cancel_message(session, message, SOUP_STATUS_CANCELLED);

	/* Discard any load data */
	g_object_get(message, "response-body", &body, NULL);
	if(body) soup_message_body_truncate(body);
}

static void _nojs_on_request_started(NoJS *self,
										SoupMessage *inMessage,
										SoupSocket *inSocket,
										gpointer inUserData)
{
	g_return_if_fail(IS_NOJS(self));
	g_return_if_fail(SOUP_IS_MESSAGE(inMessage));

	/* Connect to "got-headers" to cancel loading javascript documents early */
	g_signal_connect_swapped(inMessage, "got-headers", G_CALLBACK(_nojs_on_got_headers), self);
}

/* The icon in statusbar was clicked */
static void _nojs_on_statusbar_icon_clicked(MidoriBrowser *inBrowser, gpointer inUserData)
{
	g_return_if_fail(MIDORI_IS_BROWSER(inBrowser));

	MidoriView		*activeView;
	NoJSView		*view;
	GtkMenu			*menu;

	/* Get current active midori view */
	activeView=MIDORI_VIEW(midori_browser_get_current_tab(inBrowser));
	g_return_if_fail(MIDORI_IS_VIEW(activeView));

	/* Get NoJS view of current active midori view */
	view=NOJS_VIEW(g_object_get_data(G_OBJECT(activeView), "nojs-view-instance"));
	g_return_if_fail(NOJS_IS_VIEW(view));

	/* Get menu of current view */
	menu=nojs_view_get_menu(view);
	g_return_if_fail(menu);

	/* Show menu */
	gtk_menu_popup(menu, NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
}

gchar* nojs_get_icon_path (const gchar* icon)
{
    gchar* nojs_dir = midori_paths_get_res_filename("nojs");
    return g_build_filename (nojs_dir, icon, NULL);
}

/* Menu icon of a view has changed */
static void _nojs_on_menu_icon_changed(MidoriBrowser *inBrowser, GParamSpec *inSpec, gpointer inUserData)
{
	g_return_if_fail(MIDORI_IS_BROWSER(inBrowser));
	g_return_if_fail(NOJS_IS_VIEW(inUserData));

	NoJSView			*view=NOJS_VIEW(inUserData);
	NoJSMenuIconState	menuIconState;
	GtkWidget			*statusbarIcon;
	GtkWidget			*buttonImage;
	gchar				*imageFilename;

	/* Get icon in status bar of this browser */
	statusbarIcon=GTK_WIDGET(g_object_get_data(G_OBJECT(inBrowser), "nojs-statusicon"));
	g_return_if_fail(GTK_IS_WIDGET(statusbarIcon));

	/* Get menu icon state of view */
	menuIconState=nojs_view_get_menu_icon_state(view);

	/* Create image for statusbar button */
	imageFilename=NULL;
	switch(menuIconState)
	{
		case NOJS_MENU_ICON_STATE_ALLOWED:
			imageFilename=nojs_get_icon_path("nojs-statusicon-allowed.png");
			break;

		case NOJS_MENU_ICON_STATE_MIXED:
			imageFilename=nojs_get_icon_path("nojs-statusicon-mixed.png");
			break;

		case NOJS_MENU_ICON_STATE_DENIED:
		case NOJS_MENU_ICON_STATE_UNDETERMINED:
			imageFilename=nojs_get_icon_path("nojs-statusicon-denied.png");
			break;
	}

	buttonImage=gtk_image_new_from_file(imageFilename);
	g_free(imageFilename);

	/* Set image at statusbar button */
	gtk_button_set_image(GTK_BUTTON(statusbarIcon), buttonImage);
}

/* A tab in browser was activated */
static void _nojs_on_switch_tab(NoJS *self, MidoriView *inOldView, MidoriView *inNewView, gpointer inUserData)
{
	g_return_if_fail(IS_NOJS(self));
	g_return_if_fail(MIDORI_IS_BROWSER(inUserData));

	MidoriBrowser		*browser=MIDORI_BROWSER(inUserData);
	NoJSView			*view;

	/* Disconnect signal handlers from old view */
	if(inOldView)
	{
		/* Get NoJS view of old view */
		view=(NoJSView*)g_object_get_data(G_OBJECT(inOldView), "nojs-view-instance");
		g_return_if_fail(NOJS_IS_VIEW(view));

		/* Disconnect signal handlers */
		g_signal_handlers_disconnect_by_func(view, G_CALLBACK(_nojs_on_menu_icon_changed), browser);
	}

	/* Get NoJS view of new view */
	view=(NoJSView*)g_object_get_data(G_OBJECT(inNewView), "nojs-view-instance");
	g_return_if_fail(NOJS_IS_VIEW(view));

	/* Connect signals */
	g_signal_connect_swapped(view, "notify::menu-icon-state", G_CALLBACK(_nojs_on_menu_icon_changed), browser);

	/* Update menu icon*/
	_nojs_on_menu_icon_changed(browser, NULL, view);
}

/* A tab of a browser was removed */
static void _nojs_on_remove_tab(NoJS *self, MidoriView *inView, gpointer inUserData)
{
	g_return_if_fail(IS_NOJS(self));

	NoJSView		*view;

	/* Get NoJS view of current active midori view */
	view=NOJS_VIEW(g_object_get_data(G_OBJECT(inView), "nojs-view-instance"));
	g_return_if_fail(NOJS_IS_VIEW(view));

	g_object_unref(view);
}

/* A tab of a browser was added */
static void _nojs_on_add_tab(NoJS *self, MidoriView *inView, gpointer inUserData)
{
	g_return_if_fail(IS_NOJS(self));
	g_return_if_fail(MIDORI_IS_BROWSER(inUserData));

	/* Create nojs view and add to tab */
	MidoriBrowser	*browser=MIDORI_BROWSER(inUserData);

	nojs_view_new(self, browser, inView);
}

/* A browser window was added */
static void _nojs_on_add_browser(NoJS *self, MidoriBrowser *inBrowser, gpointer inUserData)
{
	g_return_if_fail(IS_NOJS(self));
	g_return_if_fail(MIDORI_IS_BROWSER(inBrowser));

	GList			*tabs, *iter;
	GtkWidget		*statusbar;
	GtkWidget		*statusbarIcon;
	MidoriView		*view;
	NoJSView		*nojsView;

	/* Set up all current available tabs in browser */
	tabs=midori_browser_get_tabs(inBrowser);
	for(iter=tabs; iter; iter=g_list_next(iter)) _nojs_on_add_tab(self, iter->data, inBrowser);
	g_list_free(tabs);

	/* Add status bar icon to browser */
	g_object_get(inBrowser, "statusbar", &statusbar, NULL);
	if(statusbar)
	{
		/* Create and set up status icon */
		statusbarIcon=gtk_button_new();
		gtk_button_set_relief(GTK_BUTTON(statusbarIcon), GTK_RELIEF_NONE);
		gtk_widget_show_all(statusbarIcon);
		gtk_box_pack_end(GTK_BOX(statusbar), statusbarIcon, FALSE, FALSE, 0);
		g_object_set_data_full(G_OBJECT(inBrowser), "nojs-statusicon", g_object_ref(statusbarIcon), (GDestroyNotify)gtk_widget_destroy);

		/* Connect signals */
		g_signal_connect_swapped(statusbarIcon, "clicked", G_CALLBACK(_nojs_on_statusbar_icon_clicked), inBrowser);

		/* Release our reference to statusbar and status icon */
		g_object_unref(statusbarIcon);
		g_object_unref(statusbar);

		/* Update menu icon*/
		view=MIDORI_VIEW(midori_browser_get_current_tab(inBrowser));
		if(view)
		{
			nojsView=(NoJSView*)g_object_get_data(G_OBJECT(view), "nojs-view-instance");
			if(nojsView) _nojs_on_menu_icon_changed(inBrowser, NULL, nojsView);
		}
	}

	/* Listen to new tabs opened in browser */
	g_signal_connect_swapped(inBrowser, "add-tab", G_CALLBACK(_nojs_on_add_tab), self);
	g_signal_connect_swapped(inBrowser, "switch-tab", G_CALLBACK(_nojs_on_switch_tab), self);
	g_signal_connect_swapped(inBrowser, "remove-tab", G_CALLBACK(_nojs_on_remove_tab), self);
}

/* Application property has changed */
static void _nojs_on_application_changed(NoJS *self)
{
	g_return_if_fail(IS_NOJS(self));

	NoJSPrivate		*priv=NOJS(self)->priv;
	GList			*browsers, *iter;

	/* Set up all current open browser windows */
	browsers=midori_app_get_browsers(priv->application);
	for(iter=browsers; iter; iter=g_list_next(iter)) _nojs_on_add_browser(self, MIDORI_BROWSER(iter->data), priv->application);
	g_list_free(browsers);

	/* Listen to new browser windows opened */
	g_signal_connect_swapped(priv->application, "add-browser", G_CALLBACK(_nojs_on_add_browser), self);

	/* Notify about property change */
	g_object_notify_by_pspec(G_OBJECT(self), NoJSProperties[PROP_APPLICATION]);
}

/* IMPLEMENTATION: GObject */

/* Finalize this object */
static void nojs_finalize(GObject *inObject)
{
	NoJS			*self=NOJS(inObject);
	NoJSPrivate		*priv=self->priv;
	GList			*browsers, *browser;
	GList			*tabs, *tab;
	WebKitWebView	*webkitView;
	SoupSession		*session;

	/* Dispose allocated resources */
	session=webkit_get_default_session();
	g_signal_handlers_disconnect_by_data(session, self);

	if(priv->databaseFilename)
	{
		g_free(priv->databaseFilename);
		priv->databaseFilename=NULL;
	}

	if(priv->database)
	{
		sqlite3_close(priv->database);
		priv->database=NULL;
	}

	if(priv->application)
	{
		g_signal_handlers_disconnect_by_data(priv->application, self);

		browsers=midori_app_get_browsers(priv->application);
		for(browser=browsers; browser; browser=g_list_next(browser))
		{
			g_signal_handlers_disconnect_by_data(browser->data, self);
			g_object_set_data(G_OBJECT(browser->data), "nojs-statusicon", NULL);

			tabs=midori_browser_get_tabs(MIDORI_BROWSER(browser->data));
			for(tab=tabs; tab; tab=g_list_next(tab))
			{
				g_signal_handlers_disconnect_by_data(tab->data, self);

				webkitView=WEBKIT_WEB_VIEW(midori_view_get_web_view(MIDORI_VIEW(tab->data)));
				g_signal_handlers_disconnect_by_data(webkitView, self);
			}
			g_list_free(tabs);
		}
		g_list_free(browsers);

		priv->application=NULL;
	}

	/* Call parent's class finalize method */
	G_OBJECT_CLASS(nojs_parent_class)->finalize(inObject);
}

/* Set/get properties */
static void nojs_set_property(GObject *inObject,
								guint inPropID,
								const GValue *inValue,
								GParamSpec *inSpec)
{
	NoJS		*self=NOJS(inObject);
	
	switch(inPropID)
	{
		/* Construct-only properties */
		case PROP_EXTENSION:
			self->priv->extension=g_value_get_object(inValue);
			_nojs_open_database(self);
			break;

		case PROP_APPLICATION:
			self->priv->application=g_value_get_object(inValue);
			_nojs_on_application_changed(self);
			break;

		case PROP_ALLOW_LOCAL_PAGES:
			self->priv->allowLocalPages=g_value_get_boolean(inValue);
			g_object_notify_by_pspec(G_OBJECT(self), NoJSProperties[PROP_ALLOW_LOCAL_PAGES]);
			break;

		case PROP_ONLY_SECOND_LEVEL:
			self->priv->checkOnlySecondLevel=g_value_get_boolean(inValue);
			g_object_notify_by_pspec(G_OBJECT(self), NoJSProperties[PROP_ONLY_SECOND_LEVEL]);
			break;

		case PROP_UNKNOWN_DOMAIN_POLICY:
			self->priv->unknownDomainPolicy=g_value_get_enum(inValue);
			g_object_notify_by_pspec(G_OBJECT(self), NoJSProperties[PROP_UNKNOWN_DOMAIN_POLICY]);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(inObject, inPropID, inSpec);
			break;
	}
}

static void nojs_get_property(GObject *inObject,
								guint inPropID,
								GValue *outValue,
								GParamSpec *inSpec)
{
	NoJS		*self=NOJS(inObject);

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

		case PROP_ALLOW_LOCAL_PAGES:
			g_value_set_boolean(outValue, self->priv->allowLocalPages);
			break;

		case PROP_ONLY_SECOND_LEVEL:
			g_value_set_boolean(outValue, self->priv->checkOnlySecondLevel);
			break;

		case PROP_UNKNOWN_DOMAIN_POLICY:
			g_value_set_enum(outValue, self->priv->unknownDomainPolicy);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(inObject, inPropID, inSpec);
			break;
	}
}

/* Class initialization
 * Override functions in parent classes and define properties and signals
 */
static void nojs_class_init(NoJSClass *klass)
{
	GObjectClass		*gobjectClass=G_OBJECT_CLASS(klass);

	/* Override functions */
	gobjectClass->finalize=nojs_finalize;
	gobjectClass->set_property=nojs_set_property;
	gobjectClass->get_property=nojs_get_property;

	/* Set up private structure */
	g_type_class_add_private(klass, sizeof(NoJSPrivate));

	/* Define properties */
	NoJSProperties[PROP_EXTENSION]=
		g_param_spec_object("extension",
								_("Extension instance"),
								_("The Midori extension instance for this extension"),
								MIDORI_TYPE_EXTENSION,
								G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	NoJSProperties[PROP_APPLICATION]=
		g_param_spec_object("application",
								_("Application instance"),
								_("The Midori application instance this extension belongs to"),
								MIDORI_TYPE_APP,
								G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	NoJSProperties[PROP_DATABASE]=
		g_param_spec_pointer("database",
								_("Database instance"),
								_("Pointer to sqlite database instance used by this extension"),
								G_PARAM_READABLE);

	NoJSProperties[PROP_DATABASE_FILENAME]=
		g_param_spec_string("database-filename",
								_("Database path"),
								_("Path to sqlite database instance used by this extension"),
								NULL,
								G_PARAM_READABLE);

	NoJSProperties[PROP_ALLOW_LOCAL_PAGES]=
		g_param_spec_boolean("allow-local-pages",
								_("Allow local pages"),
								_("Allow scripts to run on local (file://) pages"),
								TRUE,
								G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	NoJSProperties[PROP_ONLY_SECOND_LEVEL]=
		g_param_spec_boolean("only-second-level",
								_("Only second level"),
								_("Reduce each domain to its second-level (e.g. www.example.org to example.org) for comparison"),
								TRUE,
								G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	NoJSProperties[PROP_UNKNOWN_DOMAIN_POLICY]=
		g_param_spec_enum("unknown-domain-policy",
								_("Unknown domain policy"),
								_("Policy to use for unknown domains"),
								NOJS_TYPE_POLICY,
								NOJS_POLICY_BLOCK,
								G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties(gobjectClass, PROP_LAST, NoJSProperties);

	/* Define signals */

	/* Why does this signal exist?
	 * 
	 * The problem I faced when developing this extension was
	 * that I needed to cancel a SoupMessage as soon as possible
	 * (when http headers were received).
	 * I tried to connect to signal "resource-response-received"
	 * of WebKitWebView but the SoupMessage instance was not
	 * exactly the same which were sent or received by SoupSession.
	 * So I could not cancel the SoupMessage or better: I cancelled
	 * a SoupMessage which is not be handled so it had no effect.
	 * The body of SoupMessage was still being loaded and javascript
	 * was executed. I think the problem is that webkit-gtk creates
	 * a copy of the real SoupMessage which is going to be sent and
	 * received.
	 * 
	 * So I decided to connect to signal "got-headers" of every
	 * SoupMessage sent by the default SoupSession which I notice
	 * by connecting to signal "request-started" of SoupSession. Each
	 * NoJSView connects to signal "resource-request-starting" of
	 * WebKitWebView to remember each URI going to be loaded. When
	 * a SoupMessage hits "got-headers" and is a javascript resource
	 * I can cancel the message immediately and clear the body which
	 * causes webkit-gtk to copy a empty body if it does at all as the
	 * SoupMessage was cancelled. Then I emit this signal
	 * "uri-load-policy-status" to notify each view but the cancellation.
	 * (It also notifies all views if it is going to load to keep the
	 * menu in right state.) Each view will check if it _could_ be a
	 * resource itself requested and will update its menu accordingly.
	 * It might happen that a request will match two views because only
	 * the URI will be checked by the view because I cannot determine
	 * to which view the SoupMessage belongs to. But it doesn't matter
	 * because if a javascript resource was denied or allowed in one view
	 * it is likely be denied or allowed in other views too ;)
	 */
	NoJSSignals[URI_LOAD_POLICY_STATUS]=
		g_signal_new("uri-load-policy-status",
						G_TYPE_FROM_CLASS(klass),
						G_SIGNAL_RUN_LAST,
						G_STRUCT_OFFSET(NoJSClass, uri_load_policy_status),
						NULL,
						NULL,
						_nojs_closure_VOID__STRING_ENUM,
						G_TYPE_NONE,
						2,
						G_TYPE_STRING,
						NOJS_TYPE_POLICY);

	NoJSSignals[POLICY_CHANGED]=
		g_signal_new("policy-changed",
						G_TYPE_FROM_CLASS(klass),
						G_SIGNAL_RUN_LAST,
						G_STRUCT_OFFSET(NoJSClass, policy_changed),
						NULL,
						NULL,
						g_cclosure_marshal_VOID__STRING,
						G_TYPE_NONE,
						1,
						G_TYPE_STRING);
}

/* Object initialization
 * Create private structure and set up default values
 */

static void nojs_init(NoJS *self)
{
	NoJSPrivate		*priv;
	SoupSession		*session;

	priv=self->priv=NOJS_GET_PRIVATE(self);

	/* Set up default values */
	priv->database=NULL;
	priv->databaseFilename=NULL;
	priv->allowLocalPages=TRUE;
	priv->checkOnlySecondLevel=TRUE;
	priv->unknownDomainPolicy=NOJS_POLICY_BLOCK;

	/* Connect to signals on session to be able to cancel messages
	 * loading javascript documents
	 */
	session=webkit_get_default_session();
	g_signal_connect_swapped(session, "request-started", G_CALLBACK(_nojs_on_request_started), self);
}

/* Implementation: Public API */

/* Create new object */
NoJS* nojs_new(MidoriExtension *inExtension, MidoriApp *inApp)
{
	return(g_object_new(TYPE_NOJS,
							"extension", inExtension,
							"application", inApp,
							NULL));
}

/* Retrieves domain from uri depending on preferences (e.g. only second level domain) */
gchar* nojs_get_domain(NoJS *self, SoupURI *inURI)
{
	g_return_val_if_fail(IS_NOJS(self), NULL);
	g_return_val_if_fail(inURI, NULL);

	NoJSPrivate			*priv=self->priv;
	const gchar			*realDomain;
	gchar				*finalDomain;

	/* Get domain of site to lookup */
	realDomain=soup_uri_get_host(inURI);

	if(priv->checkOnlySecondLevel)
		finalDomain=midori_uri_get_base_domain(realDomain);
    else
		finalDomain=midori_uri_to_ascii(realDomain);

	/* Return domain */
	return(finalDomain);
}

/* Get/set policy for javascript from site */
gint nojs_get_policy(NoJS *self, SoupURI *inURI)
{
	g_return_val_if_fail(IS_NOJS(self), NOJS_POLICY_UNDETERMINED);
	g_return_val_if_fail(inURI, NOJS_POLICY_UNDETERMINED);

	NoJSPrivate			*priv=self->priv;
	sqlite3_stmt		*statement=NULL;
	gint				error;
	gint				policy=NOJS_POLICY_UNDETERMINED;
	gchar				*inDomain;

	/* Check to allow local pages */
	if(soup_uri_get_scheme(inURI) == SOUP_URI_SCHEME_FILE)
	{
		if(priv->allowLocalPages) return(NOJS_POLICY_ACCEPT);
		else return(priv->unknownDomainPolicy);
	}

	/* Check for open database */
	g_return_val_if_fail(priv->database, policy);

	/* Get domain from URI */
	inDomain=nojs_get_domain(self, inURI);

	/* Lookup policy for site in database */
	error=sqlite3_prepare_v2(priv->database,
								"SELECT site, value FROM policies WHERE site LIKE ? LIMIT 1;",
								-1,
								&statement,
								NULL);
	if(statement && error==SQLITE_OK) error=sqlite3_bind_text(statement, 1, inDomain, -1, NULL);
	if(statement && error==SQLITE_OK)
	{
		if(sqlite3_step(statement)==SQLITE_ROW) policy=sqlite3_column_int(statement, 1);
	}
		else g_warning(_("SQL fails: %s"), sqlite3_errmsg(priv->database));

	sqlite3_finalize(statement);

	/* If we have not found a policy for the domain then it is an unknown domain.
	 * Get default policy for unknown domains.
	 */
	if(policy==NOJS_POLICY_UNDETERMINED) policy=priv->unknownDomainPolicy;

	return(policy);
}

void nojs_set_policy(NoJS *self, const gchar *inDomain, NoJSPolicy inPolicy)
{
	g_return_if_fail(IS_NOJS(self));
	g_return_if_fail(inDomain);
	g_return_if_fail(inPolicy>=NOJS_POLICY_ACCEPT && inPolicy<=NOJS_POLICY_BLOCK);

	NoJSPrivate			*priv=self->priv;
	gchar				*sql;
	gchar				*error=NULL;
	gint				success;

	/* Check for open database */
	g_return_if_fail(priv->database);

	/* Update policy in database */
	sql=sqlite3_mprintf("INSERT OR REPLACE INTO policies (site, value) VALUES ('%q', %d);",
							inDomain,
							inPolicy);
	success=sqlite3_exec(priv->database, sql, NULL, NULL, &error);
	if(success!=SQLITE_OK) g_warning(_("SQL fails: %s"), error);
	if(error) sqlite3_free(error);
	sqlite3_free(sql);

	/* Emit signal to notify about policy change */
	if(success==SQLITE_OK) g_signal_emit(self, NoJSSignals[POLICY_CHANGED], 0, inDomain);
}

/* Get/set default policy for unknown domains */
NoJSPolicy nojs_get_policy_for_unknown_domain(NoJS *self)
{
	g_return_val_if_fail(IS_NOJS(self), NOJS_POLICY_UNDETERMINED);

	return(self->priv->unknownDomainPolicy);
}

void nojs_set_policy_for_unknown_domain(NoJS *self, NoJSPolicy inPolicy)
{
	g_return_if_fail(IS_NOJS(self));
	g_return_if_fail(inPolicy>=NOJS_POLICY_ACCEPT && inPolicy<=NOJS_POLICY_BLOCK);

	if(self->priv->unknownDomainPolicy!=inPolicy)
	{
		self->priv->unknownDomainPolicy=inPolicy;
		midori_extension_set_integer(self->priv->extension, "unknown-domain-policy", inPolicy);
		g_object_notify_by_pspec(G_OBJECT(self), NoJSProperties[PROP_UNKNOWN_DOMAIN_POLICY]);
	}
}

/* Get/set flag to allow javascript on local pages */
gboolean nojs_get_allow_local_pages(NoJS *self)
{
	g_return_val_if_fail(IS_NOJS(self), TRUE);

	return(self->priv->allowLocalPages);
}

void nojs_set_allow_local_pages(NoJS *self, gboolean inAllow)
{
	g_return_if_fail(IS_NOJS(self));

	if(self->priv->allowLocalPages!=inAllow)
	{
		self->priv->allowLocalPages=inAllow;
		midori_extension_set_boolean(self->priv->extension, "allow-local-pages", inAllow);
		g_object_notify_by_pspec(G_OBJECT(self), NoJSProperties[PROP_ALLOW_LOCAL_PAGES]);
	}
}

/* Get/set flag to check for second-level domains only */
gboolean nojs_get_only_second_level_domain(NoJS *self)
{
	g_return_val_if_fail(IS_NOJS(self), TRUE);

	return(self->priv->checkOnlySecondLevel);
}

void nojs_set_only_second_level_domain(NoJS *self, gboolean inOnlySecondLevel)
{
	g_return_if_fail(IS_NOJS(self));

	if(self->priv->checkOnlySecondLevel!=inOnlySecondLevel)
	{
		self->priv->checkOnlySecondLevel=inOnlySecondLevel;
		midori_extension_set_boolean(self->priv->extension, "only-second-level", inOnlySecondLevel);
		g_object_notify_by_pspec(G_OBJECT(self), NoJSProperties[PROP_ONLY_SECOND_LEVEL]);
	}
}

/************************************************************************************/

/* Implementation: Enumeration */
GType nojs_policy_get_type(void)
{
	static volatile gsize	g_define_type_id__volatile=0;

	if(g_once_init_enter(&g_define_type_id__volatile))
	{
		static const GEnumValue values[]=
		{
			{ NOJS_POLICY_UNDETERMINED, "NOJS_POLICY_UNDETERMINED", N_("Undetermined") },
			{ NOJS_POLICY_ACCEPT, "NOJS_POLICY_ACCEPT", N_("Accept") },
			{ NOJS_POLICY_ACCEPT_TEMPORARILY, "NOJS_POLICY_ACCEPT_TEMPORARILY", N_("Accept temporarily") },
			{ NOJS_POLICY_BLOCK, "NOJS_POLICY_BLOCK", N_("Block") },
			{ 0, NULL, NULL }
		};

		GType	g_define_type_id=g_enum_register_static(g_intern_static_string("NoJSPolicy"), values);
		g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
	}

	return(g_define_type_id__volatile);
}
