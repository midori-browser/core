/*
 Copyright (C) 2008-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

#include "katze-cellrenderer2texts.h"

#include "marshal.h"

#include <gdk/gdk.h>

#define P_(String) (String)
#define I_(String) (String)
#define GTK_PARAM_READABLE G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_WRITABLE G_PARAM_WRITABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_READWRITE G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB


static void
katze_cell_renderer_2texts_finalize (GObject* object);

static void
katze_cell_renderer_2texts_get_property (GObject*    object,
    guint       param_id,
    GValue*     value,
    GParamSpec* pspec);
static void
katze_cell_renderer_2texts_set_property (GObject*      object,
    guint         param_id,
    const GValue* value,
    GParamSpec*   pspec);
static void
katze_cell_renderer_2texts_get_size (GtkCellRenderer* cell,
    GtkWidget*       widget,
    GdkRectangle*    cell_area,
    gint*            x_offset,
    gint*            y_offset,
    gint*            width,
    gint*            height);
static void
#if GTK_CHECK_VERSION(3,0,0)
katze_cell_renderer_2texts_render (GtkCellRenderer      *cell,
    cairo_t*             cr,
    GtkWidget            *widget,
    GdkRectangle         *background_area,
    GdkRectangle         *cell_area,
    GtkCellRendererState  flags);
#else
katze_cell_renderer_2texts_render (GtkCellRenderer      *cell,
    GdkDrawable          *window,
    GtkWidget            *widget,
    GdkRectangle         *background_area,
    GdkRectangle         *cell_area,
    GdkRectangle         *expose_area,
    GtkCellRendererState  flags);
#endif

static GtkCellEditable *
katze_cell_renderer_2texts_start_editing (GtkCellRenderer      *cell,
    GdkEvent             *event,
    GtkWidget            *widget,
    const gchar          *path,
    GdkRectangle         *background_area,
    GdkRectangle         *cell_area,
    GtkCellRendererState  flags);

enum {
  EDITED,
  LAST_SIGNAL
};

enum {
  PROP_0,

  PROP_TEXT,
  PROP_MARKUP,
  PROP_ATTRIBUTES,
  PROP_ALTERNATE_TEXT,
  PROP_ALTERNATE_MARKUP,
  PROP_ALTERNATE_ATTRIBUTES,

  /* GtkCellRendererText args */
  PROP_SINGLE_PARAGRAPH_MODE,
  PROP_WIDTH_CHARS,
  PROP_WRAP_WIDTH,
  PROP_ALIGN,

  /* Style args */
  PROP_BACKGROUND,
  PROP_FOREGROUND,
  PROP_BACKGROUND_GDK,
  PROP_FOREGROUND_GDK,
  PROP_FONT,
  PROP_FONT_DESC,
  PROP_FAMILY,
  PROP_STYLE,
  PROP_VARIANT,
  PROP_WEIGHT,
  PROP_STRETCH,
  PROP_SIZE,
  PROP_SIZE_POINTS,
  PROP_SCALE,
  PROP_EDITABLE,
  PROP_STRIKETHROUGH,
  PROP_UNDERLINE,
  PROP_RISE,
  PROP_LANGUAGE,
  PROP_ELLIPSIZE,
  PROP_WRAP_MODE,

  /* Whether-a-style-arg-is-set args */
  PROP_BACKGROUND_SET,
  PROP_FOREGROUND_SET,
  PROP_FAMILY_SET,
  PROP_STYLE_SET,
  PROP_VARIANT_SET,
  PROP_WEIGHT_SET,
  PROP_STRETCH_SET,
  PROP_SIZE_SET,
  PROP_SCALE_SET,
  PROP_EDITABLE_SET,
  PROP_STRIKETHROUGH_SET,
  PROP_UNDERLINE_SET,
  PROP_RISE_SET,
  PROP_LANGUAGE_SET,
  PROP_ELLIPSIZE_SET,
  PROP_ALIGN_SET
};

static guint _2texts_cell_renderer_signals [LAST_SIGNAL];

#define KATZE_CELL_RENDERER_2TEXTS_PATH "gtk-cell-renderer-text-path"

#define KATZE_CELL_RENDERER_2TEXTS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), KATZE_TYPE_CELL_RENDERER_2TEXTS, KatzeCellRenderer2textsPrivate))

typedef struct _KatzeCellRenderer2textsPrivate KatzeCellRenderer2textsPrivate;
struct _KatzeCellRenderer2textsPrivate
{
  GtkCellRendererText* celltext;

  guint markup_set : 1;
  guint alternate_markup_set : 1;
};

G_DEFINE_TYPE (KatzeCellRenderer2texts, katze_cell_renderer_2texts, GTK_TYPE_CELL_RENDERER)

static void
katze_cell_renderer_2texts_notify (GObject    *gobject,
                                   GParamSpec *pspec,
                                   KatzeCellRenderer2texts *celltext)
{
    if (!g_strcmp0(P_("text"), pspec->name)
        || !g_strcmp0(P_("attributes"), pspec->name))
        return;

    g_object_notify (G_OBJECT (celltext), pspec->name);
}

static void
katze_cell_renderer_2texts_init (KatzeCellRenderer2texts *celltext)
{
  KatzeCellRenderer2textsPrivate *priv;

  priv = KATZE_CELL_RENDERER_2TEXTS_GET_PRIVATE (celltext);

  priv->celltext = GTK_CELL_RENDERER_TEXT (gtk_cell_renderer_text_new());
  g_object_ref (priv->celltext);

  priv->markup_set = FALSE;
  priv->alternate_markup_set = FALSE;

  g_signal_connect (priv->celltext, "notify",
      G_CALLBACK (katze_cell_renderer_2texts_notify),
      celltext);
}

static void
katze_cell_renderer_2texts_class_init (KatzeCellRenderer2textsClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

  object_class->finalize = katze_cell_renderer_2texts_finalize;

  object_class->get_property = katze_cell_renderer_2texts_get_property;
  object_class->set_property = katze_cell_renderer_2texts_set_property;

  cell_class->get_size = katze_cell_renderer_2texts_get_size;
  cell_class->render = katze_cell_renderer_2texts_render;
  cell_class->start_editing = katze_cell_renderer_2texts_start_editing;

  g_object_class_install_property (object_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text",
                                                        P_("Text"),
                                                        P_("Text to render"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ALTERNATE_TEXT,
                                   g_param_spec_string ("alternate-text",
                                                        P_("Alternate text"),
                                                        P_("Text to render if 2texts is opened"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_MARKUP,
                                   g_param_spec_string ("markup",
                                                        P_("Markup"),
                                                        P_("Marked up text to render"),
                                                        NULL,
                                                        GTK_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_ALTERNATE_MARKUP,
                                   g_param_spec_string ("alternate-markup",
                                                        P_("Markup"),
                                                        P_("Marked up text to render if 2texts is opened"),
                                                        NULL,
                                                        GTK_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_ATTRIBUTES,
				   g_param_spec_boxed ("attributes",
						       P_("Attributes"),
						       P_("A list of style attributes to apply to the text of the renderer"),
						       PANGO_TYPE_ATTR_LIST,
						       GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
				   PROP_ALTERNATE_ATTRIBUTES,
				   g_param_spec_boxed ("alternate-attributes",
						       P_("Attributes"),
						       P_("A list of style attributes to apply to the text of the renderer"),
						       PANGO_TYPE_ATTR_LIST,
						       GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SINGLE_PARAGRAPH_MODE,
                                   g_param_spec_boolean ("single-paragraph-mode",
                                                         P_("Single Paragraph Mode"),
                                                         P_("Whether or not to keep all text in a single paragraph"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));


  g_object_class_install_property (object_class,
                                   PROP_BACKGROUND,
                                   g_param_spec_string ("background",
                                                        P_("Background color name"),
                                                        P_("Background color as a string"),
                                                        NULL,
                                                        GTK_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_BACKGROUND_GDK,
                                   g_param_spec_boxed ("background-gdk",
                                                       P_("Background color"),
                                                       P_("Background color as a GdkColor"),
                                                       GDK_TYPE_COLOR,
                                                       GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_FOREGROUND,
                                   g_param_spec_string ("foreground",
                                                        P_("Foreground color name"),
                                                        P_("Foreground color as a string"),
                                                        NULL,
                                                        GTK_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_FOREGROUND_GDK,
                                   g_param_spec_boxed ("foreground-gdk",
                                                       P_("Foreground color"),
                                                       P_("Foreground color as a GdkColor"),
                                                       GDK_TYPE_COLOR,
                                                       GTK_PARAM_READWRITE));


  g_object_class_install_property (object_class,
                                   PROP_EDITABLE,
                                   g_param_spec_boolean ("editable",
                                                         P_("Editable"),
                                                         P_("Whether the text can be modified by the user"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_FONT,
                                   g_param_spec_string ("font",
                                                        P_("Font"),
                                                        P_("Font description as a string, e.g. \"Sans Italic 12\""),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_FONT_DESC,
                                   g_param_spec_boxed ("font-desc",
                                                       P_("Font"),
                                                       P_("Font description as a PangoFontDescription struct"),
                                                       PANGO_TYPE_FONT_DESCRIPTION,
                                                       GTK_PARAM_READWRITE));


  g_object_class_install_property (object_class,
                                   PROP_FAMILY,
                                   g_param_spec_string ("family",
                                                        P_("Font family"),
                                                        P_("Name of the font family, e.g. Sans, Helvetica, Times, Monospace"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_STYLE,
                                   g_param_spec_enum ("style",
                                                      P_("Font style"),
                                                      P_("Font style"),
                                                      PANGO_TYPE_STYLE,
                                                      PANGO_STYLE_NORMAL,
                                                      GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_VARIANT,
                                   g_param_spec_enum ("variant",
                                                     P_("Font variant"),
                                                     P_("Font variant"),
                                                      PANGO_TYPE_VARIANT,
                                                      PANGO_VARIANT_NORMAL,
                                                      GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_WEIGHT,
                                   g_param_spec_int ("weight",
                                                     P_("Font weight"),
                                                     P_("Font weight"),
                                                     0,
                                                     G_MAXINT,
                                                     PANGO_WEIGHT_NORMAL,
                                                     GTK_PARAM_READWRITE));

   g_object_class_install_property (object_class,
                                   PROP_STRETCH,
                                   g_param_spec_enum ("stretch",
                                                      P_("Font stretch"),
                                                      P_("Font stretch"),
                                                      PANGO_TYPE_STRETCH,
                                                      PANGO_STRETCH_NORMAL,
                                                      GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SIZE,
                                   g_param_spec_int ("size",
                                                     P_("Font size"),
                                                     P_("Font size"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SIZE_POINTS,
                                   g_param_spec_double ("size-points",
                                                        P_("Font points"),
                                                        P_("Font size in points"),
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SCALE,
                                   g_param_spec_double ("scale",
                                                        P_("Font scale"),
                                                        P_("Font scaling factor"),
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        1.0,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_RISE,
                                   g_param_spec_int ("rise",
                                                     P_("Rise"),
                                                     P_("Offset of text above the baseline "
							"(below the baseline if rise is negative)"),
                                                     -G_MAXINT,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READWRITE));


  g_object_class_install_property (object_class,
                                   PROP_STRIKETHROUGH,
                                   g_param_spec_boolean ("strikethrough",
                                                         P_("Strikethrough"),
                                                         P_("Whether to strike through the text"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_UNDERLINE,
                                   g_param_spec_enum ("underline",
                                                      P_("Underline"),
                                                      P_("Style of underline for this text"),
                                                      PANGO_TYPE_UNDERLINE,
                                                      PANGO_UNDERLINE_NONE,
                                                      GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_LANGUAGE,
                                   g_param_spec_string ("language",
                                                        P_("Language"),
                                                        P_("The language this text is in, as an ISO code. "
							   "Pango can use this as a hint when rendering the text. "
							   "If you don't understand this parameter, you probably don't need it"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));


  /**
   * KatzeCellRenderer2texts:ellipsize:
   *
   * Specifies the preferred place to ellipsize the string, if the cell renderer
   * does not have enough room to display the entire string. Setting it to
   * %PANGO_ELLIPSIZE_NONE turns off ellipsizing. See the wrap-width property
   * for another way of making the text fit into a given width.
   *
   * Since: 2.6
   */
  g_object_class_install_property (object_class,
                                   PROP_ELLIPSIZE,
                                   g_param_spec_enum ("ellipsize",
						      P_("Ellipsize"),
						      P_("The preferred place to ellipsize the string, "
							 "if the cell renderer does not have enough room "
							 "to display the entire string"),
						      PANGO_TYPE_ELLIPSIZE_MODE,
						      PANGO_ELLIPSIZE_NONE,
						      GTK_PARAM_READWRITE));

  /**
   * KatzeCellRenderer2texts:width-chars:
   *
   * The desired width of the cell, in characters. If this property is set to
   * -1, the width will be calculated automatically, otherwise the cell will
   * request either 3 characters or the property value, whichever is greater.
   *
   * Since: 2.6
   **/
  g_object_class_install_property (object_class,
                                   PROP_WIDTH_CHARS,
                                   g_param_spec_int ("width-chars",
                                                     P_("Width In Characters"),
                                                     P_("The desired width of the label, in characters"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE));

  /**
   * KatzeCellRenderer2texts:wrap-mode:
   *
   * Specifies how to break the string into multiple lines, if the cell
   * renderer does not have enough room to display the entire string.
   * This property has no effect unless the wrap-width property is set.
   *
   * Since: 2.8
   */
  g_object_class_install_property (object_class,
                                   PROP_WRAP_MODE,
                                   g_param_spec_enum ("wrap-mode",
						      P_("Wrap mode"),
						      P_("How to break the string into multiple lines, "
							 "if the cell renderer does not have enough room "
							 "to display the entire string"),
						      PANGO_TYPE_WRAP_MODE,
						      PANGO_WRAP_CHAR,
						      GTK_PARAM_READWRITE));

  /**
   * KatzeCellRenderer2texts:wrap-width:
   *
   * Specifies the width at which the text is wrapped. The wrap-mode property can
   * be used to influence at what character positions the line breaks can be placed.
   * Setting wrap-width to -1 turns wrapping off.
   *
   * Since: 2.8
   */
  g_object_class_install_property (object_class,
				   PROP_WRAP_WIDTH,
				   g_param_spec_int ("wrap-width",
						     P_("Wrap width"),
						     P_("The width at which the text is wrapped"),
						     -1,
						     G_MAXINT,
						     -1,
						     GTK_PARAM_READWRITE));

  /**
   * KatzeCellRenderer2texts:alignment:
   *
   * Specifies how to align the lines of text with respect to each other.
   *
   * Note that this property describes how to align the lines of text in
   * case there are several of them. The "xalign" property of #GtkCellRenderer,
   * on the other hand, sets the horizontal alignment of the whole text.
   *
   * Since: 2.10
   */
  g_object_class_install_property (object_class,
                                   PROP_ALIGN,
                                   g_param_spec_enum ("alignment",
						      P_("Alignment"),
						      P_("How to align the lines"),
						      PANGO_TYPE_ALIGNMENT,
						      PANGO_ALIGN_LEFT,
						      GTK_PARAM_READWRITE));

  /* Style props are set or not */

#define ADD_SET_PROP(propname, propval, nick, blurb) g_object_class_install_property (object_class, propval, g_param_spec_boolean (propname, nick, blurb, FALSE, GTK_PARAM_READWRITE))

  ADD_SET_PROP ("background-set", PROP_BACKGROUND_SET,
                P_("Background set"),
                P_("Whether this tag affects the background color"));

  ADD_SET_PROP ("foreground-set", PROP_FOREGROUND_SET,
                P_("Foreground set"),
                P_("Whether this tag affects the foreground color"));

  ADD_SET_PROP ("editable-set", PROP_EDITABLE_SET,
                P_("Editability set"),
                P_("Whether this tag affects text editability"));

  ADD_SET_PROP ("family-set", PROP_FAMILY_SET,
                P_("Font family set"),
                P_("Whether this tag affects the font family"));

  ADD_SET_PROP ("style-set", PROP_STYLE_SET,
                P_("Font style set"),
                P_("Whether this tag affects the font style"));

  ADD_SET_PROP ("variant-set", PROP_VARIANT_SET,
                P_("Font variant set"),
                P_("Whether this tag affects the font variant"));

  ADD_SET_PROP ("weight-set", PROP_WEIGHT_SET,
                P_("Font weight set"),
                P_("Whether this tag affects the font weight"));

  ADD_SET_PROP ("stretch-set", PROP_STRETCH_SET,
                P_("Font stretch set"),
                P_("Whether this tag affects the font stretch"));

  ADD_SET_PROP ("size-set", PROP_SIZE_SET,
                P_("Font size set"),
                P_("Whether this tag affects the font size"));

  ADD_SET_PROP ("scale-set", PROP_SCALE_SET,
                P_("Font scale set"),
                P_("Whether this tag scales the font size by a factor"));

  ADD_SET_PROP ("rise-set", PROP_RISE_SET,
                P_("Rise set"),
                P_("Whether this tag affects the rise"));

  ADD_SET_PROP ("strikethrough-set", PROP_STRIKETHROUGH_SET,
                P_("Strikethrough set"),
                P_("Whether this tag affects strikethrough"));

  ADD_SET_PROP ("underline-set", PROP_UNDERLINE_SET,
                P_("Underline set"),
                P_("Whether this tag affects underlining"));

  ADD_SET_PROP ("language-set", PROP_LANGUAGE_SET,
                P_("Language set"),
                P_("Whether this tag affects the language the text is rendered as"));

  ADD_SET_PROP ("ellipsize-set", PROP_ELLIPSIZE_SET,
                P_("Ellipsize set"),
                P_("Whether this tag affects the ellipsize mode"));

  ADD_SET_PROP ("align-set", PROP_ALIGN_SET,
                P_("Align set"),
                P_("Whether this tag affects the alignment mode"));

  /**
   * KatzeCellRenderer2texts::edited
   * @renderer: the object which received the signal
   * @path: the path identifying the edited cell
   * @new_text: the new text
   *
   * This signal is emitted after @renderer has been edited.
   *
   * It is the responsibility of the application to update the model
   * and store @new_text at the position indicated by @path.
   */
  _2texts_cell_renderer_signals [EDITED] =
    g_signal_new (I_("edited"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (KatzeCellRenderer2textsClass, edited),
		  NULL, NULL,
		  midori_cclosure_marshal_VOID__STRING_STRING,
		  G_TYPE_NONE, 2,
		  G_TYPE_STRING,
		  G_TYPE_STRING);

  g_type_class_add_private (object_class, sizeof (KatzeCellRenderer2textsPrivate));
}

static void
katze_cell_renderer_2texts_finalize (GObject *object)
{
  KatzeCellRenderer2texts *celltext = KATZE_CELL_RENDERER_2TEXTS (object);
  KatzeCellRenderer2textsPrivate *priv;

  priv = KATZE_CELL_RENDERER_2TEXTS_GET_PRIVATE (object);

  g_free (celltext->text);
  if (celltext->extra_attrs)
    pango_attr_list_unref (celltext->extra_attrs);

  g_free (celltext->alternate_text);
  if (celltext->alternate_extra_attrs)
    pango_attr_list_unref (celltext->alternate_extra_attrs);

  g_object_unref (priv->celltext);

  G_OBJECT_CLASS (katze_cell_renderer_2texts_parent_class)->finalize (object);
}

static const gchar* const cell_text_renderer_property_names[] =
{
  /* GtkCellRendererText args */
  "single-paragraph-mode",
  "width-chars",
  "wrap-width",
  "align",

  /* Style args */
  "background",
  "foreground",
  "background-gdk",
  "foreground-gdk",
  "font",
  "font-desc",
  "family",
  "style",
  "variant",
  "weight",
  "stretch",
  "size",
  "size-points",
  "scale",
  "editable",
  "strikethrough",
  "underline",
  "rise",
  "language",
  "ellipsize",
  "wrap-mode",

  /* Whether-a-style-arg-is-set args */
  "background-set",
  "foreground-set",
  "family-set",
  "style-set",
  "variant-set",
  "weight-set",
  "stretch-set",
  "size-set",
  "scale-set",
  "editable-set",
  "strikethrough-set",
  "underline-set",
  "rise-set",
  "language-set",
  "ellipsize-set",
  "align-set"
};

static void
katze_cell_renderer_2texts_get_property (GObject*    object,
					 guint       param_id,
					 GValue*     value,
					 GParamSpec* pspec)
{
  KatzeCellRenderer2texts *celltext = KATZE_CELL_RENDERER_2TEXTS (object);
  KatzeCellRenderer2textsPrivate *priv;

  priv = KATZE_CELL_RENDERER_2TEXTS_GET_PRIVATE (object);

  switch (param_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, celltext->text);
      break;

    case PROP_ATTRIBUTES:
      g_value_set_boxed (value, celltext->extra_attrs);
      break;

    case PROP_ALTERNATE_TEXT:
      g_value_set_string (value, celltext->alternate_text);
      break;

    case PROP_ALTERNATE_ATTRIBUTES:
      g_value_set_boxed (value, celltext->alternate_extra_attrs);
      break;

    case PROP_SINGLE_PARAGRAPH_MODE:
    case PROP_BACKGROUND_GDK:
    case PROP_FOREGROUND_GDK:
    case PROP_FONT:
    case PROP_FONT_DESC:
    case PROP_FAMILY:
    case PROP_STYLE:
    case PROP_VARIANT:
    case PROP_WEIGHT:
    case PROP_STRETCH:
    case PROP_SIZE:
    case PROP_SIZE_POINTS:
    case PROP_SCALE:
    case PROP_EDITABLE:
    case PROP_STRIKETHROUGH:
    case PROP_UNDERLINE:
    case PROP_RISE:
    case PROP_LANGUAGE:
    case PROP_ELLIPSIZE:
    case PROP_WRAP_MODE:
    case PROP_WRAP_WIDTH:
    case PROP_ALIGN:
    case PROP_BACKGROUND_SET:
    case PROP_FOREGROUND_SET:
    case PROP_FAMILY_SET:
    case PROP_STYLE_SET:
    case PROP_VARIANT_SET:
    case PROP_WEIGHT_SET:
    case PROP_STRETCH_SET:
    case PROP_SIZE_SET:
    case PROP_SCALE_SET:
    case PROP_EDITABLE_SET:
    case PROP_STRIKETHROUGH_SET:
    case PROP_UNDERLINE_SET:
    case PROP_RISE_SET:
    case PROP_LANGUAGE_SET:
    case PROP_ELLIPSIZE_SET:
    case PROP_ALIGN_SET:
    case PROP_WIDTH_CHARS:
    case PROP_BACKGROUND:
    case PROP_FOREGROUND:
      g_object_get_property (G_OBJECT (priv->celltext), cell_text_renderer_property_names[param_id-PROP_SINGLE_PARAGRAPH_MODE], value);
      break;
    case PROP_MARKUP:
    case PROP_ALTERNATE_MARKUP:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}


static void
katze_cell_renderer_2texts_set_property (GObject*      object,
					 guint         param_id,
					 const GValue* value,
					 GParamSpec*   pspec)
{
  KatzeCellRenderer2texts *celltext = KATZE_CELL_RENDERER_2TEXTS (object);
  KatzeCellRenderer2textsPrivate *priv;

  priv = KATZE_CELL_RENDERER_2TEXTS_GET_PRIVATE (object);

  switch (param_id)
    {
    case PROP_TEXT:
      g_free (celltext->text);

      if (priv->markup_set)
        {
          if (celltext->extra_attrs)
            pango_attr_list_unref (celltext->extra_attrs);
          celltext->extra_attrs = NULL;
          priv->markup_set = FALSE;
        }

      celltext->text = g_value_dup_string (value);
      g_object_notify (object, "text");
      break;
   case PROP_ATTRIBUTES:
      if (celltext->extra_attrs)
	pango_attr_list_unref (celltext->extra_attrs);

      celltext->extra_attrs = g_value_get_boxed (value);
      if (celltext->extra_attrs)
        pango_attr_list_ref (celltext->extra_attrs);
      break;
    case PROP_MARKUP:
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

	g_free (celltext->text);

	if (celltext->extra_attrs)
	  pango_attr_list_unref (celltext->extra_attrs);

	celltext->text = text;
	celltext->extra_attrs = attrs;
        priv->markup_set = TRUE;
      }
      break;

    case PROP_ALTERNATE_TEXT:
      g_free (celltext->alternate_text);

      if (priv->alternate_markup_set)
        {
          if (celltext->alternate_extra_attrs)
            pango_attr_list_unref (celltext->alternate_extra_attrs);
          celltext->alternate_extra_attrs = NULL;
          priv->alternate_markup_set = FALSE;
        }

      celltext->alternate_text = g_value_dup_string (value);
      g_object_notify (object, "alternate-text");
      break;
   case PROP_ALTERNATE_ATTRIBUTES:
      if (celltext->alternate_extra_attrs)
	pango_attr_list_unref (celltext->alternate_extra_attrs);

      celltext->alternate_extra_attrs = g_value_get_boxed (value);
      if (celltext->alternate_extra_attrs)
        pango_attr_list_ref (celltext->alternate_extra_attrs);
      break;
    case PROP_ALTERNATE_MARKUP:
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

	g_free (celltext->alternate_text);

	if (celltext->alternate_extra_attrs)
	  pango_attr_list_unref (celltext->alternate_extra_attrs);

	celltext->alternate_text = text;
	celltext->alternate_extra_attrs = attrs;
        priv->alternate_markup_set = TRUE;
      }
      break;

    case PROP_SINGLE_PARAGRAPH_MODE:
    case PROP_BACKGROUND:
    case PROP_FOREGROUND:
    case PROP_BACKGROUND_GDK:
    case PROP_FOREGROUND_GDK:
    case PROP_FONT:
    case PROP_FONT_DESC:
    case PROP_FAMILY:
    case PROP_STYLE:
    case PROP_VARIANT:
    case PROP_WEIGHT:
    case PROP_STRETCH:
    case PROP_SIZE:
    case PROP_SIZE_POINTS:
    case PROP_SCALE:
    case PROP_EDITABLE:
    case PROP_STRIKETHROUGH:
    case PROP_UNDERLINE:
    case PROP_RISE:
    case PROP_LANGUAGE:
    case PROP_ELLIPSIZE:
    case PROP_WRAP_MODE:
    case PROP_WRAP_WIDTH:
    case PROP_WIDTH_CHARS:
    case PROP_ALIGN:
    case PROP_BACKGROUND_SET:
    case PROP_FOREGROUND_SET:
    case PROP_FAMILY_SET:
    case PROP_STYLE_SET:
    case PROP_VARIANT_SET:
    case PROP_WEIGHT_SET:
    case PROP_STRETCH_SET:
    case PROP_SIZE_SET:
    case PROP_SCALE_SET:
    case PROP_EDITABLE_SET:
    case PROP_STRIKETHROUGH_SET:
    case PROP_UNDERLINE_SET:
    case PROP_RISE_SET:
    case PROP_LANGUAGE_SET:
    case PROP_ELLIPSIZE_SET:
    case PROP_ALIGN_SET:
      g_object_set_property (G_OBJECT (priv->celltext), cell_text_renderer_property_names[param_id-PROP_SINGLE_PARAGRAPH_MODE], value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

/**
 * katze_cell_renderer_2texts_new:
 *
 * Creates a new #KatzeCellRenderer2texts. Adjust how text is drawn using
 * object properties. Object properties can be
 * set globally (with g_object_set()). Also, with #GtkTreeViewColumn,
 * you can bind a property to a value in a #GtkTreeModel. For example,
 * you can bind the "text" property on the cell renderer to a string
 * value in the model, thus rendering a different string in each row
 * of the #GtkTreeView
 *
 * Return value: the new cell renderer
 **/
GtkCellRenderer *
katze_cell_renderer_2texts_new (void)
{
  return g_object_new (KATZE_TYPE_CELL_RENDERER_2TEXTS, NULL);
}

static void
set_text(KatzeCellRenderer2texts*        celltext,
	 GtkWidget*                      widget,
	 KatzeCellRenderer2textsPrivate* priv)
{
  GValue text_value = {0};
  GValue attrs_value = {0};
  GtkWidget* pwidget = gtk_widget_get_parent (widget);
  gboolean alternate = FALSE;

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
	  alternate = TRUE;
	}
    }

  g_value_init (&text_value, G_TYPE_STRING);
  g_value_init (&attrs_value, PANGO_TYPE_ATTR_LIST);

  if (alternate)
    {
      g_value_set_static_string (&text_value, celltext->alternate_text);
      g_value_set_boxed (&attrs_value, celltext->alternate_extra_attrs);
    }
  else
    {
      g_value_set_static_string (&text_value, celltext->text);
      g_value_set_boxed (&attrs_value, celltext->extra_attrs);
    }

  g_object_set_property (G_OBJECT (priv->celltext),
			 "text", &text_value);
  g_object_set_property (G_OBJECT (priv->celltext),
			 "attributes", &attrs_value);

}

static void
katze_cell_renderer_2texts_get_size (GtkCellRenderer *cell,
				 GtkWidget       *widget,
				 GdkRectangle    *cell_area,
				 gint            *x_offset,
				 gint            *y_offset,
				 gint            *width,
				 gint            *height)
{
  KatzeCellRenderer2texts *celltext = (KatzeCellRenderer2texts *) cell;
  KatzeCellRenderer2textsPrivate *priv;
  priv = KATZE_CELL_RENDERER_2TEXTS_GET_PRIVATE (cell);

  set_text (celltext, widget, priv);

  gtk_cell_renderer_get_size (GTK_CELL_RENDERER (priv->celltext),
			      widget, cell_area,
			      x_offset, y_offset, width, height);
}

static void
#if GTK_CHECK_VERSION(3,0,0)
katze_cell_renderer_2texts_render (GtkCellRenderer      *cell,
			       cairo_t*             cr,
			       GtkWidget            *widget,
			       GdkRectangle         *background_area,
			       GdkRectangle         *cell_area,
			       GtkCellRendererState  flags)
#else
katze_cell_renderer_2texts_render (GtkCellRenderer      *cell,
			       GdkDrawable          *window,
			       GtkWidget            *widget,
			       GdkRectangle         *background_area,
			       GdkRectangle         *cell_area,
			       GdkRectangle         *expose_area,
			       GtkCellRendererState  flags)
#endif
{
  KatzeCellRenderer2texts *celltext = (KatzeCellRenderer2texts *) cell;
  KatzeCellRenderer2textsPrivate *priv;

  priv = KATZE_CELL_RENDERER_2TEXTS_GET_PRIVATE (cell);

  set_text (celltext, widget, priv);

#if GTK_CHECK_VERSION(3,0,0)
  gtk_cell_renderer_render (GTK_CELL_RENDERER (priv->celltext),
			    cr,
			    widget,
			    background_area,
			    cell_area,
			    flags);
#else
  gtk_cell_renderer_render (GTK_CELL_RENDERER (priv->celltext),
			    window,
			    widget,
			    background_area,
			    cell_area,
			    expose_area,
			    flags);
#endif
}

static void
katze_cell_renderer_2texts_edited (GtkCellRendererText *celltext,
				   const gchar* path,
				   const gchar* new_text,
				   gpointer         data)
{
  g_signal_emit (data, _2texts_cell_renderer_signals[EDITED], 0, path, new_text);
}

static GtkCellEditable *
katze_cell_renderer_2texts_start_editing (GtkCellRenderer      *cell,
				      GdkEvent             *event,
				      GtkWidget            *widget,
				      const gchar          *path,
				      GdkRectangle         *background_area,
				      GdkRectangle         *cell_area,
				      GtkCellRendererState  flags)
{
  GtkCellEditable *celledit;
  GtkRequisition requisition;
  KatzeCellRenderer2texts *celltext;
  KatzeCellRenderer2textsPrivate *priv;

  celltext = KATZE_CELL_RENDERER_2TEXTS (cell);
  priv = KATZE_CELL_RENDERER_2TEXTS_GET_PRIVATE (cell);

  celledit = gtk_cell_renderer_start_editing (GTK_CELL_RENDERER (priv->celltext), event, widget, path, background_area, cell_area, flags);

  if (celledit)
    g_signal_connect (priv->celltext,
		      "edited",
		      G_CALLBACK (katze_cell_renderer_2texts_edited),
		      celltext);

  return celledit;
}

/**
 * katze_cell_renderer_2texts_set_fixed_height_from_font:
 * @renderer: A #KatzeCellRenderer2texts
 * @number_of_rows: Number of rows of text each cell renderer is allocated, or -1
 *
 * Sets the height of a renderer to explicitly be determined by the "font" and
 * "y_pad" property set on it.  Further changes in these properties do not
 * affect the height, so they must be accompanied by a subsequent call to this
 * function.  Using this function is unflexible, and should really only be used
 * if calculating the size of a cell is too slow (ie, a massive number of cells
 * displayed).  If @number_of_rows is -1, then the fixed height is unset, and
 * the height is determined by the properties again.
 **/
void
katze_cell_renderer_2texts_set_fixed_height_from_font (KatzeCellRenderer2texts *renderer,
						                               gint number_of_rows)
{
  g_return_if_fail (KATZE_IS_CELL_RENDERER_2TEXTS (renderer));
  g_return_if_fail (number_of_rows == -1 || number_of_rows > 0);

  KatzeCellRenderer2textsPrivate *priv;

  priv = KATZE_CELL_RENDERER_2TEXTS_GET_PRIVATE (renderer);

  gtk_cell_renderer_text_set_fixed_height_from_font (priv->celltext, number_of_rows);
}
