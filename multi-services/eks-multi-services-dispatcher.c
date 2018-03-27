/* Copyright 2016 Endless Mobile, Inc. */

#include <gio/gio.h>
#include <glib.h>

#include <string.h>

static char *services_version;

static GOptionEntry entries[] = {
  { "services-version", 's', 0, G_OPTION_ARG_STRING, &services_version, "The eks-search-provider version to use", "VERSION" },
  { NULL }
};

static GSubprocess *
spawnv_with_appended_paths_and_fds (const char * const  *argv,
                                    const char * const  *executable_paths,
                                    const char * const  *ld_library_paths,
                                    const char * const  *xdg_data_dirs,
                                    GError             **error)
{
  g_autoptr(GSubprocessLauncher) launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_INHERIT_FDS);
  g_autofree char *executable_path_variable = g_strjoinv (":", (GStrv) executable_paths);
  g_autofree char *ld_library_path_variable = g_strjoinv (":", (GStrv) ld_library_paths);
  g_autofree char *xdg_data_dirs_variable = g_strjoinv (":", (GStrv) xdg_data_dirs);

  g_subprocess_launcher_setenv (launcher, "PATH", executable_path_variable, TRUE);
  g_subprocess_launcher_setenv (launcher, "LD_LIBRARY_PATH", ld_library_path_variable, TRUE);
  g_subprocess_launcher_setenv (launcher, "XDG_DATA_DIRS", xdg_data_dirs_variable, TRUE);

  return g_subprocess_launcher_spawnv (launcher, argv, error); 
}

/* Try and find an installed and mounted SDK with the highest priority,
 * the first item having the highest priority. */
static gboolean
find_sdk_with_highest_priority (const char * const  *sdks_in_priority_order,
                                const char          **out_sdk,
                                GError             **error)
{
  const char * const *iter = sdks_in_priority_order;

  g_return_val_if_fail (out_sdk != NULL, FALSE);

  for (; *iter != NULL; ++iter)
    {
      g_autoptr(GError) local_error = NULL;
      g_autoptr(GFile) sdk_dir = g_file_new_for_path (*iter);
      g_autoptr(GFileEnumerator) sdk_enumerator = g_file_enumerate_children (sdk_dir,
                                                                             G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                                             G_FILE_QUERY_INFO_NONE,
                                                                             NULL,
                                                                             error);

      if (sdk_enumerator == NULL)
        return FALSE;

      g_autoptr(GFileInfo) first_file_info = g_file_enumerator_next_file (sdk_enumerator,
                                                                          NULL,
                                                                          &local_error);

      if (local_error != NULL)
        {
          g_propagate_error (error, g_steal_pointer (&local_error));
          return FALSE;
        }

      if (first_file_info != NULL)
        {
          *out_sdk = *iter;
          return TRUE;
        }
    }

  *out_sdk = NULL;
  return TRUE;
}

static gboolean
dispatch_correct_service (const char  *services_version,
                          GError      **error)
{
  /* We keep track of this and then free it, the exec'd service
   * will become a child of init and inherit all our fds, thus
   * consuming the d-bus traffic that was destined for this process. */
  g_autoptr(GSubprocess) subprocess = NULL;

  if (g_strcmp0 (services_version, "1") == 0)
    {
      const char * const argv[] = {
        "/app/eos-knowledge-services/1/bin/eks-search-provider-v1",
        NULL
      };
      const char * const executable_paths[] = {
        "/app/sdk/1/bin",
        "/app/eos-knowledge-services/1/bin",
        NULL
      };
      const char * const ld_library_paths[] = {
        "/app/sdk/1/lib",
        "/app/eos-knowledge-services/1/lib",
        NULL
      };
      const char * const xdg_data_dirs[] = {
        "/app/sdk/1/share",
        "/app/eos-knowledge-services/1/share",
        NULL
      };

      subprocess = spawnv_with_appended_paths_and_fds (argv,
                                                       executable_paths,
                                                       ld_library_paths,
                                                       xdg_data_dirs,
                                                       error);
      g_subprocess_wait (subprocess, NULL, error);
      return subprocess != NULL;
    }
  else if (g_strcmp0 (services_version, "2") == 0)
    {
      const char * const candidate_sdks[] = {
        "/app/sdk/3",
        "/app/sdk/2",
        NULL
      };
      const char *sdk_path = NULL;

      if (!find_sdk_with_highest_priority (candidate_sdks, &sdk_path, error))
        return FALSE;

      if (sdk_path == NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Could not find candidate SDK for services version 2");
          return FALSE;
        }

      g_autofree char *sdk_bin_path = g_build_filename (sdk_path, "bin", NULL);
      g_autofree char *sdk_lib_path = g_build_filename (sdk_path, "lib", NULL);
      g_autofree char *sdk_share_path = g_build_filename (sdk_path, "share", NULL);

      const char * const argv[] = {
        "/app/eos-knowledge-services/2/bin/eks-search-provider-v2",
        NULL
      };
      const char * const executable_paths[] = {
        sdk_bin_path,
        "/app/eos-knowledge-services/2/bin",
        NULL
      };
      const char * const ld_library_paths[] = {
        sdk_lib_path,
        "/app/eos-knowledge-services/2/lib",
        NULL
      };
      const char * const xdg_data_dirs[] = {
        sdk_share_path,
        "/app/eos-knowledge-services/2/share",
        NULL
      };

      subprocess = spawnv_with_appended_paths_and_fds (argv,
                                                       executable_paths,
                                                       ld_library_paths,
                                                       xdg_data_dirs,
                                                       error);
      g_subprocess_wait (subprocess, NULL, error);
      return subprocess != NULL;
    }

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_FAILED,
               "Don't know how to spawn services version %s",
               services_version);
  return FALSE;
}

int
main (int    argc,
      char **argv)
{
  g_autoptr(GError) local_error = NULL;
  g_autoptr(GOptionContext) context = g_option_context_new ("- multiplexer for EknServices");

  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &local_error))
    {
      g_message ("Option parsing failed: %s\n", local_error->message);
      return 1;
    }

  if (!dispatch_correct_service (services_version, &local_error))
    {
      g_message ("Failed to dispatch correct service for version %s: %s",
                 services_version,
                 local_error->message);
      return 1;
    }

  return 0;
}
