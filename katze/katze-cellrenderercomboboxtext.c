/*
 Copyright (C) 2008-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "katze-cellrenderercomboboxtext.h"

#include "marshal.h"

#include <string.h>
#include <gdk/gdk.h>

#define P_(String) (String)
#define I_(String) (String)
#define GTK_PARAM_READABLE G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_WRITABLE G_PARAM_WRITABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_READWRITE G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB


static void
katze_cell_renderer_combobox_text_finalize (GObject* object);

static void
katze_cell_renderer_combobox_text_get_property (GObject*    object,
    guint       param_id,
    GValue*     value,
    GParamSpec* pspec);
static void
katze_cell_renderer_combobox_text_set_property (GObject*      object,
    guint         param_id,
    const GValue* value,
    GParamSpec*   pspec);
static void
katze_cell_renderer_combobox_text_get_size (GtkCellRenderer* cell,
    GtkWidget*       widget,
#if GTK_CHECK_VERSION(3,0,0)
    const GdkRectangle*      cell_area,
#else
    GdkRectangle*            cell_area,
#endif
    gint*            x_offset,
    gint*            y_offset,
    gint*            width,
    gint*            height);
#if GTK_CHECK_VERSION(3,0,0)
static void
katze_cell_renderer_combobox_text_render (GtkCellRenderer      *cell,
    cairo_t*             cr,
    GtkWidget            *widget,
    const GdkRectangle   *background_area,
    const GdkRectangle   *cell_area,
    GtkCellRendererState  flags);
#else
static void
katze_cell_renderer_combobox_text_render (GtkCellRenderer      *cell,
    GdkDrawable          *window,
    GtkWidget            *widget,
    GdkRectangle         *background_area,
    GdkRectangle         *cell_area,
    GdkRectangle         *expose_area,
    GtkCellRendererState  flags);
#endif

enum {
    PROP_0,

    PROP_FOLDED_TEXT,
    PROP_FOLDED_MARKUP,
    PROP_FOLDED_ATTRIBUTES,
    PROP_UNFOLDED_TEXT,
    PROP_UNFOLDED_MARKUP,
    PROP_UNFOLDED_ATTRIBUTES,
};

#define KATZE_CELL_RENDERER_COMBOBOX_TEXT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), KATZE_TYPE_CELL_RENDERER_COMBOBOX_TEXT, KatzeCellRendererComboBoxTextPrivate))

typedef struct _KatzeCellRendererComboBoxTextPrivate KatzeCellRendererComboBoxTextPrivate;
struct _KatzeCellRendererComboBoxTextPrivate
{
    struct _Properties {
        PangoAttrList* extra_attrs;

        gchar* text;

        guint markup_set : 1;
    } props[2];
};

G_DEFINE_TYPE (KatzeCellRendererComboBoxText, katze_cell_renderer_combobox_text, GTK_TYPE_CELL_RENDERER_TEXT)

static void
katze_cell_renderer_combobox_text_init (KatzeCellRendererComboBoxText *celltext)
{
    guint prop_index;
    KatzeCellRendererComboBoxTextPrivate *priv;

    priv = KATZE_CELL_RENDERER_COMBOBOX_TEXT_GET_PRIVATE (celltext);

    for (prop_index = 0 ; prop_index < 2; prop_index++)
    {
        priv->props[prop_index].text = NULL;
        priv->props[prop_index].extra_attrs = NULL;
        priv->props[prop_index].markup_set = FALSE;
    }
}

static void
katze_cell_renderer_combobox_text_class_init (KatzeCellRendererComboBoxTextClass *class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (class);
    GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

    object_class->finalize = katze_cell_renderer_combobox_text_finalize;

    object_class->get_property = katze_cell_renderer_combobox_text_get_property;
    object_class->set_property = katze_cell_renderer_combobox_text_set_property;

    cell_class->get_size = katze_cell_renderer_combobox_text_get_size;
    cell_class->render = katze_cell_renderer_combobox_text_render;

    g_object_class_install_property (object_class,
        PROP_FOLDED_TEXT,
        g_param_spec_string ("folded-text",
            P_("Folded text"),
            P_("Text to render if combobox_text is closed. The string [text] is replaced by default text"),
            NULL,
            GTK_PARAM_READWRITE));

    g_object_class_install_property (object_class,
        PROP_FOLDED_MARKUP,
        g_param_spec_string ("folded-markup",
            P_("Folded markup"),
            P_("Marked up text to render if combobox_text is closed. The string [text] is replaced by default text"),
            NULL,
            GTK_PARAM_WRITABLE));

    g_object_class_install_property (object_class,
        PROP_FOLDED_ATTRIBUTES,
        g_param_spec_boxed ("folded-attributes",
            P_("Folded attributes"),
            P_("A list of style attributes to apply to the text of the renderer if combobox_text is closed"),
            PANGO_TYPE_ATTR_LIST,
            GTK_PARAM_READWRITE));

    g_object_class_install_property (object_class,
        PROP_UNFOLDED_TEXT,
        g_param_spec_string ("unfolded-text",
            P_("Unfolded text"),
            P_("Text to render if combobox_text is opened"),
            NULL,
            GTK_PARAM_READWRITE));

    g_object_class_install_property (object_class,
        PROP_UNFOLDED_MARKUP,
        g_param_spec_string ("unfolded-markup",
            P_("Unfolded markup"),
            P_("Marked up text to render if combobox_text is opened"),
            NULL,
            GTK_PARAM_WRITABLE));

    g_object_class_install_property (object_class,
        PROP_UNFOLDED_ATTRIBUTES,
        g_param_spec_boxed ("unfolded-attributes",
            P_("Unfolded attributes"),
            P_("A list of style attributes to apply to the text of the renderer if combobox_text is opened"),
            PANGO_TYPE_ATTR_LIST,
            GTK_PARAM_READWRITE));


    g_type_class_add_private (object_class, sizeof (KatzeCellRendererComboBoxTextPrivate));
}

static void
katze_cell_renderer_combobox_text_finalize (GObject *object)
{
    guint prop_index;
    KatzeCellRendererComboBoxTextPrivate *priv;

    priv = KATZE_CELL_RENDERER_COMBOBOX_TEXT_GET_PRIVATE (object);

    for (prop_index = 0 ; prop_index < 2; prop_index++)
    {
        g_free (priv->props[prop_index].text);
        if (priv->props[prop_index].extra_attrs)
            pango_attr_list_unref (priv->props[prop_index].extra_attrs);
    }

    G_OBJECT_CLASS (katze_cell_renderer_combobox_text_parent_class)->finalize (object);
}

static void
katze_cell_renderer_combobox_text_get_property (GObject*    object,
					 guint       param_id,
					 GValue*     value,
					 GParamSpec* pspec)
{
    KatzeCellRendererComboBoxTextPrivate *priv;

    priv = KATZE_CELL_RENDERER_COMBOBOX_TEXT_GET_PRIVATE (object);

    switch (param_id)
    {
        case PROP_FOLDED_TEXT:
            g_value_set_string (value, priv->props[0].text);
            break;

        case PROP_FOLDED_ATTRIBUTES:
            g_value_set_boxed (value, priv->props[0].extra_attrs);
            break;

        case PROP_UNFOLDED_TEXT:
            g_value_set_string (value, priv->props[1].text);
            break;

        case PROP_UNFOLDED_ATTRIBUTES:
            g_value_set_boxed (value, priv->props[1].extra_attrs);
            break;

        case PROP_FOLDED_MARKUP:
        case PROP_UNFOLDED_MARKUP:
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
            break;
    }
}


static void
katze_cell_renderer_combobox_text_set_property (GObject*      object,
					 guint         param_id,
					 const GValue* value,
					 GParamSpec*   pspec)
{
    guint prop_index = 0;
    KatzeCellRendererComboBoxTextPrivate *priv;

    priv = KATZE_CELL_RENDERER_COMBOBOX_TEXT_GET_PRIVATE (object);

    switch (param_id)
    {
        case PROP_FOLDED_TEXT:
        prop_text:
            g_free (priv->props[prop_index].text);

            if (priv->props[prop_index].markup_set)
            {
                if (priv->props[prop_index].extra_attrs)
                {
                    pango_attr_list_unref (priv->props[prop_index].extra_attrs);
                    priv->props[prop_index].extra_attrs = NULL;
                }
                priv->props[prop_index].markup_set = FALSE;
            }

            priv->props[prop_index].text = g_value_dup_string (value);
            break;

        case PROP_FOLDED_ATTRIBUTES:
        prop_attributes:
            if (priv->props[prop_index].extra_attrs)
                pango_attr_list_unref (priv->props[prop_index].extra_attrs);

            priv->props[prop_index].extra_attrs = g_value_get_boxed (value);
            if (priv->props[prop_index].extra_attrs)
                pango_attr_list_ref (priv->props[prop_index].extra_attrs);
            break;

        case PROP_FOLDED_MARKUP:
        prop_markup:
        {
            const gchar *str;
            gchar *text = NULL;
            GError *error = NULL;
            PangoAttrList *attrs = NULL;

            str = g_value_get_string (value);
            if (str && !pango_parse_markup (str,
					-1,
					0,
					&attrs,
					&text,
					NULL,
					&error))
            {
                g_warning ("Failed to set text from markup due to error parsing markup: %s",
                    error->message);
                g_error_free (error);
                return;
            }

            g_free (priv->props[prop_index].text);

            if (priv->props[prop_index].extra_attrs)
                pango_attr_list_unref (priv->props[prop_index].extra_attrs);

            priv->props[prop_index].text = text;
            priv->props[prop_index].extra_attrs = attrs;
            priv->props[prop_index].markup_set = TRUE;
        }
        break;

        case PROP_UNFOLDED_TEXT:
            prop_index = 1;
            goto prop_text;

        case PROP_UNFOLDED_ATTRIBUTES:
            prop_index = 1;
            goto prop_attributes;

        case PROP_UNFOLDED_MARKUP:
            prop_index = 1;
            goto prop_markup;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
            break;
    }
}

/**
 * katze_cell_renderer_combobox_text_new:
 *
 * Creates a new #KatzeCellRendererComboBoxText. Adjust how text is drawn using
 * object properties. Object properties can be
 * set globally (with g_object_set()). Also, with #GtkTreeViewColumn,
 * you can bind a property to a value in a #GtkTreeModel. For example,
 * you can bind the "text" property on the cell renderer to a string
 * value in the model, thus rendering a different string in each row
 * of the #GtkTreeView
 *
 * Return value: (transfer full): the new cell renderer
 **/
GtkCellRenderer *
katze_cell_renderer_combobox_text_new (void)
{
    return g_object_new (KATZE_TYPE_CELL_RENDERER_COMBOBOX_TEXT, NULL);
}

static void
set_text(KatzeCellRendererComboBoxText* cell,
    GtkWidget* widget,
    const gchar* repl)
{
    const gchar *text = NULL;
    PangoAttrList* extra_attrs = NULL;
    GtkWidget* pwidget = gtk_widget_get_parent (widget);
    gboolean unfolded = FALSE;
    KatzeCellRendererComboBoxTextPrivate *priv;

    priv = KATZE_CELL_RENDERER_COMBOBOX_TEXT_GET_PRIVATE (cell);

    if (GTK_IS_MENU_ITEM (pwidget))
    {
        GtkWidget* menu = gtk_widget_get_parent (pwidget);
        GList* items;

        if (menu
            && (GTK_IS_MENU (menu))
            && (items = gtk_container_get_children (GTK_CONTAINER (menu)))
            && (GTK_WIDGET (items->data) == pwidget)
            && (g_list_length (items) > 1)
            && (GTK_IS_SEPARATOR_MENU_ITEM (g_list_next (items)->data)))
        {
            unfolded = TRUE;
        }
    }

    if (unfolded)
    {
        text = priv->props[1].text;
        extra_attrs = priv->props[1].extra_attrs;
    }
    else
    {
        text = priv->props[0].text;
        extra_attrs = priv->props[0].extra_attrs;
    }

    if (!text)
    {
        text = g_strdup (repl ? repl : "");
    }
    else
    {
        GString* string = g_string_new ("");
        const gchar* src = text;
        const guint skip = sizeof ("[text]") - 1;
        guint len;

        while (0 != (len = strlen(src)))
        {
            const gchar* found = strstr (src, "[text]");

            if (!found)
            {
                g_string_append (string, src);
                src += len;
            }
            else
            {
                g_string_append_len (string, src, found-src);
                if (repl)
                    g_string_append (string, repl);
                src = found + skip;
            }
        }

        text = g_string_free (string, FALSE);
    }

    g_object_set (G_OBJECT (cell),
        "text", text,
        "attributes", extra_attrs,
        NULL);

    g_free ((gpointer)text);
}

static void
katze_cell_renderer_combobox_text_get_size (GtkCellRenderer *cell,
				 GtkWidget       *widget,
#if GTK_CHECK_VERSION(3,0,0)
                                 const GdkRectangle* cell_area,
#else
                                 GdkRectangle*       cell_area,
#endif
				 gint            *x_offset,
				 gint            *y_offset,
				 gint            *width,
				 gint            *height)
{
    const gchar *text = NULL;

    g_object_get (G_OBJECT (cell), "text", &text, NULL);

    set_text (KATZE_CELL_RENDERER_COMBOBOX_TEXT (cell), widget, text);

    GTK_CELL_RENDERER_CLASS (katze_cell_renderer_combobox_text_parent_class)->get_size (cell,
        widget, cell_area,
        x_offset, y_offset, width, height);

    g_object_set (G_OBJECT (cell), "text", text, NULL);
    g_free ((gpointer)text);
}

static void
#if GTK_CHECK_VERSION(3,0,0)
katze_cell_renderer_combobox_text_render (GtkCellRenderer      *cell,
			       cairo_t*             cr,
			       GtkWidget            *widget,
			       const GdkRectangle   *background_area,
			       const GdkRectangle   *cell_area,
			       GtkCellRendererState  flags)
#else
katze_cell_renderer_combobox_text_render (GtkCellRenderer      *cell,
			       GdkDrawable          *window,
			       GtkWidget            *widget,
			       GdkRectangle         *background_area,
			       GdkRectangle         *cell_area,
			       GdkRectangle         *expose_area,
			       GtkCellRendererState  flags)
#endif
{
    const gchar *text = NULL;

    g_object_get (G_OBJECT (cell), "text", &text, NULL);

    set_text (KATZE_CELL_RENDERER_COMBOBOX_TEXT (cell), widget, text);

#if GTK_CHECK_VERSION(3,0,0)
    GTK_CELL_RENDERER_CLASS (katze_cell_renderer_combobox_text_parent_class)->render (cell,
        cr,
        widget,
        background_area,
        cell_area,
        flags);
#else
    GTK_CELL_RENDERER_CLASS (katze_cell_renderer_combobox_text_parent_class)->render (cell,
        window,
        widget,
        background_area,
        cell_area,
        expose_area,
        flags);
#endif

    g_object_set (G_OBJECT (cell), "text", text, NULL);
    g_free ((gpointer)text);
}
