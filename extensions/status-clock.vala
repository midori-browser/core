/*
 Copyright (C) 2010 Arno Renevier <arno@renevier.net>
 Copyright (C) 2018 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace StatusClock {
    public class Frontend : Object, Midori.BrowserActivatable {
        public Midori.Browser browser { owned get; set; }

        bool set_current_time (Gtk.Label clock) {
            var date = new DateTime.now_local ();
            string format = "%X";
            clock.label = date.format (format);
            int interval = format in "%s%N%s%S%T%X" ? 1 : int.max (60 - date.get_second (), 1);
            Timeout.add_seconds (interval, () => {
                set_current_time (clock);
                return Source.REMOVE;
            }, Priority.LOW);
            return false;
        }

        public void activate () {
            var clock = new Gtk.Label ("");
            set_current_time (clock);
            clock.halign = Gtk.Align.START;
            clock.valign = Gtk.Align.START;
            clock.set_padding (4, 4);
            clock.margin = 4;
            clock.get_style_context ().add_class ("background");
            clock.show ();
            browser.overlay.add_overlay (clock);
            browser.overlay.enter_notify_event.connect ((event) => {
                // Flip overlay to evade the mouse pointer
                clock.hide ();
                clock.halign = clock.halign == Gtk.Align.START ? Gtk.Align.END : Gtk.Align.START;
                clock.show ();
                return false;
            });
            deactivate.connect (() => {
                clock.destroy ();
            });
        }
    }
}

[ModuleInit]
public void peas_register_types(TypeModule module) {
    ((Peas.ObjectModule)module).register_extension_type (
        typeof (Midori.BrowserActivatable), typeof (StatusClock.Frontend));
}
