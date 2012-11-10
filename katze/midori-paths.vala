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
extern const string MIDORI_VERSION_SUFFIX;

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
        static string? readonly_dir = null;
        static string? cache_dir = null;
        static string? cache_dir_for_reading = null;
        static string? user_data_dir = null;
        static string? user_data_dir_for_reading = null;
        static string? tmp_dir = null;

        public static string get_config_dir_for_reading () {
            assert (mode != RuntimeMode.UNDEFINED);
            return readonly_dir ?? config_dir;
        }

        /* returns the path to a user configuration file whose contents should not be modified.
        to get the path to save settings, use get_config_filename() */
        public static string get_config_filename_for_reading (string filename) {
            assert (mode != RuntimeMode.UNDEFINED);
            return Path.build_path (Path.DIR_SEPARATOR_S,
                readonly_dir ?? config_dir, filename);
        }

        public bool is_readonly () {
            assert (mode != RuntimeMode.UNDEFINED);
            return readonly_dir != null;
        }

        public RuntimeMode get_runtime_mode () {
            assert (mode != RuntimeMode.UNDEFINED);
            return mode;
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
                readonly_dir = config_base ?? Path.build_path (Path.DIR_SEPARATOR_S,
                    Environment.get_user_config_dir (), PACKAGE_NAME);
                cache_dir_for_reading = Path.build_path (Path.DIR_SEPARATOR_S,
                    Environment.get_user_cache_dir (), PACKAGE_NAME);
                user_data_dir_for_reading = Environment.get_user_data_dir ();
                tmp_dir = Path.build_path (Path.DIR_SEPARATOR_S,
                    Environment.get_tmp_dir (), "midori-" + Environment.get_user_name ());
            }
            else {
                config_dir = config_base ?? Path.build_path (Path.DIR_SEPARATOR_S,
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

        public static void mkdir_with_parents (string path, int mode = 0700) {
            /* Use g_access instead of g_file_test for better performance */
            if (Posix.access (path, Posix.F_OK) == 0)
                return;
            int i = path.index_of_char (Path.DIR_SEPARATOR, 0);
            do {
                string fn = path.substring (i, -1);
                if (Posix.access (fn, Posix.F_OK) != 0) {
                    if (DirUtils.create (fn, mode) == -1)
                        return; /* Failed */
                }
                else if (!FileUtils.test (fn, FileTest.IS_SYMLINK))
                    return; /* Failed */

                i = path.index_of_char (Path.DIR_SEPARATOR, i);
            }
            while (i != -1);
        }

        public static unowned string get_config_dir_for_writing () {
            assert (config_dir != null);
            mkdir_with_parents (config_dir);
            return config_dir;
        }

        /* returns the path to a user configuration file to which it is permitted to write.
        this is also necessary for files whose state is synchronized to disk by a manager,
        e.g. cookies. */
        public static string get_config_filename_for_writing (string filename) {
            assert (mode != RuntimeMode.UNDEFINED);
            assert (config_dir != null);
            return Path.build_path (Path.DIR_SEPARATOR_S, config_dir, filename);
        }

        public static unowned string get_cache_dir () {
            assert (cache_dir != null);
            return cache_dir;
        }

        public static unowned string get_user_data_dir () {
            assert (user_data_dir != null);
            return user_data_dir;
        }

        public static unowned string get_user_data_dir_for_reading () {
            assert (user_data_dir_for_reading != null || user_data_dir != null);
            if (user_data_dir != null)
                return user_data_dir;
            return user_data_dir_for_reading;
        }

        public static unowned string get_cache_dir_for_reading () {
            assert (cache_dir_for_reading != null || cache_dir != null);
            if (cache_dir != null)
                return cache_dir;
            return cache_dir_for_reading;
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
                               get_command_line_str (), exec_path,
                               get_res_filename (""), get_lib_path (PACKAGE_NAME));
            }
        }

        public static unowned string[] get_command_line () {
            assert (command_line != null);
            return command_line;
        }

        public static string get_command_line_str () {
            assert (command_line != null);
            return string.joinv (" ", command_line).replace (Environment.get_home_dir (), "~");
        }

        public static string get_lib_path (string package) {
            assert (command_line != null);
            #if HAVE_WIN32
            return Path.build_filename (exec_path, "lib", package);
            #else
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
            #endif
        }

        public static string get_res_filename (string filename) {
            assert (command_line != null);
            #if HAVE_WIN32
            return Path.build_filename (exec_path, "share", PACKAGE_NAME, "res", filename);
            #else
            string path = Path.build_filename (exec_path, "share", PACKAGE_NAME, "res", filename);
            if (Posix.access (path, Posix.F_OK) == 0)
                return path;

            /* Fallback to build folder */
            path = Path.build_filename ((File.new_for_path (exec_path)
                .get_parent ().get_parent ().get_path ()), "data", filename);
            if (Posix.access (path, Posix.F_OK) == 0)
                return path;

            return Path.build_filename (MDATADIR, PACKAGE_NAME, "res", filename);
            #endif
        }

        /* returns the path to a file containing read-only data installed with the application
        if @res is true, looks in the midori resource folder specifically */
        public static string get_data_filename (string filename, bool res) {
            assert (command_line != null);
            string res1 = res ? PACKAGE_NAME : "";
            string res2 = res ? "res" : "";

            #if HAVE_WIN32
            return Path.build_filename (exec_path, "share", res1, res2, filename);
            #else
            string path = Path.build_filename (get_user_data_dir_for_reading (), res1, res2, filename);
            if (Posix.access (path, Posix.F_OK) == 0)
                return path;

            foreach (string data_dir in Environment.get_system_data_dirs ()) {
                path = Path.build_filename (data_dir, res1, res2, filename);
                if (Posix.access (path, Posix.F_OK) == 0)
                    return path;
            }

            return Path.build_filename (MDATADIR, res1, res2, filename);
            #endif
        }

        /* returns the path to a file containing system default configuration */
        public static string get_preset_filename (string? folder, string filename) {
            assert (exec_path != null);

            #if HAVE_WIN32
            return Path.build_filename (exec_path, "etc", "xdg", PACKAGE_NAME, folder ?? "", filename);
            #else
            foreach (string config_dir in Environment.get_system_config_dirs ()) {
                string path = Path.build_filename (config_dir, PACKAGE_NAME, folder ?? "", filename);
                if (Posix.access (path, Posix.F_OK) == 0)
                    return path;
            }

            return Path.build_filename (SYSCONFDIR, "xdg", PACKAGE_NAME, folder ?? "", filename);
            #endif
        }
    }
}
