/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

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

        public Suggestion (string? uri, string? markup, bool use_markup=false,
            string? background=null, GLib.Icon? icon=null) {

            GLib.Object (uri: uri, markup: markup, use_markup: use_markup,
                         background: background, icon: icon);
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
            N
        }

        public Autocompleter (GLib.Object app) {
            this.app = app;
            completions = new List<Completion> ();
            next_position = 0;
            model = new Gtk.ListStore (Columns.N,
                typeof (Gdk.Pixbuf), typeof (string), typeof (string),
                typeof (string), typeof (float));
        }

        public void add (Completion completion) {
            completion.prepare (app);
            completion.position = next_position;
            next_position += completion.max_items;
            completions.append (completion);
        }

        public bool can_complete (string text) {
            foreach (var completion in completions)
                if (completion.can_complete (text))
                    return true;
            return false;
        }

        private void fill_model (GLib.Object? object, AsyncResult result) {
            var completion = object as Completion;
            List<Suggestion>? suggestions = completion.complete.end (result);
            if (suggestions == null)
                return;

            if (need_to_clear) {
                model.clear ();
                need_to_clear = false;
                current_count = 0;
            }

            int count = 0;
            foreach (var suggestion in suggestions) {
                model.insert_with_values (null, completion.position + count,
                    Columns.URI, suggestion.uri,
                    Columns.MARKUP, suggestion.use_markup
                    ? suggestion.markup : Markup.escape_text (suggestion.markup),
                    Columns.ICON, suggestion.icon,
                    Columns.BACKGROUND, suggestion.background,
                    Columns.YALIGN, 0.25);

                count++;
                if (count > completion.max_items)
                    break;
            }

            current_count += count;
            populated (current_count);
        }

        public signal void populated (uint count);

        public async void complete (string text) {
            if (cancellable != null)
                cancellable.cancel ();
            cancellable = new Cancellable ();
            need_to_clear = true;

            foreach (var completion in completions) {
                if (completion.can_complete (text))
                    completion.complete.begin (text, null, cancellable, fill_model);

                uint src = Idle.add (complete.callback);
                yield;
                Source.remove (src);

                if (cancellable.is_cancelled ())
                    break;
            }
        }

        public bool can_action (string action) {
            foreach (var completion in completions)
                if (completion.can_action (action))
                    return true;
            return false;
        }

        public async void action (string action, string text) {
            if (cancellable != null)
                cancellable.cancel ();
            cancellable = new Cancellable ();
            need_to_clear = true;

            foreach (var completion in completions) {
                if (completion.can_action (action))
                    completion.complete.begin (text, action, cancellable, fill_model);
            }
        }
    }
}
