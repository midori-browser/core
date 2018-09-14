/*
 Copyright (C) 2012-2018 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    public class SuggestionItem : DatabaseItem {
        public string? search { get; protected set; }

        SuggestionItem (string uri, string? title) {
            base (uri, title, 0);
        }

        public SuggestionItem.for_input (string uri, string? title) {
            base (uri, title, 0);
            this.search = uri;
        }
    }

    public interface CompletionActivatable : Peas.ExtensionBase {
        public abstract Completion completion { owned get; set; }
        public abstract void activate ();
    }

    public class Completion : Object, ListModel {
        List<ListModel> models = new List<ListModel> ();
        public bool incognito { get; construct set; default = false; }
        public string? key { get; set; default = null; }

        construct {
            var model = new ListStore (typeof (DatabaseItem));
            // Allow DuckDuckGo to distinguish Midori and in turn share revenue
            model.append (new SuggestionItem.for_input ("https://duckduckgo.com/?q=%s&t=midori", _("Search with DuckDuckGo")));
            models.append (model);

            try {
                add (HistoryDatabase.get_default (incognito));
            } catch (DatabaseError error) {
                debug ("Failed to initialize completion model: %s", error.message);
            }

            var extensions = Plugins.get_default ().plug<CompletionActivatable> ("completion", this);
            extensions.extension_added.connect ((info, extension) => ((CompletionActivatable)extension).activate ());
            extensions.foreach ((extensions, info, extension) => { extensions.extension_added (info, extension); });
        }

        public Completion (bool incognito) {
            Object (incognito: incognito);
        }

        /*
         * Add a model to complete from. Items need to be based on DatabaseItem
         * and filtered by key if set.
         */
        public void add (ListModel model) {
            if (model is Database) {
                bind_property ("key", model, "key");
            }
            model.items_changed.connect (model_changed);
            models.append (model);
        }

        void model_changed (ListModel model, uint position, uint removed, uint added) {
            uint index = 0;
            foreach (var other in models) {
                if (model == other) {
                    items_changed (position + index, removed, added);
                    break;
                }
                index += other.get_n_items ();
            }
        }

        public Type get_item_type () {
            return typeof (DatabaseItem);
        }

        public Object? get_item (uint position) {
            uint index = 0;
            foreach (var model in models) {
                uint count = model.get_n_items ();
                if (position < index + count) {
                    return model.get_item (position - index);
                }
                index += count;
            }
            return null;
        }

        public uint get_n_items () {
            uint count = 0;
            foreach (var model in models) {
                count += model.get_n_items ();
            }
            return count;
        }
    }
}
