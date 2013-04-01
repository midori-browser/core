/*
 Copyright (C) 2007-2010 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "katze-throbber.h"

#include "katze-utils.h"

#include <glib/gi18n.h>
#include <math.h>

#define HAVE_SPINNER GTK_CHECK_VERSION (2, 20, 0)

struct _KatzeThrobber
{
    #if HAVE_SPINNER
    GtkSpinner parent_instance;
    #else
    GtkMisc parent_instance;
    #endif

    GtkIconSize icon_size;
    gchar* icon_name;
    GdkPixbuf* pixbuf;
    gchar* stock_id;
    gboolean animated;
    gchar* static_icon_name;
    GdkPixbuf* static_pixbuf;
    gchar* static_stock_id;

    gint index;
    gint timer_id;
    gint width;
    gint height;
};

struct _KatzeThrobberClass
{
    #if HAVE_SPINNER
    GtkSpinnerClass parent_class;
    #else
    GtkMiscClass parent_class;
    #endif
};

#if HAVE_SPINNER
G_DEFINE_TYPE (KatzeThrobber, katze_throbber, GTK_TYPE_SPINNER);
#else
G_DEFINE_TYPE (KatzeThrobber, katze_throbber, GTK_TYPE_MISC);
#endif

enum
{
    PROP_0,

    PROP_ICON_SIZE,
    PROP_ICON_NAME,
    PROP_PIXBUF,
    PROP_ANIMATED,
    PROP_STATIC_ICON_NAME,
    PROP_STATIC_PIXBUF,
    PROP_STATIC_STOCK_ID
};

static void
katze_throbber_dispose (GObject* object);

static void
katze_throbber_set_property (GObject* object,
                             guint prop_id,
                             const GValue* value,
                             GParamSpec* pspec);

static void
katze_throbber_get_property (GObject* object,
                             guint prop_id,
                             GValue* value,
                             GParamSpec* pspec);

static void
#if GTK_CHECK_VERSION (3, 0, 0)
katze_throbber_destroy (GtkWidget* object);
#else
katze_throbber_destroy (GtkObject* object);
#endif

static void
katze_throbber_realize (GtkWidget* widget);

#if GTK_CHECK_VERSION (3, 0, 0)
static void
katze_throbber_get_preferred_height (GtkWidget *widget,
                                     gint      *minimal_height,
                                     gint      *natural_height);
static void
katze_throbber_get_preferred_width (GtkWidget *widget,
                                     gint      *minimal_width,
                                     gint      *natural_width);
#endif
static void
katze_throbber_unrealize (GtkWidget* widget);

static void
katze_throbber_map (GtkWidget* widget);

static void
katze_throbber_unmap (GtkWidget* widget);

static void
katze_throbber_style_set (GtkWidget* widget,
                          GtkStyle* style);

static void
katze_throbber_screen_changed (GtkWidget* widget,
                               GdkScreen* screen_prev);

static void
katze_throbber_size_request (GtkWidget*      widget,
                             GtkRequisition* requisition);

static gboolean
katze_throbber_expose_event (GtkWidget*      widget,
#if GTK_CHECK_VERSION (3, 0, 0)
                             cairo_t* cr);
#else
                             GdkEventExpose* event);
#endif

static void
icon_theme_changed (KatzeThrobber* throbber);

#if !HAVE_SPINNER
static gboolean
katze_throbber_timeout (KatzeThrobber* throbber);

static void
katze_throbber_timeout_destroy (KatzeThrobber* throbber);
#endif

static void
katze_throbber_class_init (KatzeThrobberClass* class)
{
    GObjectClass* gobject_class;
#if !GTK_CHECK_VERSION (3, 0, 0)
    GtkObjectClass* object_class;
#endif
    GtkWidgetClass* widget_class;
    GParamFlags flags = G_PARAM_READWRITE | G_PARAM_CONSTRUCT;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->dispose = katze_throbber_dispose;
    gobject_class->set_property = katze_throbber_set_property;
    gobject_class->get_property = katze_throbber_get_property;

    widget_class = GTK_WIDGET_CLASS (class);
#if GTK_CHECK_VERSION (3, 0, 0)
    widget_class->destroy = katze_throbber_destroy;
#else
    object_class = GTK_OBJECT_CLASS (class);
    object_class->destroy = katze_throbber_destroy;
#endif
    widget_class->realize = katze_throbber_realize;
    widget_class->unrealize = katze_throbber_unrealize;
    widget_class->map = katze_throbber_map;
    widget_class->unmap = katze_throbber_unmap;
    widget_class->style_set = katze_throbber_style_set;
    widget_class->screen_changed = katze_throbber_screen_changed;
#if GTK_CHECK_VERSION (3, 0, 0)
    widget_class->get_preferred_width= katze_throbber_get_preferred_width;
    widget_class->get_preferred_height= katze_throbber_get_preferred_height;
#else
    widget_class->size_request = katze_throbber_size_request;
#endif
#if GTK_CHECK_VERSION (3, 0, 0)
    widget_class->draw = katze_throbber_expose_event;
#else
    widget_class->expose_event = katze_throbber_expose_event;
#endif
    g_object_class_install_property (gobject_class,
                                     PROP_ICON_SIZE,
                                     g_param_spec_int (
                                     "icon-size",
                                     "Icon size",
                                     "Symbolic size to use for the animation",
                                     0, G_MAXINT, GTK_ICON_SIZE_MENU,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_ICON_NAME,
                                     g_param_spec_string (
                                     "icon-name",
                                     "Icon Name",
                                     "The name of an icon containing animation frames",
                                     "process-working",
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_PIXBUF,
                                     g_param_spec_object (
                                     "pixbuf",
                                     "Pixbuf",
                                     "A GdkPixbuf containing animation frames",
                                     GDK_TYPE_PIXBUF,
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_ANIMATED,
                                     g_param_spec_boolean (
                                     "animated",
                                     "Animated",
                                     "Whether the throbber should be animated",
                                     FALSE,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_STATIC_ICON_NAME,
                                     g_param_spec_string (
                                     "static-icon-name",
                                     "Static Icon Name",
                                     "The name of an icon to be used as the static image",
                                     NULL,
                                     flags));

    g_object_class_install_property (gobject_class,
                                     PROP_STATIC_PIXBUF,
                                     g_param_spec_object (
                                     "static-pixbuf",
                                     "Static Pixbuf",
                                     "A GdkPixbuf to be used as the static image",
                                     GDK_TYPE_PIXBUF,
                                     G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_STATIC_STOCK_ID,
                                     g_param_spec_string (
                                     "static-stock-id",
                                     "Static Stock ID",
                                     "The stock ID of an icon to be used as the static image",
                                     NULL,
                                     flags));
}

static void
katze_throbber_init (KatzeThrobber *throbber)
{
    gtk_widget_set_has_window (GTK_WIDGET (throbber), FALSE);
    #if !HAVE_SPINNER
    gtk_misc_set_alignment (GTK_MISC (throbber), 0.0, 0.5);
    #endif

    throbber->timer_id = -1;
}

static void
katze_throbber_dispose (GObject* object)
{
    KatzeThrobber* throbber = KATZE_THROBBER (object);

    if (G_UNLIKELY (throbber->timer_id >= 0))
        g_source_remove (throbber->timer_id);

    (*G_OBJECT_CLASS (katze_throbber_parent_class)->dispose) (object);
}

static void
#if GTK_CHECK_VERSION (3, 0, 0)
katze_throbber_destroy (GtkWidget* object)
#else
katze_throbber_destroy (GtkObject* object)
#endif
{
    KatzeThrobber* throbber = KATZE_THROBBER (object);

    katze_assign (throbber->icon_name, NULL);
    katze_object_assign (throbber->pixbuf, NULL);
    katze_assign (throbber->static_icon_name, NULL);
    katze_object_assign (throbber->static_pixbuf, NULL);
    katze_assign (throbber->static_stock_id, NULL);
#if GTK_CHECK_VERSION (3, 0, 0)
    GTK_WIDGET_CLASS (katze_throbber_parent_class)->destroy (object);
#else
    GTK_OBJECT_CLASS (katze_throbber_parent_class)->destroy (object);
#endif
}

static void
katze_throbber_set_property (GObject*      object,
                             guint         prop_id,
                             const GValue* value,
                             GParamSpec*   pspec)
{
    KatzeThrobber* throbber = KATZE_THROBBER (object);

    switch (prop_id)
    {
    case PROP_ICON_SIZE:
        katze_throbber_set_icon_size (throbber, g_value_get_int (value));
        break;
    case PROP_ICON_NAME:
        katze_throbber_set_icon_name (throbber, g_value_get_string (value));
        break;
    case PROP_PIXBUF:
        katze_throbber_set_pixbuf (throbber, g_value_get_object (value));
        break;
    case PROP_ANIMATED:
        katze_throbber_set_animated (throbber, g_value_get_boolean (value));
        break;
    case PROP_STATIC_ICON_NAME:
        katze_throbber_set_static_icon_name (throbber, g_value_get_string (value));
        break;
    case PROP_STATIC_PIXBUF:
        katze_throbber_set_static_pixbuf (throbber, g_value_get_object (value));
        break;
    case PROP_STATIC_STOCK_ID:
        katze_throbber_set_static_stock_id (throbber, g_value_get_string (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
katze_throbber_get_property (GObject*    object,
                             guint       prop_id,
                             GValue*     value,
                             GParamSpec* pspec)
{
    KatzeThrobber* throbber = KATZE_THROBBER (object);

    switch (prop_id)
    {
    case PROP_ICON_SIZE:
        g_value_set_int (value, katze_throbber_get_icon_size (throbber));
        break;
    case PROP_ICON_NAME:
        g_value_set_string (value, katze_throbber_get_icon_name (throbber));
        break;
    case PROP_PIXBUF:
        g_value_set_object (value, katze_throbber_get_pixbuf (throbber));
        break;
    case PROP_ANIMATED:
        g_value_set_boolean (value, katze_throbber_get_animated (throbber));
        break;
    case PROP_STATIC_ICON_NAME:
        g_value_set_string (value, katze_throbber_get_static_icon_name (throbber));
        break;
    case PROP_STATIC_PIXBUF:
        g_value_set_object (value, katze_throbber_get_static_pixbuf (throbber));
        break;
    case PROP_STATIC_STOCK_ID:
        g_value_set_string (value, katze_throbber_get_static_stock_id (throbber));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/**
 * katze_throbber_new:
 *
 * Creates a new throbber widget.
 *
 * Return value: a new #KatzeThrobber
 **/
GtkWidget*
katze_throbber_new (void)
{
    KatzeThrobber* throbber = g_object_new (KATZE_TYPE_THROBBER,
                                            NULL);

    return GTK_WIDGET (throbber);
}

/**
 * katze_throbber_set_icon_size:
 * @throbber: a #KatzeThrobber
 * @icon_size: the new icon size
 *
 * Sets the desired size of the throbber image. The animation and static image
 * will be displayed in this size. If a pixbuf is used for the animation every
 * single frame is assumed to have this size.
 **/
void
katze_throbber_set_icon_size (KatzeThrobber* throbber,
                              GtkIconSize    icon_size)
{
    GtkSettings* gtk_settings;

    g_return_if_fail (KATZE_IS_THROBBER (throbber));
    gtk_settings = gtk_widget_get_settings (GTK_WIDGET (throbber));
    g_return_if_fail (gtk_icon_size_lookup_for_settings (gtk_settings,
                                                         icon_size,
                                                         &throbber->width,
                                                         &throbber->height));

    throbber->icon_size = icon_size;

    g_object_notify (G_OBJECT (throbber), "icon-size");

    gtk_widget_queue_draw (GTK_WIDGET (throbber));
}

/**
 * katze_throbber_set_icon_name:
 * @throbber: a #KatzeThrobber
 * @icon_name: an icon name or %NULL
 *
 * Sets the name of an icon that should provide the animation frames.
 *
 * The pixbuf is automatically invalidated.
 **/
void
katze_throbber_set_icon_name (KatzeThrobber*  throbber,
                              const gchar*    icon_name)
{
    g_return_if_fail (KATZE_IS_THROBBER (throbber));

    katze_assign (throbber->icon_name, g_strdup (icon_name));

    if (icon_name)
        icon_theme_changed (throbber);

    g_object_notify (G_OBJECT (throbber), "icon-name");
}

/**
 * katze_throbber_set_pixbuf:
 * @throbber: a #KatzeThrobber
 * @pixbuf: a #GdkPixbuf or %NULL
 *
 * Sets the pixbuf that should provide the animation frames. Every frame
 * is assumed to have the icon size of the throbber, which can be specified
 * with katze_throbber_set_icon_size ().
 *
 * The icon name is automatically invalidated.
 **/
void
katze_throbber_set_pixbuf (KatzeThrobber* throbber,
                           GdkPixbuf*     pixbuf)
{
    g_return_if_fail (KATZE_IS_THROBBER (throbber));
    g_return_if_fail (!pixbuf || GDK_IS_PIXBUF (pixbuf));

    katze_object_assign (throbber->pixbuf, pixbuf);

    g_object_freeze_notify (G_OBJECT (throbber));

    if (pixbuf)
    {
        g_object_ref (pixbuf);

        katze_assign (throbber->icon_name, NULL);
        g_object_notify (G_OBJECT (throbber), "icon-name");
    }

    gtk_widget_queue_draw (GTK_WIDGET (throbber));

    g_object_notify (G_OBJECT (throbber), "pixbuf");
    g_object_thaw_notify (G_OBJECT (throbber));
}

/**
 * katze_throbber_set_animated:
 * @throbber: a #KatzeThrobber
 * @animated: %TRUE to animate the throbber
 *
 * Sets the animation state of the throbber.
 **/
void
katze_throbber_set_animated (KatzeThrobber*  throbber,
                             gboolean        animated)
{
    g_return_if_fail (KATZE_IS_THROBBER (throbber));

    if (G_UNLIKELY (throbber->animated == animated))
        return;

    throbber->animated = animated;

    #if HAVE_SPINNER
    g_object_set (throbber, "active", animated, NULL);
    #else
    if (animated && (throbber->timer_id < 0))
        throbber->timer_id = midori_timeout_add (50,
            (GSourceFunc)katze_throbber_timeout, throbber,
            (GDestroyNotify)katze_throbber_timeout_destroy);
    #endif
    gtk_widget_queue_draw (GTK_WIDGET (throbber));

    g_object_notify (G_OBJECT (throbber), "animated");
}

/**
 * katze_throbber_set_static_icon_name:
 * @throbber: a #KatzeThrobber
 * @icon_name: an icon name or %NULL
 *
 * Sets the name of an icon that should provide the static image.
 *
 * The static pixbuf and stock ID are automatically invalidated.
 **/
void
katze_throbber_set_static_icon_name (KatzeThrobber*  throbber,
                                     const gchar*    icon_name)
{
    g_return_if_fail (KATZE_IS_THROBBER (throbber));

    katze_assign (throbber->static_icon_name, g_strdup (icon_name));

    g_object_freeze_notify (G_OBJECT (throbber));

    if (icon_name)
    {
        katze_assign (throbber->static_stock_id, NULL);

        icon_theme_changed (throbber);

        g_object_notify (G_OBJECT (throbber), "static-pixbuf");
        g_object_notify (G_OBJECT (throbber), "static-stock-id");
    }

    g_object_notify (G_OBJECT (throbber), "static-icon-name");
    g_object_thaw_notify (G_OBJECT (throbber));
}

/**
 * katze_throbber_set_static_pixbuf:
 * @throbber: a #KatzeThrobber
 * @pixbuf: a #GdkPixbuf or %NULL
 *
 * Sets the pixbuf that should provide the static image. The pixbuf is
 * assumed to have the icon size of the throbber, which can be specified
 * with katze_throbber_set_icon_size ().
 *
 * The static icon name and stock ID are automatically invalidated.
 **/
void
katze_throbber_set_static_pixbuf (KatzeThrobber* throbber,
                                  GdkPixbuf*     pixbuf)
{
    g_return_if_fail (KATZE_IS_THROBBER (throbber));
    g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

    katze_object_assign (throbber->static_pixbuf, pixbuf);

    g_object_freeze_notify (G_OBJECT (throbber));

    if (pixbuf)
    {
        g_object_ref (pixbuf);

        katze_assign (throbber->static_icon_name, NULL);
        katze_assign (throbber->static_stock_id, NULL);

        gtk_widget_queue_draw (GTK_WIDGET (throbber));

        g_object_notify (G_OBJECT (throbber), "static-icon-name");
        g_object_notify (G_OBJECT (throbber), "static-stock-id");
    }

    g_object_notify (G_OBJECT (throbber), "static-pixbuf");
    g_object_thaw_notify (G_OBJECT (throbber));
}

/**
 * katze_throbber_set_static_stock_id:
 * @throbber: a #KatzeThrobber
 * @stock_id: a stock ID or %NULL
 *
 * Sets the stock ID of an icon that should provide the static image.
 *
 * The static icon name and pixbuf are automatically invalidated.
 **/
void
katze_throbber_set_static_stock_id (KatzeThrobber* throbber,
                                    const gchar*   stock_id)
{
    g_return_if_fail (KATZE_IS_THROBBER (throbber));

    g_object_freeze_notify (G_OBJECT (throbber));

    if (stock_id)
    {
        GtkStockItem stock_item;
        g_return_if_fail (gtk_stock_lookup (stock_id, &stock_item));

        g_object_notify (G_OBJECT (throbber), "static-icon-name");
        g_object_notify (G_OBJECT (throbber), "static-pixbuf");
    }

    katze_assign (throbber->static_stock_id, g_strdup (stock_id));

    if (stock_id)
        icon_theme_changed (throbber);

    g_object_notify (G_OBJECT (throbber), "static-stock-id");
    g_object_thaw_notify (G_OBJECT (throbber));
}

/**
 * katze_throbber_get_icon_size:
 * @throbber: a #KatzeThrobber
 *
 * Retrieves the size of the throbber.
 *
 * Return value: the size of the throbber
 **/
GtkIconSize
katze_throbber_get_icon_size (KatzeThrobber* throbber)
{
    g_return_val_if_fail (KATZE_IS_THROBBER (throbber), GTK_ICON_SIZE_INVALID);

    return throbber->icon_size;
}

/**
 * katze_throbber_get_icon_name:
 * @throbber: a #KatzeThrobber
 *
 * Retrieves the name of the icon providing the animation frames.
 *
 * Return value: the name of the icon providing the animation frames, or %NULL
 **/
const gchar*
katze_throbber_get_icon_name (KatzeThrobber* throbber)
{
    g_return_val_if_fail (KATZE_IS_THROBBER (throbber), NULL);

    return throbber->icon_name;
}

/**
 * katze_throbber_get_pixbuf:
 * @throbber: a #KatzeThrobber
 *
 * Retrieves the #GdkPixbuf providing the animation frames if an icon name
 * or pixbuf is available. The caller of this function does not own a
 * reference to the returned pixbuf.
 *
 * Return value: the pixbuf providing the animation frames, or %NULL
 **/
GdkPixbuf*
katze_throbber_get_pixbuf (KatzeThrobber* throbber)
{
    g_return_val_if_fail (KATZE_IS_THROBBER (throbber), NULL);

    return throbber->pixbuf;
}

/**
 * katze_throbber_get_animated:
 * @throbber: a #KatzeThrobber
 *
 * Retrieves the status of the animation, whcih can be animated or static.
 *
 * Return value: %TRUE if the throbber is animated
 **/
gboolean
katze_throbber_get_animated (KatzeThrobber* throbber)
{
    g_return_val_if_fail (KATZE_IS_THROBBER (throbber), FALSE);

    return throbber->animated;
}

/**
 * katze_throbber_get_static_icon_name:
 * @throbber: a #KatzeThrobber
 *
 * Retrieves the name of the icon providing the static image, if an icon name
 * for the static image was specified.
 *
 * Return value: the name of the icon providing the static image, or %NULL
 **/
const gchar*
katze_throbber_get_static_icon_name (KatzeThrobber* throbber)
{
    g_return_val_if_fail (KATZE_IS_THROBBER (throbber), NULL);

    return throbber->static_icon_name;
}

/**
 * katze_throbber_get_static pixbuf:
 * @throbber: a #KatzeThrobber
 *
 * Retrieves the #GdkPixbuf providing the static image, if an icon name, a
 * pixbuf or a stock ID for the static image was specified. The caller of this
 * function does not own a reference to the returned pixbuf.
 *
 * Return value: the pixbuf providing the static image, or %NULL
 **/
GdkPixbuf*
katze_throbber_get_static_pixbuf (KatzeThrobber* throbber)
{
    g_return_val_if_fail (KATZE_IS_THROBBER (throbber), NULL);

    return throbber->static_pixbuf;
}

/**
 * katze_throbber_get_static_stock_id:
 * @throbber: a #KatzeThrobber
 *
 * Retrieves the stock ID of the icon providing the static image, if a
 * stock ID for the static image was specified.
 *
 * Return value: the stock ID of the icon providing the static image, or %NULL
 **/
const gchar*
katze_throbber_get_static_stock_id (KatzeThrobber* throbber)
{
    g_return_val_if_fail (KATZE_IS_THROBBER (throbber), NULL);

    return throbber->static_stock_id;
}

static void
katze_throbber_realize (GtkWidget* widget)
{
    (*GTK_WIDGET_CLASS (katze_throbber_parent_class)->realize) (widget);

    icon_theme_changed (KATZE_THROBBER (widget));
}

static void
katze_throbber_unrealize (GtkWidget* widget)
{
    if (GTK_WIDGET_CLASS (katze_throbber_parent_class)->unrealize)
        GTK_WIDGET_CLASS (katze_throbber_parent_class)->unrealize (widget);
}

static void
pixbuf_assign_icon (GdkPixbuf**    pixbuf,
                    const gchar*   icon_name,
                    KatzeThrobber* throbber)
{
    GdkScreen* screen;
    GtkIconTheme* icon_theme;

    if (*pixbuf)
        g_object_unref (*pixbuf);

    screen = gtk_widget_get_screen (GTK_WIDGET (throbber));
    icon_theme = gtk_icon_theme_get_for_screen (screen);
    *pixbuf = gtk_icon_theme_load_icon (icon_theme,
                                        icon_name,
                                        MAX (throbber->width, throbber->height),
                                        (GtkIconLookupFlags) 0,
                                        NULL);
}

static void
icon_theme_changed (KatzeThrobber* throbber)
{
    if (throbber->icon_name)
        pixbuf_assign_icon (&throbber->pixbuf,
                            throbber->icon_name, throbber);

    if (throbber->static_icon_name)
        pixbuf_assign_icon (&throbber->static_pixbuf,
                            throbber->static_icon_name, throbber);
    else if (throbber->static_stock_id)
        katze_object_assign (throbber->static_pixbuf,
            gtk_widget_render_icon (GTK_WIDGET (throbber),
                                    throbber->static_stock_id,
                                    throbber->icon_size,
                                    NULL));

    g_object_freeze_notify (G_OBJECT (throbber));
    g_object_notify (G_OBJECT (throbber), "pixbuf");
    g_object_notify (G_OBJECT (throbber), "static-pixbuf");
    g_object_thaw_notify (G_OBJECT (throbber));

    gtk_widget_queue_draw (GTK_WIDGET (throbber));
}

static void
katze_throbber_map (GtkWidget* widget)
{
    (*GTK_WIDGET_CLASS (katze_throbber_parent_class)->map) (widget);
}

static void
katze_throbber_unmap (GtkWidget* widget)
{
    if (GTK_WIDGET_CLASS (katze_throbber_parent_class)->unmap)
        GTK_WIDGET_CLASS (katze_throbber_parent_class)->unmap (widget);
}

#if !HAVE_SPINNER
static gboolean
katze_throbber_timeout (KatzeThrobber*  throbber)
{
    throbber->index++;
    gtk_widget_queue_draw (GTK_WIDGET (throbber));

    return throbber->animated;
}

static void
katze_throbber_timeout_destroy (KatzeThrobber*  throbber)
{
    throbber->index = 0;
    throbber->timer_id = -1;
}
#endif

static void
katze_throbber_style_set (GtkWidget* widget,
                          GtkStyle*  prev_style)
{
    if (GTK_WIDGET_CLASS (katze_throbber_parent_class)->style_set)
        GTK_WIDGET_CLASS (katze_throbber_parent_class)->style_set (widget,
                                                                   prev_style);

    icon_theme_changed (KATZE_THROBBER (widget));
}

static void
katze_throbber_screen_changed (GtkWidget* widget,
                               GdkScreen* prev_screen)
{
    if (GTK_WIDGET_CLASS (katze_throbber_parent_class)->screen_changed)
        GTK_WIDGET_CLASS (katze_throbber_parent_class)->screen_changed (
                                                        widget,
                                                        prev_screen);

    icon_theme_changed (KATZE_THROBBER (widget));
}

static void
katze_throbber_size_request (GtkWidget*      widget,
                             GtkRequisition* requisition)
{
    KatzeThrobber* throbber = KATZE_THROBBER (widget);

    requisition->width = throbber->width;
    requisition->height = throbber->height;
#if !GTK_CHECK_VERSION (3, 0, 0)
    GTK_WIDGET_CLASS (katze_throbber_parent_class)->size_request (widget,
                                                                  requisition);
#endif
}

#if GTK_CHECK_VERSION (3, 0, 0)
static void
katze_throbber_get_preferred_height (GtkWidget *widget,
                                     gint      *minimal_height,
                                     gint      *natural_height)
{
    GtkRequisition requisition;

    katze_throbber_size_request (widget, &requisition);

    *minimal_height = *natural_height = requisition.height;
}

static void
katze_throbber_get_preferred_width (GtkWidget *widget,
                                    gint      *minimal_width,
                                    gint      *natural_width)
{
    GtkRequisition requisition;

    katze_throbber_size_request (widget, &requisition);

    *minimal_width = *natural_width = requisition.width;
}
#endif

static void
katze_throbber_aligned_coords (GtkWidget* widget,
                               gint*      ax,
                               gint*      ay)
{
    gfloat xalign, yalign;
    gint xpad, ypad;
    GtkAllocation allocation;
    GtkRequisition requisition;

    #if HAVE_SPINNER
    xalign = 0.0;
    yalign = 0.5;
    xpad = ypad = 0.0;
    #else
    gtk_misc_get_alignment (GTK_MISC (widget), &xalign, &yalign);
    if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
        xalign = 1.0f - xalign;
    gtk_misc_get_padding (GTK_MISC (widget), &xpad, &ypad);
    #endif

    #if GTK_CHECK_VERSION (3, 0, 0)
    allocation.x = allocation.y = 0;
    allocation.width = gtk_widget_get_allocated_width (widget);
    allocation.height = gtk_widget_get_allocated_height (widget);
    gtk_widget_get_preferred_size (widget, &requisition, NULL);
    #else
    gtk_widget_get_allocation (widget, &allocation);
    gtk_widget_size_request (widget, &requisition);
    #endif
    *ax = floor (allocation.x + xpad
        + ((allocation.width - requisition.width) * xalign));
    *ay = floor (allocation.y + ypad
        + ((allocation.height - requisition.height) * yalign));
}

static gboolean
katze_throbber_expose_event (GtkWidget*      widget,
#if GTK_CHECK_VERSION (3, 0, 0)
                             cairo_t* cr)
#else
                             GdkEventExpose* event)
#endif
{
    KatzeThrobber* throbber = KATZE_THROBBER (widget);
    gint ax, ay;

    if (G_UNLIKELY (!throbber->width || !throbber->height))
        return TRUE;

    if (G_UNLIKELY (!throbber->pixbuf && !throbber->static_pixbuf))
        if (throbber->animated && !throbber->pixbuf && !throbber->icon_name)
            return TRUE;

    if (!throbber->animated && (throbber->static_pixbuf
        || throbber->static_icon_name || throbber->static_stock_id))
    {
#if !GTK_CHECK_VERSION (3, 0, 0)
        cairo_t* cr;
#endif
        if (G_UNLIKELY (!throbber->static_pixbuf && throbber->static_icon_name))
        {
            icon_theme_changed (KATZE_THROBBER (widget));

            if (!throbber->static_pixbuf)
            {
                g_warning (_("Named icon '%s' couldn't be loaded"),
                           throbber->static_icon_name);
                katze_assign (throbber->static_icon_name, NULL);
                g_object_notify (G_OBJECT (throbber), "static-icon-name");
                return TRUE;
            }
        }
        else if (G_UNLIKELY (!throbber->static_pixbuf && throbber->static_stock_id))
        {
            icon_theme_changed (KATZE_THROBBER (widget));

            if (!throbber->static_pixbuf)
            {
                g_warning (_("Stock icon '%s' couldn't be loaded"),
                           throbber->static_stock_id);
                katze_assign (throbber->static_stock_id, NULL);
                g_object_notify (G_OBJECT (throbber), "static-stock-id");
                return TRUE;
            }
        }

        katze_throbber_aligned_coords (widget, &ax, &ay);

#if !GTK_CHECK_VERSION (3, 0, 0)
        cr = gdk_cairo_create (gtk_widget_get_window (widget));
#endif
        gdk_cairo_set_source_pixbuf (cr, throbber->static_pixbuf, ax, ay);
        cairo_paint (cr);
#if !GTK_CHECK_VERSION (3, 0, 0)
        cairo_destroy (cr);
#endif
    }
    else
    {
        #if HAVE_SPINNER
        if (throbber->animated)
#if GTK_CHECK_VERSION (3, 0, 0)
            return GTK_WIDGET_CLASS (katze_throbber_parent_class)->draw (widget, cr);
#else
            return GTK_WIDGET_CLASS (katze_throbber_parent_class)->expose_event (widget, event);
#endif
        #else
        gint cols, rows;

        if (G_UNLIKELY (throbber->icon_name && !throbber->pixbuf))
        {
            icon_theme_changed (KATZE_THROBBER (widget));

            if (!throbber->pixbuf)
            {
                /* Fallback to a stock icon */
                katze_assign (throbber->icon_name, g_strdup (GTK_STOCK_EXECUTE));
                g_object_notify (G_OBJECT (throbber), "icon-name");
                return TRUE;
            }
        }

        cols = gdk_pixbuf_get_width (throbber->pixbuf) / throbber->width;
        rows = gdk_pixbuf_get_height (throbber->pixbuf) / throbber->height;

        if (G_UNLIKELY (cols == 1 && cols == rows))
        {
            katze_throbber_aligned_coords (widget, &ax, &ay);

            if (throbber->animated)
                gdk_draw_pixbuf (event->window, NULL, throbber->pixbuf,
                                 0, 0, ax, ay,
                                 throbber->width, throbber->height,
                                 GDK_RGB_DITHER_NONE, 0, 0);
            return TRUE;
        }

        if (G_LIKELY (cols > 0 && rows > 0))
        {
            gint idx;
            guint x, y;

            katze_throbber_aligned_coords (widget, &ax, &ay);

            idx = throbber->index % (cols * rows);
            if (G_LIKELY (throbber->timer_id >= 0))
                idx = MAX (idx, 1);

            x = (idx % cols) * throbber->width;
            y = (idx / cols) * throbber->height;

            gdk_draw_pixbuf (event->window, NULL, throbber->pixbuf,
                             x, y, ax, ay,
                             throbber->width, throbber->height,
                             GDK_RGB_DITHER_NONE, 0, 0);
        }
        else
        {
            g_warning (_("Animation frames are broken"));
            katze_assign (throbber->icon_name, NULL);
            katze_object_assign (throbber->pixbuf, NULL);

            g_object_freeze_notify (G_OBJECT (throbber));
            g_object_notify (G_OBJECT (throbber), "icon-name");
            g_object_notify (G_OBJECT (throbber), "pixbuf");
            g_object_thaw_notify (G_OBJECT (throbber));
        }
        #endif
    }

    return TRUE;
}
