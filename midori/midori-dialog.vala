/*
 Copyright (C) 2011-2012 Christian Dywan <christian@twotoats.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace Midori {
    namespace Test {
        internal static Gtk.ResponseType test_response = Gtk.ResponseType.NONE;
        public void set_dialog_response (Gtk.ResponseType response) {
            test_response = response;
        }

        internal static string? test_filename = null;
        public void set_file_chooser_filename (string filename) {
            test_filename = filename;
        }
    }

    public class FileChooserDialog : Gtk.FileChooserDialog {
        public FileChooserDialog (string title, Gtk.Window window, Gtk.FileChooserAction action) {
            /* Creates a new file chooser dialog to Open or Save and Cancel.
               The positive response is %Gtk.ResponseType.OK. */
            unowned string stock_id = Gtk.Stock.OPEN;
            if (action == Gtk.FileChooserAction.SAVE)
                stock_id = Gtk.Stock.SAVE;
            this.title = title;
            transient_for = window;
            this.action = action;
            add_buttons (Gtk.Stock.CANCEL, Gtk.ResponseType.CANCEL,
                         stock_id, Gtk.ResponseType.OK);
            icon_name = stock_id;
        }
    }

    namespace Dialog {
        public static new int run (Gtk.Dialog dialog) {
            if (Test.test_response != Gtk.ResponseType.NONE) {
                if (Test.test_filename != null && dialog is Gtk.FileChooser)
                    (dialog as Gtk.FileChooser).set_filename (Test.test_filename);
                return Test.test_response;
            }
            return dialog.run ();
        }
    }
}

