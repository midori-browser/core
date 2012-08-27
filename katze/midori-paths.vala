/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

namespace GLib {
    #if HAVE_WIN32
    extern static string win32_get_package_installation_directory_of_module (void* hmodule = null);
    #endif
}

namespace Midori {
    public enum RuntimeMode {
        UNDEFINED,
        NORMAL,
        APP,
        PRIVATE,
        PORTABLE
    }

    namespace Paths {
        static RuntimeMode mode = RuntimeMode.UNDEFINED;
        static string? config_dir = null;
        static string? cache_dir = null;
        static string? user_data_dir = null;
        static string? tmp_dir = null;

        public static string get_readonly_config_dir (RuntimeMode new_mode) {
            assert (mode == RuntimeMode.UNDEFINED);
            if (new_mode == RuntimeMode.PORTABLE) {
                #if HAVE_WIN32
                string profile = win32_get_package_installation_directory_of_module ();
                #else
                string profile = "profile://";
                #endif
                return Path.build_path (Path.DIR_SEPARATOR_S,
                    profile, "profile", "config");
            }
            return Path.build_path (Path.DIR_SEPARATOR_S,
                Environment.get_user_config_dir (), "midori");
        }

        public static void init (RuntimeMode new_mode, string? config_base) {
            assert (mode == RuntimeMode.UNDEFINED);
            assert (new_mode != RuntimeMode.UNDEFINED);
            mode = new_mode;
            if (mode == RuntimeMode.PORTABLE) {
                #if HAVE_WIN32
                string profile = win32_get_package_installation_directory_of_module ();
                #else
                string profile = "profile://";
                #endif
                config_dir = Path.build_path (Path.DIR_SEPARATOR_S,
                    profile, "profile", "config");
                cache_dir = Path.build_path (Path.DIR_SEPARATOR_S,
                    profile, "profile", "cache");
                user_data_dir = Path.build_path (Path.DIR_SEPARATOR_S,
                    profile, "profile", "misc");
                tmp_dir = Path.build_path (Path.DIR_SEPARATOR_S,
                    profile, "profile", "tmp");
            }
            else if (mode == RuntimeMode.PRIVATE || mode == RuntimeMode.APP) {
                config_dir = "private-or-app://";
                cache_dir = "private-or-app://";
                user_data_dir = "private-or-app://";
                tmp_dir = Path.build_path (Path.DIR_SEPARATOR_S,
                    Environment.get_tmp_dir (), "midori-" + Environment.get_user_name ());
            }
            else {
                if (config_base != null)
                    config_dir = config_base;
                else
                    config_dir = Path.build_path (Path.DIR_SEPARATOR_S,
                        Environment.get_user_config_dir (), "midori");
                cache_dir = Path.build_path (Path.DIR_SEPARATOR_S,
                    Environment.get_user_cache_dir (), "midori");
                user_data_dir = Environment.get_user_data_dir ();
                tmp_dir = Path.build_path (Path.DIR_SEPARATOR_S,
                    Environment.get_tmp_dir (), "midori-" + Environment.get_user_name ());
            }
            if (strcmp (Environment.get_variable ("MIDORI_DEBUG"), "paths") == 0) {
                stdout.printf ("config: %s\ncache: %s\nuser_data: %s\ntmp: %s\n",
                               config_dir, cache_dir, user_data_dir, tmp_dir);
            }
        }

        public bool is_readonly () {
            return mode == RuntimeMode.APP || mode == RuntimeMode.PRIVATE;
        }

        public static string get_config_dir () {
            assert (config_dir != null);
            return config_dir;
        }

        public static string get_cache_dir () {
            assert (cache_dir != null);
            return cache_dir;
        }

        public static string get_user_data_dir () {
            assert (user_data_dir != null);
            return user_data_dir;
        }

        public static string get_tmp_dir () {
            assert (tmp_dir != null);
            return tmp_dir;
        }
    }
}
