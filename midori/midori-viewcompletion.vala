/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public class ViewCompletion : Completion {
        GLib.Object app;
        GLib.Object browsers;

        public ViewCompletion () {
            GLib.Object (description: _("Open tabs"));
        }

        public override void prepare (GLib.Object app) {
            this.app = app;
            app.get ("browsers", out browsers);
        }

        public override bool can_complete (string text) {
            return browsers != null;
        }

        public override bool can_action (string action) {
            return action == "about:views";
        }

        public override async List<Suggestion>? complete (string text, string? action, Cancellable cancellable) {
            return_val_if_fail (browsers != null, null);

            unowned List<GLib.Object> browsers_list = Katze.array_peek_items (browsers);
            var suggestions = new List<Suggestion> ();
            uint n = 0;
            string key = text.casefold ();
            foreach (var browser in browsers_list) {
                /* FIXME multiple windows */
                GLib.Object current_browser;
                app.get ("browser", out current_browser);
                if (browser != current_browser)
                    continue;

                GLib.Object items;
                browser.get ("proxy-items", out items);
                unowned List<GLib.Object> items_list = Katze.array_peek_items (items);

                foreach (var item in items_list) {
                    string? uri, title;
                    item.get ("uri", out uri);
                    item.get ("name", out title);
                    if (uri == null) {
                        warning ("item.uri != null");
                        continue;
                    }
                    if (title == null) {
                        warning ("item.name != null");
                        continue;
                    }
                    if (!(key in uri.casefold () || key in title.casefold ()))
                        continue;

                    Gdk.Pixbuf? icon = Midori.Paths.get_icon (uri, null);
                    /* FIXME: Theming? Win32? */
                    string background = "gray";
                    var suggestion = new Suggestion (uri, title + "\n" + uri, false, background, icon);
                    suggestions.append (suggestion);

                    n++;
                    if (n == 3 && action == null) {
                        suggestion = new Suggestion ("about:views", _("More open tabs…"), false, background);
                        suggestion.action = true;
                        suggestions.append (suggestion);
                        break;
                    }

                    uint src = Idle.add (complete.callback);
                    yield;
                    Source.remove (src);
                }
            }

            if (cancellable.is_cancelled ())
                return null;

            return suggestions;
        }
    }
}
