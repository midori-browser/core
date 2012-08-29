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

extern const string LIBDIR;
extern const string MDATADIR;
extern const string PACKAGE_NAME;
extern const string SYSCONFDIR;

namespace Midori {
    public enum RuntimeMode {
        UNDEFINED,
        NORMAL,
        APP,
        PRIVATE,
        PORTABLE
    }

    namespace Paths {
        static string? exec_path = null;
        static string[] command_line = null;
        static RuntimeMode mode = RuntimeMode.UNDEFINED;

        static string? config_dir = null;
        static string? cache_dir = null;
        static string? user_data_dir = null;
        static string? tmp_dir = null;

        public static string get_readonly_config_dir (RuntimeMode new_mode) {
            assert (mode == RuntimeMode.UNDEFINED);
            if (new_mode == RuntimeMode.PORTABLE) {
                return Path.build_path (Path.DIR_SEPARATOR_S,
                    exec_path, "profile", "config");
            }
            return Path.build_path (Path.DIR_SEPARATOR_S,
                Environment.get_user_config_dir (), PACKAGE_NAME);
        }

        public static void init (RuntimeMode new_mode, string? config_base) {
            assert (mode == RuntimeMode.UNDEFINED);
            assert (new_mode != RuntimeMode.UNDEFINED);
            mode = new_mode;
            if (mode == RuntimeMode.PORTABLE) {
                config_dir = Path.build_path (Path.DIR_SEPARATOR_S,
                    exec_path, "profile", "config");
                cache_dir = Path.build_path (Path.DIR_SEPARATOR_S,
                    exec_path, "profile", "cache");
                user_data_dir = Path.build_path (Path.DIR_SEPARATOR_S,
                    exec_path, "profile", "misc");
                tmp_dir = Path.build_path (Path.DIR_SEPARATOR_S,
                    exec_path, "profile", "tmp");
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
                        Environment.get_user_config_dir (), PACKAGE_NAME);
                cache_dir = Path.build_path (Path.DIR_SEPARATOR_S,
                    Environment.get_user_cache_dir (), PACKAGE_NAME);
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

        public static unowned string get_config_dir () {
            assert (config_dir != null);
            return config_dir;
        }

        public static unowned string get_cache_dir () {
            assert (cache_dir != null);
            return cache_dir;
        }

        public static unowned string get_user_data_dir () {
            assert (user_data_dir != null);
            return user_data_dir;
        }

        public static unowned string get_tmp_dir () {
            assert (tmp_dir != null);
            return tmp_dir;
        }

        public static void init_exec_path (string[] new_command_line) {
            assert (command_line == null);
            command_line = new_command_line;
            #if HAVE_WIN32
            exec_path = win32_get_package_installation_directory_of_module ();
            #else
            string? executable;
            try {
                if (!Path.is_absolute (command_line[0])) {
                    string program = Environment.find_program_in_path (command_line[0]);
                    if (FileUtils.test (program, FileTest.IS_SYMLINK))
                        executable = FileUtils.read_link (program);
                    else
                        executable = program;
                }
                else
                    executable = FileUtils.read_link (command_line[0]);
            }
            catch (Error error) {
                executable = command_line[0];
            }

            exec_path = File.new_for_path (executable).get_parent ().get_parent ().get_path ();
            #endif
            if (strcmp (Environment.get_variable ("MIDORI_DEBUG"), "paths") == 0) {
                stdout.printf ("command_line: %s\nexec_path: %s\nres: %s\nlib: %s\n",
                               "".joinv (" ", command_line), exec_path,
                               get_res_filename (""), get_lib_path (PACKAGE_NAME));
            }
        }

        public static unowned string[] get_command_line () {
            assert (command_line != null);
            return command_line;
        }

        public static string get_lib_path (string package) {
            assert (command_line != null);
            string path = Path.build_filename (exec_path, "lib", package);
            if (Posix.access (path, Posix.F_OK) == 0)
                return path;

            if (package == PACKAGE_NAME) {
                /* Fallback to build folder */
                path = Path.build_filename ((File.new_for_path (exec_path).get_path ()), "extensions");
                if (Posix.access (path, Posix.F_OK) == 0)
                    return path;
            }

            return Path.build_filename (LIBDIR, PACKAGE_NAME);
        }

        public static string get_res_filename (string filename) {
            assert (command_line != null);
            string path = Path.build_filename (exec_path, "share", PACKAGE_NAME, "res", filename);
            if (Posix.access (path, Posix.F_OK) == 0)
                return path;

            /* Fallback to build folder */
            path = Path.build_filename ((File.new_for_path (exec_path)
                .get_parent ().get_parent ().get_path ()), "data", filename);
            if (Posix.access (path, Posix.F_OK) == 0)
                return path;

            return Path.build_filename (MDATADIR, PACKAGE_NAME, "res", filename);
        }

        public static string get_data_filename (string filename, bool res) {
            assert (command_line != null);
            string res1 = res ? PACKAGE_NAME : "";
            string res2 = res ? "res" : "";

            #if HAVE_WIN32
            string path = Path.build_filename (exec_path, "share", res1, res2, filename);
            if (Posix.access (path, Posix.F_OK) == 0)
                return path;
            #else
            string path = Path.build_filename (get_user_data_dir (), res1, res2, filename);
            if (Posix.access (path, Posix.F_OK) == 0)
                return path;

            foreach (string data_dir in Environment.get_system_data_dirs ()) {
                path = Path.build_filename (data_dir, res1, res2, filename);
                if (Posix.access (path, Posix.F_OK) == 0)
                    return path;
            }
            #endif

            return Path.build_filename (MDATADIR, res1, res2, filename);
        }

        public static string get_config_filename (string? folder, string filename) {
            assert (config_dir != null);

            foreach (string config_dir in Environment.get_system_config_dirs ()) {
                string path = Path.build_filename (config_dir, PACKAGE_NAME, folder ?? "", filename);
                if (Posix.access (path, Posix.F_OK) == 0)
                    return path;
            }

            #if HAVE_WIN32
            string path = Path.build_filename (exec_path, "etc", "xdg", PACKAGE_NAME, folder ?? "", filename);
            if (Posix.access (path, Posix.F_OK) == 0)
                return path;
            #endif

            return Path.build_filename (SYSCONFDIR, "xdg", PACKAGE_NAME, folder ?? "", filename);
        }
    }
}
