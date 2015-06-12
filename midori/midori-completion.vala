/*
 Copyright (C) 2012-2013 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public class Suggestion : GLib.Object {
        public string? uri { get; set; }
        public string? markup { get; set; }
        public bool use_markup { get; set; }
        public string? background { get; set; }
        public GLib.Icon? icon {  get; set; }
        public bool action { get; set; default = false; }
        public int priority {get; set; default = 0; }

        public Suggestion (string? uri, string? markup, bool use_markup=false,
            string? background=null, GLib.Icon? icon=null, int? priority=0) {

            GLib.Object (uri: uri, markup: markup, use_markup: use_markup,
                         background: background, icon: icon, priority: priority);
        }
    }

    public abstract class Completion : GLib.Object {
        public string? description { get; set; }
        public int max_items { get; internal set; default = 25; }
        internal int position { get; set; }

        public abstract void prepare (GLib.Object app);
        public abstract bool can_complete (string prefix);
        public abstract bool can_action (string action);
        public abstract async List<Suggestion>? complete (string text, string? action, Cancellable cancellable);
    }

    public class Autocompleter : GLib.Object {
        private GLib.Object app;
        private List<Completion> completions;
        private int next_position;
        public Gtk.ListStore model { get; private set; }
        private bool need_to_clear = false;
        private uint current_count = 0;
        private Cancellable? cancellable = null;

        public enum Columns {
            ICON,
            URI,
            MARKUP,
            BACKGROUND,
            YALIGN,
            SIZE,
            PRIORITY,
            N
        }

        public Autocompleter (GLib.Object app) {
            this.app = app;
            completions = new List<Completion> ();
            next_position = 0;
            model = new Gtk.ListStore (Columns.N,
                typeof (GLib.Icon), typeof (string), typeof (string),
                typeof (string), typeof (float), typeof (uint), typeof (int));
        }

        public void add (Completion completion) {
            completion.prepare (app);
            completion.position = next_position;
            next_position += completion.max_items;
            completions.append (completion);
        }

        public bool can_complete (string text) {
            foreach (unowned Completion completion in completions)
                if (completion.can_complete (text))
                    return true;
            return false;
        }

        private GLib.Icon? scale_if_needed (GLib.Icon? icon) {
            if (icon is Gdk.Pixbuf) {
                var pixbuf = icon as Gdk.Pixbuf;
                int icon_width = 16, icon_height = 16;
                Gtk.icon_size_lookup (Gtk.IconSize.MENU, out icon_width, out icon_height);
                if (pixbuf.width > icon_width || pixbuf.height > icon_height)
                    return pixbuf.scale_simple (icon_width, icon_height, Gdk.InterpType.BILINEAR);
            }
            return icon;
        }

        private void fill_model (Midori.Completion completion, List<Midori.Suggestion>? suggestions) {
            if (need_to_clear) {
                model.clear ();
                need_to_clear = false;
                current_count = 0;
            }
            int start = 0;
            // find the first index with priority greater than the current one
            Gtk.TreeModelForeachFunc find_index = (model, path, iter) => {
                GLib.Value priority;

                model.get_value (iter,Columns.PRIORITY, out priority);
                if ((int)priority < completion.position) {
                    start++;
                    return false;
                }
                return true;
            };
            model.foreach(find_index);
            int count = 0;
            if (completion.description != null) {
                model.insert_with_values (null, start,
                    Columns.URI, "about:completion-description",
                    Columns.MARKUP, "<b>%s</b>\n".printf (Markup.escape_text (completion.description)),
                    Columns.ICON, null,
                    Columns.SIZE, Gtk.IconSize.MENU,
                    Columns.BACKGROUND, null,
                    Columns.YALIGN, 0.25,
                    Columns.PRIORITY, completion.position);
                count++;
            }

            foreach (unowned Suggestion suggestion in suggestions) {
                if (suggestion.uri == null) {
                    warning ("suggestion.uri != null");
                    continue;
                }
                if (suggestion.markup == null) {
                    warning ("suggestion.markup != null");
                    continue;
                }
                model.insert_with_values (null, start + count,
                    Columns.URI, suggestion.uri,
                    Columns.MARKUP, suggestion.use_markup
                    ? suggestion.markup : Markup.escape_text (suggestion.markup),
                    Columns.ICON, scale_if_needed (suggestion.icon),
                    Columns.SIZE, Gtk.IconSize.MENU,
                    Columns.BACKGROUND, suggestion.background,
                    Columns.YALIGN, 0.25,
                    Columns.PRIORITY, completion.position);

                count++;
                if (count > completion.max_items)
                    break;
            }

            current_count += count;
            populated (current_count);
        }

        public signal void populated (uint count);

        private async void complete_wrapped (Completion completion, string text, string? action, Cancellable cancellable) {
            List<Midori.Suggestion>? suggestions = yield completion.complete (text, action, cancellable);
            if (!cancellable.is_cancelled () && suggestions != null)
                fill_model (completion, suggestions);
        }

        public async void complete (string text) {
            if (cancellable != null)
                cancellable.cancel ();
            cancellable = new Cancellable ();
            need_to_clear = true;

            foreach (unowned Completion completion in completions) {
                if (completion.can_complete (text))
                    yield complete_wrapped (completion, text, null, cancellable);
            }
        }

        public bool can_action (string action) {
            if (action == "about:completion-description")
                return true;
            foreach (unowned Completion completion in completions)
                if (completion.can_action (action))
                    return true;
            return false;
        }

        public async void action (string action, string text) {
            if (action == "about:completion-description")
                return;

            if (cancellable != null)
                cancellable.cancel ();
            cancellable = new Cancellable ();
            need_to_clear = true;

            foreach (unowned Completion completion in completions) {
                if (completion.can_action (action))
                    complete_wrapped.begin (completion, text, action, cancellable);
            }
        }
    }
}
