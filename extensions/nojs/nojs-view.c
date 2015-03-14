/*
 Copyright (C) 2013 Stephan Haller <nomad@froevel.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "nojs-view.h"
#include "nojs-preferences.h"

/* Define this class in GObject system */
G_DEFINE_TYPE(NoJSView,
				nojs_view,
				G_TYPE_OBJECT)

/* Properties */
enum
{
	PROP_0,

	PROP_MANAGER,
	PROP_BROWSER,
	PROP_VIEW,
	PROP_MENU_ICON_STATE,

	PROP_LAST
};

static GParamSpec* NoJSViewProperties[PROP_LAST]={ 0, };

/* Private structure - access only by public API if needed */
#define NOJS_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), TYPE_NOJS_VIEW, NoJSViewPrivate))

struct _NoJSViewPrivate
{
	/* Extension related */
	NoJS				*manager;
	MidoriBrowser		*browser;
	MidoriView			*view;

	GtkWidget			*menu;
	gboolean			menuPolicyWasChanged;
	NoJSMenuIconState	menuIconState;

	GSList				*resourceURIs;
};

/* IMPLEMENTATION: Private variables and methods */

/* Preferences of this extension should be opened */
static void _nojs_view_on_preferences_response(GtkWidget* inDialog,
												gint inResponse,
												gpointer *inUserData)
{
	gtk_widget_destroy(inDialog);
}

static void _nojs_view_on_open_preferences(NoJSView *self, gpointer inUserData)
{
	g_return_if_fail(NOJS_IS_VIEW(self));

	NoJSViewPrivate		*priv=self->priv;

	/* Show preferences window */
	GtkWidget* dialog;

	dialog=nojs_preferences_new(priv->manager);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	g_signal_connect(dialog, "response", G_CALLBACK (_nojs_view_on_preferences_response), self);
	gtk_widget_show_all(dialog);
}

/* Selection was done in menu */
static void _nojs_view_on_menu_selection_done(NoJSView *self, gpointer inUserData)
{
	g_return_if_fail(NOJS_IS_VIEW(self));

	NoJSViewPrivate		*priv=self->priv;

	/* Check if any policy was changed and reload page */
	if(priv->menuPolicyWasChanged!=FALSE)
	{
		/* Reset flag that any policy was changed */
		priv->menuPolicyWasChanged=FALSE;

		/* Reload page */
		midori_view_reload(priv->view, FALSE);
g_message("%s: Reloading page %s as policy has changed", __func__, midori_view_get_display_uri(priv->view));
	}
}

/* Destroy menu */
static void _nojs_view_destroy_menu(NoJSView *self)
{
	g_return_if_fail(NOJS_IS_VIEW(self));
	g_return_if_fail(self->priv->menu!=NULL);

	NoJSViewPrivate		*priv=self->priv;

	/* Empty menu and list of domains added to menu */
	gtk_widget_destroy(priv->menu);
	priv->menu=NULL;

	/* Reset menu icon to default state */
	priv->menuIconState=NOJS_MENU_ICON_STATE_UNDETERMINED;
	g_object_notify_by_pspec(G_OBJECT(self), NoJSViewProperties[PROP_MENU_ICON_STATE]);
}

/* Create empty menu */
static void _nojs_view_create_empty_menu(NoJSView *self)
{
	g_return_if_fail(NOJS_IS_VIEW(self));
	g_return_if_fail(self->priv->menu==NULL);

	NoJSViewPrivate		*priv=self->priv;
	GtkWidget			*item;

	/* Create new menu and set up default items */
	priv->menu=gtk_menu_new();

	item=gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
	g_signal_connect_swapped(item, "activate", G_CALLBACK(_nojs_view_on_open_preferences), self);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(priv->menu), item);
	gtk_widget_show_all(item);

	/* Reset flag that any policy was changed */
	priv->menuPolicyWasChanged=FALSE;

	/* Reset menu icon to default state */
	priv->menuIconState=NOJS_MENU_ICON_STATE_UNDETERMINED;
	g_object_notify_by_pspec(G_OBJECT(self), NoJSViewProperties[PROP_MENU_ICON_STATE]);

	/* Connect signal to menu */
	g_signal_connect_swapped(priv->menu, "selection-done", G_CALLBACK(_nojs_view_on_menu_selection_done), self);
}

/* Change visibility state of menu item for a domain depending on policy */
static gboolean _nojs_view_menu_item_change_policy(NoJSView *self, const gchar *inDomain, NoJSPolicy inPolicy)
{
	g_return_val_if_fail(NOJS_IS_VIEW(self), FALSE);
	g_return_val_if_fail(inDomain, FALSE);

	NoJSViewPrivate		*priv=self->priv;
	GList				*items, *iter;
	gboolean			updated;

	/* Handle accept-for-session like accept when showing or hiding menu items */
	if(inPolicy==NOJS_POLICY_ACCEPT_TEMPORARILY) inPolicy=NOJS_POLICY_ACCEPT;

	/* Update menu items */
	updated=FALSE;
	items=gtk_container_get_children(GTK_CONTAINER(priv->menu));
	for(iter=items; iter; iter=iter->next)
	{
		/* Only check and update menu items (not separators and so on) */
		if(GTK_IS_MENU_ITEM(iter->data))
		{
			GtkMenuItem		*item=GTK_MENU_ITEM(iter->data);
			const gchar		*itemDomain;
			NoJSPolicy		itemPolicy;

			itemDomain=(const gchar*)g_object_get_data(G_OBJECT(item), "domain");
			itemPolicy=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "policy"));

			/* Handle accept-for-session like accept when showing or hiding menu items */
			if(itemPolicy==NOJS_POLICY_ACCEPT_TEMPORARILY) itemPolicy=NOJS_POLICY_ACCEPT;

			/* If menu item has "domain"-data update its visibility state
			 * depending on matching policy
			 */
			if(g_strcmp0(itemDomain, inDomain)==0)
			{
				if(itemPolicy==inPolicy) gtk_widget_hide(GTK_WIDGET(item));
					else gtk_widget_show_all(GTK_WIDGET(item));

				/* Set flag that at least one menu item was updated */
				updated=TRUE;
			}
		}
	}
	g_list_free(items);

	/* Return flag indicating if at least one menu item was updated */
	return(updated);
}

/* A menu item was selected */
static void _nojs_view_on_menu_item_activate(NoJSView *self, gpointer inUserData)
{
	g_return_if_fail(NOJS_IS_VIEW(self));
	g_return_if_fail(GTK_IS_MENU_ITEM(inUserData));

	NoJSViewPrivate			*priv=self->priv;
	GtkMenuItem				*item=GTK_MENU_ITEM(inUserData);
	const gchar				*domain;
	NoJSPolicy				policy;

	/* Get domain and policy to set */
	domain=(const gchar*)g_object_get_data(G_OBJECT(item), "domain");
	policy=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "policy"));
	g_return_if_fail(domain);
	g_return_if_fail(policy>=NOJS_POLICY_ACCEPT && policy<=NOJS_POLICY_BLOCK);

	/* Set policy for domain and update menu items */
	_nojs_view_menu_item_change_policy(self, domain, policy);
	nojs_set_policy(priv->manager, domain, policy);

	/* Set flag that a policy was changed */
	priv->menuPolicyWasChanged=TRUE;
}

/* Add site to menu */
static void _nojs_view_add_site_to_menu(NoJSView *self, const gchar *inDomain, NoJSPolicy inPolicy)
{
	g_return_if_fail(NOJS_IS_VIEW(self));
	g_return_if_fail(inDomain);

	NoJSViewPrivate		*priv=self->priv;
	GtkWidget			*item;
	gchar				*itemLabel;
	GtkWidget			*itemImage;
	static gint			INSERT_POSITION=1;
	NoJSMenuIconState	newMenuIconState;

	/* Create menu object if not available */
	if(!priv->menu) _nojs_view_create_empty_menu(self);

	/* Check if domain was already added to menu. If it exists just update it. */
	if(_nojs_view_menu_item_change_policy(self, inDomain, inPolicy)==TRUE) return;

	/* Add menu item(s) for domain */
	itemLabel=g_strdup_printf(_("Deny %s"), inDomain);
	item=gtk_image_menu_item_new_with_label(itemLabel);
	itemImage=gtk_image_new_from_stock (GTK_STOCK_NO, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), itemImage);
	gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(item), TRUE);
	gtk_menu_shell_insert(GTK_MENU_SHELL(priv->menu), item, INSERT_POSITION);
	if(inPolicy!=NOJS_POLICY_BLOCK) gtk_widget_show_all(item);
	g_object_set_data_full(G_OBJECT(item), "domain", g_strdup(inDomain), (GDestroyNotify)g_free);
	g_object_set_data(G_OBJECT(item), "policy", GINT_TO_POINTER(NOJS_POLICY_BLOCK));
	g_signal_connect_swapped(item, "activate", G_CALLBACK(_nojs_view_on_menu_item_activate), self);
	g_free(itemLabel);

	itemLabel=g_strdup_printf(_("Allow %s"), inDomain);
	item=gtk_image_menu_item_new_with_label(itemLabel);
	itemImage=gtk_image_new_from_stock (GTK_STOCK_YES, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), itemImage);
	gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(item), TRUE);
	gtk_menu_shell_insert(GTK_MENU_SHELL(priv->menu), item, INSERT_POSITION);
	if(inPolicy!=NOJS_POLICY_ACCEPT && inPolicy!=NOJS_POLICY_ACCEPT_TEMPORARILY) gtk_widget_show_all(item);
	g_object_set_data_full(G_OBJECT(item), "domain", g_strdup(inDomain), (GDestroyNotify)g_free);
	g_object_set_data(G_OBJECT(item), "policy", GINT_TO_POINTER(NOJS_POLICY_ACCEPT));
	g_signal_connect_swapped(item, "activate", G_CALLBACK(_nojs_view_on_menu_item_activate), self);
	g_free(itemLabel);

	itemLabel=g_strdup_printf(_("Allow %s this session"), inDomain);
	item=gtk_image_menu_item_new_with_label(itemLabel);
	itemImage=gtk_image_new_from_stock (GTK_STOCK_OK, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), itemImage);
	gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(item), TRUE);
	gtk_menu_shell_insert(GTK_MENU_SHELL(priv->menu), item, INSERT_POSITION);
	if(inPolicy!=NOJS_POLICY_ACCEPT && inPolicy!=NOJS_POLICY_ACCEPT_TEMPORARILY) gtk_widget_show_all(item);
	g_object_set_data_full(G_OBJECT(item), "domain", g_strdup(inDomain), (GDestroyNotify)g_free);
	g_object_set_data(G_OBJECT(item), "policy", GINT_TO_POINTER(NOJS_POLICY_ACCEPT_TEMPORARILY));
	g_signal_connect_swapped(item, "activate", G_CALLBACK(_nojs_view_on_menu_item_activate), self);
	g_free(itemLabel);

	/* Add seperator to seperate actions for this domain from the other domains */
	item=gtk_separator_menu_item_new();
	gtk_menu_shell_insert(GTK_MENU_SHELL(priv->menu), item, INSERT_POSITION);
	gtk_widget_show_all(item);

	/* Determine state of status icon */
	if(priv->menuIconState!=NOJS_MENU_ICON_STATE_MIXED)
	{
		switch(inPolicy)
		{
			case NOJS_POLICY_ACCEPT:
			case NOJS_POLICY_ACCEPT_TEMPORARILY:
				newMenuIconState=NOJS_MENU_ICON_STATE_ALLOWED;
				break;

			case NOJS_POLICY_BLOCK:
				newMenuIconState=NOJS_MENU_ICON_STATE_DENIED;
				break;

			default:
				newMenuIconState=NOJS_MENU_ICON_STATE_MIXED;
				break;
		}

		if(priv->menuIconState==NOJS_MENU_ICON_STATE_UNDETERMINED ||
			priv->menuIconState!=newMenuIconState)
		{
			priv->menuIconState=newMenuIconState;
			g_object_notify_by_pspec(G_OBJECT(self), NoJSViewProperties[PROP_MENU_ICON_STATE]);
		}
	}
}

/* Status of loading a site has changed */
static void _nojs_view_on_load_status_changed(NoJSView *self, GParamSpec *inSpec, gpointer inUserData)
{
	g_return_if_fail(NOJS_IS_VIEW(self));
	g_return_if_fail(WEBKIT_IS_WEB_VIEW(inUserData));

	NoJSViewPrivate			*priv=self->priv;
	WebKitWebView			*webkitView=WEBKIT_WEB_VIEW(inUserData);
	WebKitWebSettings		*settings=webkit_web_view_get_settings(webkitView);
	WebKitLoadStatus		status;
	SoupURI					*uri;

	/* Get URI of document loading/loaded */
	uri=soup_uri_new(webkit_web_view_get_uri(webkitView));

	/* Check load status */ 
	status=webkit_web_view_get_load_status(webkitView);

	/* Check if a view was emptied, e.g. for a new document going to be loaded soon */
	if(status==WEBKIT_LOAD_PROVISIONAL)
	{
		/* Create a new empty menu */
		_nojs_view_destroy_menu(self);
		_nojs_view_create_empty_menu(self);

		/* Free list of resource URIs, that's the list of URIs for all resources
		 * of a page
		 */
		if(priv->resourceURIs)
		{
			g_slist_free_full(priv->resourceURIs, (GDestroyNotify)g_free);
			priv->resourceURIs=NULL;
		}
	}

	/* Check if document loading is going to start. Do not check special pages. */
	if(status==WEBKIT_LOAD_COMMITTED &&
		uri &&
		uri->scheme &&
		g_strcmp0(uri->scheme, "about")!=0)
	{
		/* Check if domain is black-listed or white-listed and enable or
		 * disable javascript accordingly. But if settings match already
		 * the state it should get do not set it again to avoid reloads of page.
		 */
		gchar				*domain;
		NoJSPolicy			policy;
		gboolean			currentScriptsEnabled;
		gboolean			newScriptsEnabled;

		domain=nojs_get_domain(priv->manager, uri);
		policy=nojs_get_policy(priv->manager, uri);
		if(policy==NOJS_POLICY_UNDETERMINED)
		{
			policy=nojs_get_policy_for_unknown_domain(priv->manager);
			// TODO: Show nick_name of policy (enum) to use in warning
			g_warning("Got invalid policy. Using default policy for unknown domains.");
		}

		newScriptsEnabled=(policy==NOJS_POLICY_BLOCK ? FALSE : TRUE);
		g_object_get(G_OBJECT(settings), "enable-scripts", &currentScriptsEnabled, NULL);

		if(newScriptsEnabled!=currentScriptsEnabled)
		{
			g_object_set(G_OBJECT(settings), "enable-scripts", newScriptsEnabled, NULL);
			// TODO: Set uri also to ensure this uri is going to be reloaded
		}

		if(domain)
		{
			_nojs_view_add_site_to_menu(self, domain, policy);
			g_free(domain);
		}
	}

	/* Free allocated resources */
	if(uri) soup_uri_free(uri);
}

/* A request is going to sent */
static void _nojs_view_on_resource_request_starting(NoJSView *self,
														WebKitWebFrame *inFrame,
														WebKitWebResource *inResource,
														WebKitNetworkRequest *inRequest,
														WebKitNetworkResponse *inResponse,
														gpointer inUserData)
{
	g_return_if_fail(NOJS_IS_VIEW(self));

	NoJSViewPrivate			*priv=self->priv;
	SoupMessage				*message;
	SoupURI					*uri;
	gchar					*uriText;

	/* Remember resource URIs requesting */
	message=(inRequest ? webkit_network_request_get_message(inRequest) : NULL);
	if(message)
	{
		uri=soup_message_get_uri(message);
		if(uri)
		{
			uriText=soup_uri_to_string(uri, FALSE);
			priv->resourceURIs=g_slist_prepend(priv->resourceURIs, uriText);
		}
	}

	message=(inResponse ? webkit_network_response_get_message(inResponse) : NULL);
	if(message)
	{
		uri=soup_message_get_uri(message);
		if(uri)
		{
			uriText=soup_uri_to_string(uri, FALSE);
			priv->resourceURIs=g_slist_prepend(priv->resourceURIs, uriText);
		}
	}
}

/* A policy has changed */
static void _nojs_view_on_policy_changed(NoJSView *self, gchar *inDomain, gpointer inUserData)
{
	g_return_if_fail(NOJS_IS_VIEW(self));
	g_return_if_fail(inDomain);

	NoJSViewPrivate		*priv=self->priv;
	GList				*items, *iter;
	gboolean			reloaded;

	/* Check if the policy of a domain has changed this view has referenced resources to */
	reloaded=FALSE;
	items=gtk_container_get_children(GTK_CONTAINER(priv->menu));
	for(iter=items; reloaded==FALSE && iter; iter=iter->next)
	{
		if(GTK_IS_MENU_ITEM(iter->data))
		{
			const gchar		*itemDomain;

			/* Check if domain matches menu item */
			itemDomain=(const gchar*)g_object_get_data(G_OBJECT(iter->data), "domain");
			if(g_strcmp0(itemDomain, inDomain)==0)
			{
				/* Found domain in our menu so reload page */
				midori_view_reload(priv->view, FALSE);
				reloaded=TRUE;
			}
		}
	}
	g_list_free(items);
}

/* A javascript URI is going to loaded or blocked */
static void _nojs_view_on_uri_load_policy_status(NoJSView *self, gchar *inURI, NoJSPolicy inPolicy, gpointer inUserData)
{
	g_return_if_fail(NOJS_IS_VIEW(self));

	NoJSViewPrivate		*priv=self->priv;
	GSList				*iter;
	gchar				*checkURI;

	/* Check if uri (accepted or blocked) might be one of ours */
	for(iter=priv->resourceURIs; iter; iter=iter->next)
	{
		checkURI=(gchar*)iter->data;
		if(g_strcmp0(checkURI, inURI)==0)
		{
			SoupURI		*uri;
			gchar		*domain;

			uri=soup_uri_new(inURI);
			domain=nojs_get_domain(priv->manager, uri);
			if(domain)
			{
				_nojs_view_add_site_to_menu(self, domain, inPolicy);
				g_free(domain);
			}

			soup_uri_free(uri);
			break;
		}
	}
}

/* Property "view" has changed */
static void _nojs_view_on_view_changed(NoJSView *self, MidoriView *inView)
{
	NoJSViewPrivate		*priv=self->priv;
	WebKitWebView		*webkitView;

	/* Disconnect signal on old view */
	if(priv->view)
	{
		webkitView=WEBKIT_WEB_VIEW(midori_view_get_web_view(priv->view));
		g_signal_handlers_disconnect_by_data(webkitView, self);
		g_object_set_data(G_OBJECT(priv->view), "nojs-view-instance", NULL);
		g_object_unref(priv->view);
		priv->view=NULL;
	}

	/* Set new view if valid pointer */
	if(!inView) return;

	priv->view=g_object_ref(inView);
	g_object_set_data(G_OBJECT(priv->view), "nojs-view-instance", self);

	/* Listen to changes of load-status in view */
	webkitView=WEBKIT_WEB_VIEW(midori_view_get_web_view(priv->view));
	g_signal_connect_swapped(webkitView, "notify::load-status", G_CALLBACK(_nojs_view_on_load_status_changed), self);
	g_signal_connect_swapped(webkitView, "resource-request-starting", G_CALLBACK(_nojs_view_on_resource_request_starting), self);

	/* Create empty menu */
	_nojs_view_destroy_menu(self);
	_nojs_view_create_empty_menu(self);

	/* Release list of resource URIs */
	if(priv->resourceURIs)
	{
		g_slist_free_full(priv->resourceURIs, (GDestroyNotify)g_free);
		priv->resourceURIs=NULL;
	}
}

/* This extension is going to be deactivated */
static void _nojs_view_on_extension_deactivated(NoJSView *self, MidoriExtension *inExtension)
{
	g_return_if_fail(NOJS_IS_VIEW(self));

	/* Dispose allocated resources by unreferencing ourselve */
	g_object_unref(self);
}

/* Property "manager" has changed */
static void _nojs_view_on_manager_changed(NoJSView *self, NoJS *inNoJS)
{
	g_return_if_fail(NOJS_IS_VIEW(self));
	g_return_if_fail(!inNoJS || IS_NOJS(inNoJS));

	NoJSViewPrivate		*priv=self->priv;
	MidoriExtension		*extension;

	/* Release reference to old manager and clean up */
	if(priv->manager)
	{
		g_object_get(priv->manager, "extension", &extension, NULL);
		g_signal_handlers_disconnect_by_data(extension, self);
		g_object_unref(extension);

		g_signal_handlers_disconnect_by_data(priv->manager, self);
		g_object_unref(priv->manager);
		priv->manager=NULL;
	}

	/* Set new view if valid pointer */
	if(!inNoJS) return;

	priv->manager=g_object_ref(inNoJS);

	/* Connect signals to manager */
	g_signal_connect_swapped(priv->manager, "uri-load-policy-status", G_CALLBACK(_nojs_view_on_uri_load_policy_status), self);
	g_signal_connect_swapped(priv->manager, "policy-changed", G_CALLBACK(_nojs_view_on_policy_changed), self);

	/* Connect signal to get noticed when extension is going to be deactivated
	 * to release all references to GObjects
	 */
	g_object_get(priv->manager, "extension", &extension, NULL);
	g_signal_connect_swapped(extension, "deactivate", G_CALLBACK(_nojs_view_on_extension_deactivated), self);
	g_object_unref(extension);
}

/* IMPLEMENTATION: GObject */

/* Finalize this object */
static void nojs_view_finalize(GObject *inObject)
{
	NoJSView				*self=NOJS_VIEW(inObject);
	NoJSViewPrivate			*priv=self->priv;

	/* Dispose allocated resources */
	if(priv->manager)
	{
		MidoriExtension		*extension;

		g_object_get(priv->manager, "extension", &extension, NULL);
		g_signal_handlers_disconnect_by_data(extension, self);
		g_object_unref(extension);

		g_signal_handlers_disconnect_by_data(priv->manager, self);
		g_object_unref(priv->manager);
		priv->manager=NULL;
	}

	if(priv->browser)
	{
		g_object_unref(priv->browser);
		priv->browser=NULL;
	}

	if(priv->view)
	{
		_nojs_view_on_view_changed(self, NULL);
	}

	if(priv->menu)
	{
		gtk_widget_destroy(priv->menu);
		priv->menu=NULL;
	}

	if(priv->resourceURIs)
	{
		g_slist_free_full(priv->resourceURIs, (GDestroyNotify)g_free);
		priv->resourceURIs=NULL;
	}

	/* Call parent's class finalize method */
	G_OBJECT_CLASS(nojs_view_parent_class)->finalize(inObject);
}

/* Set/get properties */
static void nojs_view_set_property(GObject *inObject,
									guint inPropID,
									const GValue *inValue,
									GParamSpec *inSpec)
{
	NoJSView		*self=NOJS_VIEW(inObject);

	switch(inPropID)
	{
		/* Construct-only properties */
		case PROP_MANAGER:
			_nojs_view_on_manager_changed(self, NOJS(g_value_get_object(inValue)));
			break;

		case PROP_BROWSER:
			if(self->priv->browser) g_object_unref(self->priv->browser);
			self->priv->browser=g_object_ref(g_value_get_object(inValue));
			break;

		case PROP_VIEW:
			_nojs_view_on_view_changed(self, MIDORI_VIEW(g_value_get_object(inValue)));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(inObject, inPropID, inSpec);
			break;
	}
}

static void nojs_view_get_property(GObject *inObject,
									guint inPropID,
									GValue *outValue,
									GParamSpec *inSpec)
{
	NoJSView		*self=NOJS_VIEW(inObject);

	switch(inPropID)
	{
		case PROP_MANAGER:
			g_value_set_object(outValue, self->priv->manager);
			break;

		case PROP_BROWSER:
			g_value_set_object(outValue, self->priv->browser);
			break;

		case PROP_VIEW:
			g_value_set_object(outValue, self->priv->view);
			break;

		case PROP_MENU_ICON_STATE:
			g_value_set_enum(outValue, self->priv->menuIconState);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(inObject, inPropID, inSpec);
			break;
	}
}

/* Class initialization
 * Override functions in parent classes and define properties and signals
 */
static void nojs_view_class_init(NoJSViewClass *klass)
{
	GObjectClass		*gobjectClass=G_OBJECT_CLASS(klass);

	/* Override functions */
	gobjectClass->finalize=nojs_view_finalize;
	gobjectClass->set_property=nojs_view_set_property;
	gobjectClass->get_property=nojs_view_get_property;

	/* Set up private structure */
	g_type_class_add_private(klass, sizeof(NoJSViewPrivate));

	/* Define properties */
	NoJSViewProperties[PROP_MANAGER]=
		g_param_spec_object("manager",
								_("Manager instance"),
								_("Instance to global NoJS manager"),
								TYPE_NOJS,
								G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	NoJSViewProperties[PROP_BROWSER]=
		g_param_spec_object("browser",
								_("Browser window"),
								_("The Midori browser instance this view belongs to"),
								MIDORI_TYPE_BROWSER,
								G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	NoJSViewProperties[PROP_VIEW]=
		g_param_spec_object("view",
								_("View"),
								_("The Midori view instance this view belongs to"),
								MIDORI_TYPE_VIEW,
								G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	NoJSViewProperties[PROP_MENU_ICON_STATE]=
		g_param_spec_enum("menu-icon-state",
								_("Menu icon state"),
								_("State of menu icon to show in status bar"),
								NOJS_TYPE_MENU_ICON_STATE,
								NOJS_MENU_ICON_STATE_UNDETERMINED,
								G_PARAM_READABLE);

	g_object_class_install_properties(gobjectClass, PROP_LAST, NoJSViewProperties);
}

/* Object initialization
 * Create private structure and set up default values
 */
static void nojs_view_init(NoJSView *self)
{
	NoJSViewPrivate		*priv;

	priv=self->priv=NOJS_VIEW_GET_PRIVATE(self);

	/* Set up default values */
	priv->manager=NULL;
	priv->browser=NULL;
	priv->view=NULL;

	priv->menu=NULL;
	priv->menuPolicyWasChanged=FALSE;
	priv->menuIconState=NOJS_MENU_ICON_STATE_UNDETERMINED;

	priv->resourceURIs=NULL;

	/* Create empty menu */
	_nojs_view_create_empty_menu(self);
}

/* Implementation: Public API */

/* Create new object */
NoJSView* nojs_view_new(NoJS *inNoJS, MidoriBrowser *inBrowser, MidoriView *inView)
{
	return(g_object_new(TYPE_NOJS_VIEW,
							"manager", inNoJS,
							"browser", inBrowser,
							"view", inView,
							NULL));
}

/* Get menu widget for this view */
GtkMenu* nojs_view_get_menu(NoJSView *self)
{
	g_return_val_if_fail(NOJS_IS_VIEW(self), NULL);

	return(GTK_MENU(self->priv->menu));
}

/* Get image used for menu icon in status bar */
NoJSMenuIconState nojs_view_get_menu_icon_state(NoJSView *self)
{
	g_return_val_if_fail(NOJS_IS_VIEW(self), NOJS_MENU_ICON_STATE_UNDETERMINED);

	return(self->priv->menuIconState);
}

/************************************************************************************/

/* Implementation: Enumeration */
GType nojs_menu_icon_state_get_type(void)
{
	static volatile gsize	g_define_type_id__volatile=0;

	if(g_once_init_enter(&g_define_type_id__volatile))
	{
		static const GEnumValue values[]=
		{
			{ NOJS_MENU_ICON_STATE_UNDETERMINED, "NOJS_MENU_ICON_STATE_UNDETERMINED", N_("Undetermined") },
			{ NOJS_MENU_ICON_STATE_ALLOWED, "NOJS_MENU_ICON_STATE_ALLOWED", N_("Allowed") },
			{ NOJS_MENU_ICON_STATE_MIXED, "NOJS_MENU_ICON_STATE_MIXED", N_("Mixed") },
			{ NOJS_MENU_ICON_STATE_DENIED, "NOJS_MENU_ICON_STATE_DENIED", N_("Denied") },
			{ 0, NULL, NULL }
		};

		GType	g_define_type_id=g_enum_register_static(g_intern_static_string("NoJSMenuIconState"), values);
		g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
	}

	return(g_define_type_id__volatile);
}
