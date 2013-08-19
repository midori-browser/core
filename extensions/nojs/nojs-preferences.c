/*
 Copyright (C) 2013 Stephan Haller <nomad@froevel.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "nojs-preferences.h"

/* Define this class in GObject system */
G_DEFINE_TYPE(NoJSPreferences,
				nojs_preferences,
				GTK_TYPE_DIALOG)

/* Properties */
enum
{
	PROP_0,

	PROP_MANAGER,

	PROP_LAST
};

static GParamSpec* NoJSPreferencesProperties[PROP_LAST]={ 0, };

/* Private structure - access only by public API if needed */
#define NOJS_PREFERENCES_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), TYPE_NOJS_PREFERENCES, NoJSPreferencesPrivate))

struct _NoJSPreferencesPrivate
{
	/* Extension related */
	NoJS					*manager;
	sqlite3					*database;

	/* Dialog related */
	GtkWidget				*contentArea;
	GtkListStore			*listStore;
	GtkWidget				*list;
	GtkTreeSelection		*listSelection;
	GtkWidget				*deleteButton;
	GtkWidget				*deleteAllButton;
	GtkWidget				*allowAllSitesCheckbox;
	GtkWidget				*blockUnknownDomainsCheckbox;
	GtkWidget				*checkSecondLevelOnlyCheckbox;

	gint					signalAllowAllSitesToggledID;
	gint					signalBlockUnknownDomainsToggledID;
	gint					signalCheckSecondLevelOnlyToggledID;

	gint					signalManagerChangedDatabaseID;
	gint					signalManagerChangedAllowAllSitesID;
	gint					signalManagerChangedUnknownDomainPolicyID;
	gint					signalManagerChangedCheckSecondLevelID;
};

enum
{
	DOMAIN_COLUMN,
	POLICY_COLUMN,
	N_COLUMN
};


/* IMPLEMENTATION: Private variables and methods */

/* Fill domain list with stored policies */
static void _nojs_preferences_fill(NoJSPreferences *self)
{
	NoJSPreferencesPrivate	*priv=self->priv;
	gint					success;
	sqlite3_stmt			*statement=NULL;

	/* Clear tree/list view */
	gtk_list_store_clear(priv->listStore);

	/* If no database is present return here */
	if(!priv->database) return;

	/* Fill list store with policies from database */
	success=sqlite3_prepare_v2(priv->database,
								"SELECT site, value FROM policies;",
								-1,
								&statement,
								NULL);
	if(statement && success==SQLITE_OK)
	{
		gchar		*domain;
		gint		policy;
		gchar		*policyName;
		GtkTreeIter	iter;

		while(sqlite3_step(statement)==SQLITE_ROW)
		{
			/* Get values */
			domain=(gchar*)sqlite3_column_text(statement, 0);
			policy=sqlite3_column_int(statement, 1);

			switch(policy)
			{
				case NOJS_POLICY_ACCEPT:
					policyName=_("Accept");
					break;

				case NOJS_POLICY_ACCEPT_TEMPORARILY:
					policyName=_("Accept for session");
					break;

				case NOJS_POLICY_BLOCK:
					policyName=_("Block");
					break;

				default:
					policyName=NULL;
					break;
			}

			if(policyName)
			{
				gtk_list_store_append(priv->listStore, &iter);
				gtk_list_store_set(priv->listStore,
									&iter,
									DOMAIN_COLUMN, domain,
									POLICY_COLUMN, policyName,
									-1);
			}
		}
	}
		else g_warning(_("SQL fails: %s"), sqlite3_errmsg(priv->database));

	sqlite3_finalize(statement);
}

/* Database instance in manager changed */
static void _nojs_preferences_on_manager_database_changed(NoJSPreferences *self,
															GParamSpec *inSpec,
															gpointer inUserData)
{
	NoJSPreferencesPrivate	*priv=self->priv;
	NoJS					*manager=NOJS(inUserData);
	gchar					*databaseFile;

	/* Close connection to any open database */
	if(priv->database) sqlite3_close(priv->database);
	priv->database=NULL;

	/* Get pointer to new database and open database */
	g_object_get(manager, "database-filename", &databaseFile, NULL);
	if(databaseFile)
	{
		gint				success;

		success=sqlite3_open(databaseFile, &priv->database);
		if(success!=SQLITE_OK)
		{
			g_warning(_("Could not open database of extension: %s"), sqlite3_errmsg(priv->database));

			if(priv->database) sqlite3_close(priv->database);
			priv->database=NULL;
		}

		g_free(databaseFile);
	}

	/* Fill list with new database */
	_nojs_preferences_fill(self);

	/* Set up availability of management buttons */
	gtk_widget_set_sensitive(priv->deleteAllButton, priv->database!=NULL);
	gtk_widget_set_sensitive(priv->list, priv->database!=NULL);

	return;
}

/* Allow-all-sites changed in check-box or manager */
static void _nojs_preferences_on_allow_all_sites_changed(NoJSPreferences *self,
																gpointer *inUserData)
{
	NoJSPreferencesPrivate	*priv=self->priv;
	gboolean				state;

	/* Get toggle state of widget (but block signal for manager) and set in manager */
	g_signal_handler_block(priv->manager, priv->signalManagerChangedAllowAllSitesID);

	state=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->allowAllSitesCheckbox));
	nojs_set_allow_all_sites(priv->manager, state);

	g_signal_handler_unblock(priv->manager, priv->signalManagerChangedAllowAllSitesID);
}

static void _nojs_preferences_on_manager_allow_all_sites_changed(NoJSPreferences *self,
																	GParamSpec *inSpec,
																	gpointer inUserData)
{
	NoJSPreferencesPrivate	*priv=self->priv;
	NoJS					*manager=NOJS(inUserData);
	gboolean				state;

	/* Get new value from manager */
	state=nojs_get_allow_all_sites(manager);

	/* Set toggle in widget (but block signal for toggle) */
	g_signal_handler_block(priv->allowAllSitesCheckbox, priv->signalAllowAllSitesToggledID);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->allowAllSitesCheckbox), state);

	g_signal_handler_unblock(priv->allowAllSitesCheckbox, priv->signalAllowAllSitesToggledID);
}

/* Block-unknown-domains changed in check-box or manager */
static void _nojs_preferences_on_block_unknown_domains_changed(NoJSPreferences *self,
																gpointer *inUserData)
{
	NoJSPreferencesPrivate	*priv=self->priv;
	gboolean				state;
	NoJSPolicy				policy;

	/* Get toggle state of widget (but block signal for manager) and set in manager */
	g_signal_handler_block(priv->manager, priv->signalManagerChangedUnknownDomainPolicyID);

	state=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->blockUnknownDomainsCheckbox));
	policy=(state ? NOJS_POLICY_BLOCK : NOJS_POLICY_ACCEPT);
	nojs_set_policy_for_unknown_domain(priv->manager, policy);

	g_signal_handler_unblock(priv->manager, priv->signalManagerChangedUnknownDomainPolicyID);
}

static void _nojs_preferences_on_manager_unknown_domain_policy_changed(NoJSPreferences *self,
																		GParamSpec *inSpec,
																		gpointer inUserData)
{
	NoJSPreferencesPrivate	*priv=self->priv;
	NoJS					*manager=NOJS(inUserData);
	NoJSPolicy				policy;
	gboolean				state;

	/* Get new value from manager */
	policy=nojs_get_policy_for_unknown_domain(manager);

	/* Set toggle in widget (but block signal for toggle) */
	g_signal_handler_block(priv->blockUnknownDomainsCheckbox, priv->signalBlockUnknownDomainsToggledID);

	state=(policy==NOJS_POLICY_BLOCK ? TRUE : FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->blockUnknownDomainsCheckbox), state);

	g_signal_handler_unblock(priv->blockUnknownDomainsCheckbox, priv->signalBlockUnknownDomainsToggledID);
}

/* Only-second-level changed in check-box or manager */
static void _nojs_preferences_on_check_second_level_only_changed(NoJSPreferences *self,
																	gpointer *inUserData)
{
	NoJSPreferencesPrivate	*priv=self->priv;
	gboolean				state;

	/* Get toggle state of widget (but block signal for manager) and set in manager */
	g_signal_handler_block(priv->manager, priv->signalManagerChangedCheckSecondLevelID);

	state=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->checkSecondLevelOnlyCheckbox));
	nojs_set_only_second_level_domain(priv->manager, state);

	g_signal_handler_unblock(priv->manager, priv->signalManagerChangedCheckSecondLevelID);
}

static void _nojs_preferences_on_manager_only_second_level_changed(NoJSPreferences *self,
																	GParamSpec *inSpec,
																	gpointer inUserData)
{
	NoJSPreferencesPrivate	*priv=self->priv;
	NoJS					*manager=NOJS(inUserData);
	gboolean				state;

	/* Get new value from manager */
	state=nojs_get_only_second_level_domain(manager);

	/* Set toggle in widget (but block signal for toggle) */
	g_signal_handler_block(priv->checkSecondLevelOnlyCheckbox, priv->signalCheckSecondLevelOnlyToggledID);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkSecondLevelOnlyCheckbox), state);

	g_signal_handler_unblock(priv->checkSecondLevelOnlyCheckbox, priv->signalCheckSecondLevelOnlyToggledID);
}

/* Selection in list changed */
void _nojs_preferences_changed_selection(NoJSPreferences *self,
											GtkTreeSelection *inSelection)
{
	gboolean	selected=(gtk_tree_selection_count_selected_rows(inSelection)>0 ? TRUE: FALSE);

	gtk_widget_set_sensitive(self->priv->deleteButton, selected);
}

/* Delete button was clicked on selection */
void _nojs_preferences_on_delete_selection(NoJSPreferences *self,
											GtkButton *inButton)
{
	NoJSPreferencesPrivate	*priv=self->priv;
	GList					*rows, *row, *refs=NULL;
	GtkTreeRowReference		*ref;
	GtkTreeModel			*model=GTK_TREE_MODEL(priv->listStore);
	GtkTreeIter				iter;
	GtkTreePath				*path;
	gchar					*domain;
	gchar					*sql;
	gint					success;
	gchar					*error;

	/* Get selected rows in list and create a row reference because
	 * we will modify the model while iterating through selected rows
	 */
	rows=gtk_tree_selection_get_selected_rows(priv->listSelection, &model);
	for(row=rows; row; row=row->next)
	{
		ref=gtk_tree_row_reference_new(model, (GtkTreePath*)row->data);
		refs=g_list_prepend(refs, ref);
	}
	g_list_foreach(rows,(GFunc)gtk_tree_path_free, NULL);
	g_list_free(rows);

	/* Delete each selected row by its reference */
	for(row=refs; row; row=row->next)
	{
		/* Get domain from selected row */
		path=gtk_tree_row_reference_get_path((GtkTreeRowReference*)row->data);
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_model_get(model, &iter, DOMAIN_COLUMN, &domain, -1);

		/* Delete domain from database */
		sql=sqlite3_mprintf("DELETE FROM policies WHERE site='%q';", domain);
		success=sqlite3_exec(priv->database,
								sql,
								NULL,
								NULL,
								&error);
		if(success!=SQLITE_OK || error)
		{
			if(error)
			{
				g_critical(_("Failed to execute database statement: %s"), error);
				sqlite3_free(error);
			}
				else g_critical(_("Failed to execute database statement: %s"), sqlite3_errmsg(priv->database));
		}
		sqlite3_free(sql);

		/* Delete row from model */
		gtk_list_store_remove(priv->listStore, &iter);
	}
	g_list_foreach(refs,(GFunc)gtk_tree_row_reference_free, NULL);
	g_list_free(refs);
}

/* Delete all button was clicked */
void _nojs_preferences_on_delete_all(NoJSPreferences *self,
										GtkButton *inButton)
{
	NoJSPreferencesPrivate	*priv=self->priv;
	gint					success;
	gchar					*error=NULL;
	GtkWidget				*dialog;
	gint					dialogResponse;

	/* Ask user if he really wants to delete all permissions */
	dialog=gtk_message_dialog_new(GTK_WINDOW(self),
									GTK_DIALOG_MODAL,
									GTK_MESSAGE_QUESTION,
									GTK_BUTTONS_YES_NO,
									_("Do you really want to delete all JavaScript permissions?"));

	gtk_window_set_title(GTK_WINDOW(dialog), _("Delete all JavaScript permissions?"));
	gtk_window_set_icon_name(GTK_WINDOW(dialog), GTK_STOCK_PROPERTIES);

	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
												_("This action will delete all JavaScript permissions. "
												  "You will be asked for permissions again for each web site visited."));

	dialogResponse=gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	if(dialogResponse==GTK_RESPONSE_NO) return;

	/* Delete all permission */
	success=sqlite3_exec(priv->database,
							"DELETE FROM policies;",
							NULL,
							NULL,
							&error);

	if(success!=SQLITE_OK || error)
	{
		if(error)
		{
			g_critical(_("Failed to execute database statement: %s"), error);
			sqlite3_free(error);
		}
	}

	/* Re-setup list */
	_nojs_preferences_fill(self);
}

/* Sorting callbacks */
static gint _nojs_preferences_sort_string_callback(GtkTreeModel *inModel,
													GtkTreeIter *inLeft,
													GtkTreeIter *inRight,
													gpointer inUserData)
{
	gchar		*left, *right;
	gint		column=GPOINTER_TO_INT(inUserData);
	gint		result;

	gtk_tree_model_get(inModel, inLeft, column, &left, -1);
	gtk_tree_model_get(inModel, inRight, column, &right, -1);

	result=g_strcmp0(left, right);

	g_free(left);
	g_free(right);

	return(result);
}

/* IMPLEMENTATION: GObject */

/* Finalize this object */
static void nojs_preferences_finalize(GObject *inObject)
{
	NoJSPreferencesPrivate	*priv=NOJS_PREFERENCES(inObject)->priv;

	/* Dispose allocated resources */
	if(priv->database) sqlite3_close(priv->database);
	priv->database=NULL;

	if(priv->manager)
	{
		if(priv->signalManagerChangedDatabaseID) g_signal_handler_disconnect(priv->manager, priv->signalManagerChangedDatabaseID);
		priv->signalManagerChangedDatabaseID=0;

		if(priv->signalManagerChangedAllowAllSitesID) g_signal_handler_disconnect(priv->manager, priv->signalManagerChangedAllowAllSitesID);
		priv->signalManagerChangedAllowAllSitesID=0;

		if(priv->signalManagerChangedUnknownDomainPolicyID) g_signal_handler_disconnect(priv->manager, priv->signalManagerChangedUnknownDomainPolicyID);
		priv->signalManagerChangedUnknownDomainPolicyID=0;

		if(priv->signalManagerChangedCheckSecondLevelID) g_signal_handler_disconnect(priv->manager, priv->signalManagerChangedCheckSecondLevelID);
		priv->signalManagerChangedCheckSecondLevelID=0;

		g_object_unref(priv->manager);
		priv->manager=NULL;
	}

	/* Call parent's class finalize method */
	G_OBJECT_CLASS(nojs_preferences_parent_class)->finalize(inObject);
}

/* Set/get properties */
static void nojs_preferences_set_property(GObject *inObject,
											guint inPropID,
											const GValue *inValue,
											GParamSpec *inSpec)
{
	NoJSPreferences			*self=NOJS_PREFERENCES(inObject);
	NoJSPreferencesPrivate	*priv=self->priv;
	GObject					*manager;

	switch(inPropID)
	{
		/* Construct-only properties */
		case PROP_MANAGER:
			/* Release old manager if available and disconnect signals */
			if(priv->manager)
			{
				if(priv->signalManagerChangedDatabaseID) g_signal_handler_disconnect(priv->manager, priv->signalManagerChangedDatabaseID);
				priv->signalManagerChangedDatabaseID=0;

				if(priv->signalManagerChangedAllowAllSitesID) g_signal_handler_disconnect(priv->manager, priv->signalManagerChangedAllowAllSitesID);
				priv->signalManagerChangedAllowAllSitesID=0;

				if(priv->signalManagerChangedUnknownDomainPolicyID) g_signal_handler_disconnect(priv->manager, priv->signalManagerChangedUnknownDomainPolicyID);
				priv->signalManagerChangedUnknownDomainPolicyID=0;

				if(priv->signalManagerChangedCheckSecondLevelID) g_signal_handler_disconnect(priv->manager, priv->signalManagerChangedCheckSecondLevelID);
				priv->signalManagerChangedCheckSecondLevelID=0;

				g_object_unref(priv->manager);
				priv->manager=NULL;
			}

			/* Set new JavaScript permission manager and
			 * listen to changes in database property
			 */
			manager=g_value_get_object(inValue);
			if(manager)
			{
				priv->manager=g_object_ref(manager);

				priv->signalManagerChangedDatabaseID=
					g_signal_connect_swapped(priv->manager,
												"notify::database-filename",
												G_CALLBACK(_nojs_preferences_on_manager_database_changed),
												self);
				_nojs_preferences_on_manager_database_changed(self, NULL, priv->manager);

				priv->signalManagerChangedAllowAllSitesID=
					g_signal_connect_swapped(priv->manager,
												"notify::allow-all-sites",
												G_CALLBACK(_nojs_preferences_on_manager_allow_all_sites_changed),
												self);
				_nojs_preferences_on_manager_allow_all_sites_changed(self, NULL, priv->manager);

				priv->signalManagerChangedUnknownDomainPolicyID=
					g_signal_connect_swapped(priv->manager,
												"notify::unknown-domain-policy",
												G_CALLBACK(_nojs_preferences_on_manager_unknown_domain_policy_changed),
												self);
				_nojs_preferences_on_manager_unknown_domain_policy_changed(self, NULL, priv->manager);

				priv->signalManagerChangedCheckSecondLevelID=
					g_signal_connect_swapped(priv->manager,
												"notify::only-second-level",
												G_CALLBACK(_nojs_preferences_on_manager_only_second_level_changed),
												self);
				_nojs_preferences_on_manager_only_second_level_changed(self, NULL, priv->manager);
			}
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(inObject, inPropID, inSpec);
			break;
	}
}

static void nojs_preferences_get_property(GObject *inObject,
											guint inPropID,
											GValue *outValue,
											GParamSpec *inSpec)
{
	NoJSPreferences	*self=NOJS_PREFERENCES(inObject);

	switch(inPropID)
	{
		case PROP_MANAGER:
			g_value_set_object(outValue, self->priv->manager);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(inObject, inPropID, inSpec);
			break;
	}
}

/* Class initialization
 * Override functions in parent classes and define properties and signals
 */
static void nojs_preferences_class_init(NoJSPreferencesClass *klass)
{
	GObjectClass		*gobjectClass=G_OBJECT_CLASS(klass);

	/* Override functions */
	gobjectClass->finalize=nojs_preferences_finalize;
	gobjectClass->set_property=nojs_preferences_set_property;
	gobjectClass->get_property=nojs_preferences_get_property;

	/* Set up private structure */
	g_type_class_add_private(klass, sizeof(NoJSPreferencesPrivate));

	/* Define properties */
	NoJSPreferencesProperties[PROP_MANAGER]=
		g_param_spec_object("manager",
								_("Manager instance"),
								_("Instance to global NoJS manager"),
								TYPE_NOJS,
								G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties(gobjectClass, PROP_LAST, NoJSPreferencesProperties);
}

/* Object initialization
 * Create private structure and set up default values
 */
static void nojs_preferences_init(NoJSPreferences *self)
{
	NoJSPreferencesPrivate		*priv;
	GtkTreeSortable				*sortableList;
	GtkCellRenderer				*renderer;
	GtkTreeViewColumn			*column;
	GtkWidget					*widget;
	gchar						*dialogTitle;
	GtkWidget					*scrolled;
	GtkWidget					*vbox;
	GtkWidget					*hbox;
	gint						width, height;

	priv=self->priv=NOJS_PREFERENCES_GET_PRIVATE(self);

	/* Set up default values */
	priv->manager=NULL;

	/* Get content area to add gui controls to */
	priv->contentArea=gtk_dialog_get_content_area(GTK_DIALOG(self));
#if GTK_CHECK_VERSION (3, 0, 0)
	vbox=gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);
#else
	vbox=gtk_vbox_new(FALSE, 0);
#endif

	/* Set up dialog */
	dialogTitle=_("Configure NoJS");

	gtk_window_set_title(GTK_WINDOW(self), dialogTitle);
	gtk_window_set_icon_name(GTK_WINDOW(self), GTK_STOCK_PROPERTIES);

	sokoke_widget_get_text_size(GTK_WIDGET(self), "M", &width, &height);
	gtk_window_set_default_size(GTK_WINDOW(self), width*52, -1);

	widget=sokoke_xfce_header_new(gtk_window_get_icon_name(GTK_WINDOW(self)), dialogTitle);
	if(widget) gtk_box_pack_start(GTK_BOX(priv->contentArea), widget, FALSE, FALSE, 0);

	gtk_dialog_add_button(GTK_DIALOG(self), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

	/* Set up description */
	widget=gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(widget),
            _("Below is a list of all web sites and the policy set for them. "
							"You can delete policies by marking the entries and clicking on <i>Delete</i>."));
	gtk_label_set_line_wrap(GTK_LABEL(widget), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, FALSE, 4);

	/* Set up domain list */
	priv->listStore=gtk_list_store_new(N_COLUMN,
										G_TYPE_STRING,	/* DOMAIN_COLUMN */
										G_TYPE_STRING	/* POLICY_COLUMN */);

	sortableList=GTK_TREE_SORTABLE(priv->listStore);
	gtk_tree_sortable_set_sort_func(sortableList,
										DOMAIN_COLUMN,
										(GtkTreeIterCompareFunc)_nojs_preferences_sort_string_callback,
										GINT_TO_POINTER(DOMAIN_COLUMN),
										NULL);
	gtk_tree_sortable_set_sort_func(sortableList,
										POLICY_COLUMN,
										(GtkTreeIterCompareFunc)_nojs_preferences_sort_string_callback,
										GINT_TO_POINTER(POLICY_COLUMN),
										NULL);
	gtk_tree_sortable_set_sort_column_id(sortableList, DOMAIN_COLUMN, GTK_SORT_ASCENDING);

	/* Set up domain list view */
	priv->list=gtk_tree_view_new_with_model(GTK_TREE_MODEL(priv->listStore));

#if !GTK_CHECK_VERSION (3, 0, 0)
	gtk_widget_set_size_request(priv->list, -1, 300);
#endif

	priv->listSelection=gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->list));
	gtk_tree_selection_set_mode(priv->listSelection, GTK_SELECTION_MULTIPLE);
	g_signal_connect_swapped(priv->listSelection, "changed", G_CALLBACK(_nojs_preferences_changed_selection), self);

	renderer=gtk_cell_renderer_text_new();
	column=gtk_tree_view_column_new_with_attributes(_("Domain"),
													renderer,
													"text", DOMAIN_COLUMN,
													NULL);
	gtk_tree_view_column_set_sort_column_id(column, DOMAIN_COLUMN);
	gtk_tree_view_append_column(GTK_TREE_VIEW(priv->list), column);

	renderer=gtk_cell_renderer_text_new();
	column=gtk_tree_view_column_new_with_attributes(_("Policy"),
													renderer,
													"text", POLICY_COLUMN,
													NULL);
	gtk_tree_view_column_set_sort_column_id(column, POLICY_COLUMN);
	gtk_tree_view_append_column(GTK_TREE_VIEW(priv->list), column);

	scrolled=gtk_scrolled_window_new(NULL, NULL);
#if GTK_CHECK_VERSION (3, 0, 0)
	gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), height*10);
#endif
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolled), priv->list);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 5);

	/* Set up JavaScript domain list management buttons */
#if GTK_CHECK_VERSION (3, 0, 0)
	hbox=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous(GTK_BOX(hbox), FALSE);
#else
	hbox=gtk_hbox_new(FALSE, 0);
#endif

	priv->deleteButton=gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_widget_set_sensitive(priv->deleteButton, FALSE);
	gtk_container_add(GTK_CONTAINER(hbox), priv->deleteButton);
	g_signal_connect_swapped(priv->deleteButton, "clicked", G_CALLBACK(_nojs_preferences_on_delete_selection), self);

	priv->deleteAllButton=gtk_button_new_with_mnemonic(_("Delete _all"));
	gtk_button_set_image(GTK_BUTTON(priv->deleteAllButton), gtk_image_new_from_stock(GTK_STOCK_DELETE, GTK_ICON_SIZE_BUTTON));
	gtk_widget_set_sensitive(priv->deleteAllButton, FALSE);
	gtk_container_add(GTK_CONTAINER(hbox), priv->deleteAllButton);
	g_signal_connect_swapped(priv->deleteAllButton, "clicked", G_CALLBACK(_nojs_preferences_on_delete_all), self);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 5);

	/* Add "allow-all-sites" checkbox */
	priv->allowAllSitesCheckbox=gtk_check_button_new_with_mnemonic(_("A_llow scripts at all sites"));
	priv->signalAllowAllSitesToggledID=g_signal_connect_swapped(priv->allowAllSitesCheckbox,
																"toggled",
																G_CALLBACK(_nojs_preferences_on_allow_all_sites_changed),
																self);
	gtk_box_pack_start(GTK_BOX(vbox), priv->allowAllSitesCheckbox, FALSE, TRUE, 5);

	/* Add "block-unknown-domains" checkbox */
	priv->blockUnknownDomainsCheckbox=gtk_check_button_new_with_mnemonic(_("Bloc_k scripts at unknown domains by default"));
	priv->signalBlockUnknownDomainsToggledID=g_signal_connect_swapped(priv->blockUnknownDomainsCheckbox,
																		"toggled",
																		G_CALLBACK(_nojs_preferences_on_block_unknown_domains_changed),
																		self);
	gtk_box_pack_start(GTK_BOX(vbox), priv->blockUnknownDomainsCheckbox, FALSE, TRUE, 5);

	/* Add "check-second-level-only" checkbox */
	priv->checkSecondLevelOnlyCheckbox=gtk_check_button_new_with_mnemonic(_("S_et permissions on second-level domain"));
	priv->signalCheckSecondLevelOnlyToggledID=g_signal_connect_swapped(priv->checkSecondLevelOnlyCheckbox,
																		"toggled",
																		G_CALLBACK(_nojs_preferences_on_check_second_level_only_changed),
																		self);
	gtk_box_pack_start(GTK_BOX(vbox), priv->checkSecondLevelOnlyCheckbox, FALSE, TRUE, 5);

	/* Finalize setup of content area */
	gtk_box_pack_start(GTK_BOX(priv->contentArea), vbox, TRUE, TRUE, 0);
}

/* Implementation: Public API */

/* Create new object */
GtkWidget* nojs_preferences_new(NoJS *inManager)
{
	return(g_object_new(TYPE_NOJS_PREFERENCES,
							"manager", inManager,
							NULL));
}
