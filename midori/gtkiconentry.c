#include "gtkiconentry.h"

void
gtk_icon_entry_set_icon_from_pixbuf (GtkEntry*            entry,
                                     GtkEntryIconPosition position,
                                     GdkPixbuf*           pixbuf)
{
    gboolean activatable;

    /* Without this ugly hack pixbuf icons don't work */
    activatable = gtk_entry_get_icon_activatable (entry, position);
    gtk_entry_set_icon_from_pixbuf (entry, position, pixbuf);
    gtk_entry_set_icon_activatable (entry, position, !activatable);
    gtk_entry_set_icon_activatable (entry, position, activatable);
}

