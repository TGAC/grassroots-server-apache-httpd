/*
** Copyright 2014-2016 The Earlham Institute
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/* Include the required headers from httpd */
#ifdef UNIX
	#include <unistd.h>
#endif

#include "apache_output_stream.h"
#include "apr_jobs_manager.h"
#include "apr_servers_manager.h"
#include "key_value_pair.h"
#include "mod_grassroots_config.h"
#include "apr_grassroots_servers.h"

#include "httpd.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_request.h"
#include "http_config.h"
#include "http_log.h"

#include "apr_strings.h"
#include "apr_network_io.h"
#include "apr_md5.h"
#include "apr_sha1.h"
#include "apr_hash.h"
#include "apr_base64.h"
#include "apr_dbd.h"
#include "apr_file_info.h"
#include "apr_file_io.h"
#include "apr_tables.h"
#include "util_script.h"


#include "grassroots_server.h"
#include "jansson.h"
#include "system_util.h"
#include "streams.h"
#include "util_mutex.h"
#include "async_task.h"


#ifdef _DEBUG
	#define MOD_GRASSROOTS_DEBUG	(STM_LEVEL_INFO)
#else
	#define MOD_GRASSROOTS_DEBUG	(STM_LEVEL_NONE)
#endif



/*
 * A map where the keys are the locations and
 * the values are the GrassrootsLocationConfig
 * instances for those locations
 */
static apr_hash_t *s_locations_p;



static bool s_using_apr_servers_manager_flag;


/* Define prototypes of our functions in this module */
static void RegisterHooks (apr_pool_t *pool_p);
static int GrassrootsHandler (request_rec *req_p);
static void GrassrootsChildInit (apr_pool_t *pool_p, server_rec *server_p);

static int GrassrootsPreConfig (apr_pool_t *config_pool_p, apr_pool_t *log_pool_p, apr_pool_t *temp_pool_p);
static int GrassrootsPostConfig (apr_pool_t *pconf, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s);

static const char *SetGrassrootsRootPath (cmd_parms *cmd_p, void *cfg_p, const char *arg_s);
static const char *SetGrassrootsCacheProvider (cmd_parms *cmd_p, void *cfg_p, const char *arg_s);
static const char *SetGrassrootsServersManager (cmd_parms *cmd_p, void *cfg_p, const char *arg_s);
static const char *SetGrassrootsJobsManager (cmd_parms *cmd_p, void *cfg_p, const char *arg_s);
static const char *SetGrassrootsConfigFilename (cmd_parms *cmd_p, void *cfg_p, const char *arg_s);
static const char *SetGrassrootsServiceConfigPath (cmd_parms *cmd_p, void *cfg_p, const char *arg_s);

static const char *SetGrassrootsServicesPath (cmd_parms *cmd_p, void *cfg_p, const char *arg_s);
static const char *SetGrassrootsReferencesPath (cmd_parms *cmd_p, void *cfg_p, const char *arg_s);


static void *CreateServerConfig (apr_pool_t *pool_p, server_rec *server_p);

static void *MergeServerConfig (apr_pool_t *pool_p, void *base_config_p, void *vhost_config_p);


static void *CreateDirectoryConfig (apr_pool_t *pool_p, char *context_s);
static void *MergeDirectoryConfig (apr_pool_t *pool_p, void *base_config_p, void *new_config_p);

static void *MergeConfigs (apr_pool_t *pool_p, void *base_p, void *new_p);


static GrassrootsLocationConfig *CreateConfig (apr_pool_t *pool_p, server_rec *server_p, const char *context_s);

static apr_status_t CloseInformationSystem (void *data_p);

static apr_status_t CleanUpOutputStream (void *value_p);

static apr_status_t ClearServerResources (void *value_p);

static apr_status_t CleanUpTasks (void *value_p);


static apr_status_t CleanUpGrassrootServer (void *value_p);


static bool CopyStringValue (apr_pool_t *pool_p, const char *base_src_s, const char *add_src_s, char **dest_ss);


static const char *SetGrassrootsConfigString (cmd_parms *cmd_p, char **store_ss, const char *arg_s, GrassrootsLocationConfig *config_p);


static bool AddLocationConfig (apr_pool_t *pool_p, const char *location_s, GrassrootsLocationConfig *config_p);



static int ProcessLocationsHashEntry (void *data_p, const void *key_p, apr_ssize_t key_length, const void *value_p);


/*
 * Based on code taken from http://marc.info/?l=apache-modules&m=107669698011831
 * sander@temme.net              http://www.temme.net/sander/
 */
static apr_status_t CleanUpPool (void *data_p);


static GrassrootsServer *GetOrCreateNamedGrassrootsServer (const char * const location_s, GrassrootsLocationConfig *config_p);


static const command_rec s_grassroots_directives [] =
{
	AP_INIT_TAKE1 ("GrassrootsCache", SetGrassrootsCacheProvider, NULL, ACCESS_CONF, "The provider for the Jobs Cache"),
	AP_INIT_TAKE1 ("GrassrootsConfig", SetGrassrootsConfigFilename, NULL, ACCESS_CONF, "The config file to use for this Grassroots Server"),
	AP_INIT_TAKE1 ("GrassrootsServicesConfigPath", SetGrassrootsServiceConfigPath, NULL, ACCESS_CONF, "The path to the individual services config files"),
	AP_INIT_TAKE1 ("GrassrootsServicesPath", SetGrassrootsServicesPath, NULL, ACCESS_CONF, "The path to the service module files"),
	AP_INIT_TAKE1 ("GrassrootsReferenceServicesPath", SetGrassrootsReferencesPath, NULL, ACCESS_CONF, "The path to the referred service files"),
	AP_INIT_TAKE1 ("GrassrootsRoot", SetGrassrootsRootPath, NULL, ACCESS_CONF, "The path to the Grassroots installation"),
	AP_INIT_TAKE1 ("GrassrootsServersManager", SetGrassrootsServersManager, NULL, ACCESS_CONF, "The path to the Grassroots Servers Manager Module to use"),
	AP_INIT_TAKE1 ("GrassrootsJobManager", SetGrassrootsJobsManager, NULL, ACCESS_CONF, "The path to the Grassroots Jobs Manager Module to use"),
	AP_INIT_TAKE1 ("GrassrootsServersManagersPath", SetGrassrootsServersManagerPath, NULL, ACCESS_CONF, "The path to the Grassroots Servers modules path"),
	AP_INIT_TAKE1 ("GrassrootsJobManagersManager", SetGrassrootsJobsManagerPath, NULL, ACCESS_CONF, "The path to the Grassroots Jobs Manager Module to use"),

	{ NULL }
};


static const char * const s_jobs_manager_cache_id_s = "grassroots-jobs-socache";

static const char * const s_servers_manager_cache_id_s = "grassroots-servers-socache";



/* Define our module as an entity and assign a function for registering hooks  */
module AP_MODULE_DECLARE_DATA grassroots_module =
{
    STANDARD20_MODULE_STUFF,
    CreateDirectoryConfig,   	// Per-directory configuration handler
    MergeDirectoryConfig,   	// Merge handler for per-directory configurations
    CreateServerConfig,				// Per-server configuration handler
    MergeServerConfig,				// Merge handler for per-server configurations
    s_grassroots_directives,			// Any directives we may have for httpd
    RegisterHooks    					// Our hook registering function
};


const module *GetGrassrootsModule (void)
{
	return &grassroots_module;
}



/* register_hooks: Adds a hook to the httpd process */
static void RegisterHooks (apr_pool_t *pool_p)
{
	ap_hook_pre_config (GrassrootsPreConfig, NULL, NULL, APR_HOOK_MIDDLE);

	ap_hook_post_config (GrassrootsPostConfig, NULL, NULL, APR_HOOK_MIDDLE);

  ap_hook_child_init (GrassrootsChildInit, NULL, NULL, APR_HOOK_MIDDLE);

	ap_hook_handler (GrassrootsHandler, NULL, NULL, APR_HOOK_MIDDLE);
}



static int GrassrootsPreConfig (apr_pool_t *config_pool_p, apr_pool_t *log_pool_p, apr_pool_t *temp_pool_p)
{
	int res = 500;

	apr_status_t status = ap_mutex_register (config_pool_p, s_jobs_manager_cache_id_s, NULL, APR_LOCK_DEFAULT, 0);

	if (status == APR_SUCCESS)
		{
			status = ap_mutex_register (config_pool_p, s_servers_manager_cache_id_s, NULL, APR_LOCK_DEFAULT, 0);

			if (status == APR_SUCCESS)
				{
					s_locations_p = apr_hash_make (config_pool_p);

					if (s_locations_p)
						{
							s_using_apr_servers_manager_flag = false;

							res = OK;
						}
					else
						{
							ap_log_error (APLOG_MARK, APLOG_CRIT, status, NULL, "apr_hash_make for s_locations_p failed");
						}

				}		/* if (status == APR_SUCCESS) */
			else
				{
					ap_log_error (APLOG_MARK, APLOG_CRIT, status, NULL, "ap_mutex_register for s_servers_manager_cache_id_s failed");
				}

		}		/* if (status == APR_SUCCESS) */
	else
		{
			ap_log_error (APLOG_MARK, APLOG_CRIT, status, NULL, "ap_mutex_register for s_jobs_manager_cache_id_s failed");
		}


	return res;
}


static void *CreateServerConfig (apr_pool_t *pool_p, server_rec *server_p)
{
	return ((void *) CreateConfig (pool_p, server_p, NULL));
}


static void *CreateDirectoryConfig (apr_pool_t *pool_p, char *context_s)
{
	return ((void *) CreateConfig (pool_p, NULL, context_s));
}


static GrassrootsLocationConfig *CreateConfig (apr_pool_t *pool_p, server_rec *server_p, const char *context_s)
{
	GrassrootsLocationConfig *config_p = NULL;
	apr_hash_t *locations_p = apr_hash_make (pool_p);

	if (locations_p)
		{
			apr_hash_t *servers_p = apr_hash_make (pool_p);

			if (servers_p != NULL)
				{
					char *copied_context_s = NULL;

					if (context_s)
						{
							copied_context_s = apr_pstrdup (pool_p, context_s);

							if (!copied_context_s)
								{
									ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL, NULL, "failed to copy context value \"%s\"", context_s);
									return NULL;
								}
						}

					config_p = apr_palloc (pool_p, sizeof (GrassrootsLocationConfig));

					if (config_p)
						{
							config_p -> glc_context_s = copied_context_s;
							config_p -> glc_root_path_s = NULL;
							config_p -> glc_provider_name_s = "shmcb";
							config_p -> glc_jobs_manager_s = NULL;
							config_p -> glc_servers_manager_s = NULL;
							config_p -> glc_config_s = NULL;
							config_p -> glc_services_config_path_s = NULL;
							config_p -> glc_services_path_s = NULL;
							config_p -> glc_references_path_s = NULL;
							config_p -> glc_jobs_managers_path_s = NULL;
							config_p -> glc_servers_path_s = NULL;
							config_p -> glc_servers_p = servers_p;
							config_p -> glc_server_p = server_p;
						}
				}
		}

	return config_p;
}


static bool CopyStringValue (apr_pool_t *pool_p, const char *base_src_s, const char *add_src_s, char **dest_ss)
{
	bool success_flag = false;
	const char *src_s = add_src_s ? add_src_s : base_src_s;

	if (src_s)
		{
			*dest_ss = apr_pstrdup (pool_p, src_s);

			if (*dest_ss)
				{
					success_flag = true;
				}
			else
				{
					ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL, NULL, "failed to copy config value \"%s\"", src_s);
				}
		}
	else
		{
			success_flag = true;
		}

	return success_flag;
}


static void *MergeDirectoryConfig (apr_pool_t *pool_p, void *base_p, void *new_p)
{
	return MergeConfigs (pool_p, base_p, new_p);
}



static void *MergeServerConfig (apr_pool_t *pool_p, void *base_p, void *new_p)
{
	return MergeConfigs (pool_p, base_p, new_p);
}


static void *MergeConfigs (apr_pool_t *pool_p, void *base_p, void *new_p)
{
	GrassrootsLocationConfig *base_config_p = (GrassrootsLocationConfig *) base_p;
	GrassrootsLocationConfig *new_config_p = (GrassrootsLocationConfig *) new_p;
	char *context_s = new_config_p -> glc_context_s ? new_config_p -> glc_context_s : base_config_p -> glc_context_s;
	GrassrootsLocationConfig *merged_config_p = (GrassrootsLocationConfig *) CreateDirectoryConfig (pool_p, context_s);

	if (merged_config_p)
		{
			if (CopyStringValue (pool_p, base_config_p -> glc_root_path_s, new_config_p -> glc_root_path_s, & (merged_config_p -> glc_root_path_s)))
				{
					if (CopyStringValue (pool_p, base_config_p -> glc_config_s, new_config_p -> glc_config_s, & (merged_config_p -> glc_config_s)))
						{
							if (CopyStringValue (pool_p, base_config_p -> glc_context_s, new_config_p -> glc_context_s, & (merged_config_p -> glc_context_s)))
								{
									if (CopyStringValue (pool_p, base_config_p -> glc_provider_name_s, new_config_p -> glc_provider_name_s, & (merged_config_p -> glc_provider_name_s)))
										{
											if (CopyStringValue (pool_p, base_config_p -> glc_jobs_manager_s, new_config_p -> glc_jobs_manager_s, & (merged_config_p -> glc_jobs_manager_s)))
												{
													if (CopyStringValue (pool_p, base_config_p -> glc_servers_manager_s, new_config_p -> glc_servers_manager_s, & (merged_config_p -> glc_servers_manager_s)))
														{
															if (CopyStringValue (pool_p, base_config_p -> glc_services_config_path_s, new_config_p -> glc_services_config_path_s, & (merged_config_p -> glc_services_config_path_s)))
																{
																	if (CopyStringValue (pool_p, base_config_p -> glc_services_path_s, new_config_p -> glc_services_path_s, & (merged_config_p -> glc_services_path_s)))
																		{
																			if (CopyStringValue (pool_p, base_config_p -> glc_references_path_s, new_config_p -> glc_references_path_s, & (merged_config_p -> glc_references_path_s)))
																				{

																					if (CopyStringValue (pool_p, base_config_p -> glc_jobs_managers_path_s, new_config_p -> glc_jobs_managers_path_s, & (merged_config_p -> glc_jobs_managers_path_s)))
																						{
																							if (CopyStringValue (pool_p, base_config_p -> glc_servers_path_s, new_config_p -> glc_servers_path_s, & (merged_config_p -> glc_servers_path_s)))
																								{
																									apr_hash_t *merged_servers_p = apr_hash_overlay (pool_p, new_config_p -> glc_servers_p, base_config_p -> glc_servers_p);

																									if (merged_servers_p)
																										{
																											merged_config_p -> glc_servers_p = merged_servers_p;
																											merged_config_p -> glc_server_p = new_config_p -> glc_server_p ? new_config_p -> glc_server_p : base_config_p -> glc_server_p;

																											return merged_config_p;
																										}
																									else
																										{
																											ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL, NULL, "failed to create merged grassroots servers hash table");
																										}

																								}
																							else
																								{
																									ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL, NULL, "failed to set merged config glc_servers_path_s from \"%s\" and \"%s\"", base_config_p -> glc_servers_path_s ? base_config_p -> glc_jobs_managers_path_s : "NULL", new_config_p -> glc_servers_path_s ? new_config_p -> glc_servers_path_s : "NULL");
																								}

																						}
																					else
																						{
																							ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL, NULL, "failed to set merged config glc_jobs_managers_path_s from \"%s\" and \"%s\"", base_config_p -> glc_jobs_managers_path_s ? base_config_p -> glc_jobs_managers_path_s : "NULL", new_config_p -> glc_jobs_managers_path_s ? new_config_p -> glc_jobs_managers_path_s : "NULL");
																						}

																				}
																			else
																				{
																					ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL, NULL, "failed to set merged config glc_references_path_s from \"%s\" and \"%s\"", base_config_p -> glc_services_config_path_s ? base_config_p -> glc_services_config_path_s : "NULL", new_config_p -> glc_services_config_path_s ? new_config_p -> glc_services_config_path_s : "NULL");
																				}

																		}		/* if (CopyStringValue (pool_p, base_config_p -> glc_services_path_s, new_config_p -> glc_services_path_s, & (merged_config_p -> glc_services_path_s))) */
																	else
																		{
																			ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL, NULL, "failed to set merged config glc_services_path_s from \"%s\" and \"%s\"", base_config_p -> glc_services_path_s ? base_config_p -> glc_services_path_s : "NULL", new_config_p -> glc_services_path_s ? new_config_p -> glc_services_path_s : "NULL");
																		}

																}
															else
																{
																	ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL, NULL, "failed to set merged config glc_services_config_path_s from \"%s\" and \"%s\"", base_config_p -> glc_services_config_path_s ? base_config_p -> glc_services_config_path_s : "NULL", new_config_p -> glc_services_config_path_s ? new_config_p -> glc_services_config_path_s : "NULL");
																}
														}
													else
														{
															ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL, NULL, "failed to set merged config glc_servers_manager_s from \"%s\" and \"%s\"", base_config_p -> glc_servers_manager_s ? base_config_p -> glc_servers_manager_s : "NULL", new_config_p -> glc_servers_manager_s ? new_config_p -> glc_servers_manager_s : "NULL");
														}
												}
											else
												{
													ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL, NULL, "failed to set merged config glc_jobs_manager_s from \"%s\" and \"%s\"", base_config_p -> glc_jobs_manager_s ? base_config_p -> glc_jobs_manager_s : "NULL", new_config_p -> glc_jobs_manager_s ? new_config_p -> glc_jobs_manager_s : "NULL");
												}
										}
									else
										{
											ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL, NULL, "failed to set merged config glc_provider_name_s from \"%s\" and \"%s\"", base_config_p -> glc_provider_name_s ? base_config_p -> glc_provider_name_s : "NULL", new_config_p -> glc_provider_name_s ? new_config_p -> glc_provider_name_s : "NULL");
										}
								}
							else
								{
									ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL, NULL, "failed to set merged config glc_context_s from \"%s\" and \"%s\"", base_config_p -> glc_context_s ? base_config_p -> glc_context_s : "NULL", new_config_p -> glc_context_s ? new_config_p -> glc_context_s : "NULL");
								}
						}
					else
						{
							ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL, NULL, "failed to set merged config glc_config_s from \"%s\" and \"%s\"", base_config_p -> glc_config_s ? base_config_p -> glc_config_s : "NULL", new_config_p -> glc_root_path_s ? new_config_p -> glc_config_s : "NULL");
						}
				}
			else
				{
					ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL, NULL, "failed to set merged config glc_root_path_s from \"%s\" and \"%s\"", base_config_p -> glc_root_path_s ? base_config_p -> glc_root_path_s : "NULL", new_config_p -> glc_root_path_s ? new_config_p -> glc_root_path_s : "NULL");
				}

		}
	else
		{
			ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL, NULL, "failed to allocate merged config");
		}

	return NULL;
}


static apr_status_t CloseInformationSystem (void *data_p)
{
	DestroyInformationSystem ();
	return APR_SUCCESS;
}


static void GrassrootsChildInit (apr_pool_t *pool_p, server_rec *server_p)
{
	GrassrootsLocationConfig *config_p = ap_get_module_config (server_p -> module_config, &grassroots_module);

	/* Now that we are in a child process, we have to reconnect
	 * to the global mutex and the shared segment. We also
	 * have to find out the base address of the segment, in case
	 * it moved to a new address. */
	if (InitInformationSystem ())
		{
			apr_pool_cleanup_register (pool_p, NULL, CloseInformationSystem, apr_pool_cleanup_null);


			OutputStream *log_p = AllocateApacheOutputStream (server_p);

			if (log_p)
				{
					OutputStream *error_p = AllocateApacheOutputStream (server_p);

					SetDefaultLogStream (log_p);
					apr_pool_cleanup_register (pool_p, log_p, CleanUpOutputStream, apr_pool_cleanup_null);

					if (error_p)
						{
							/* Mark the streams for deletion when the server pool expires */
							SetDefaultErrorStream (error_p);
							apr_pool_cleanup_register (pool_p, error_p, CleanUpOutputStream, apr_pool_cleanup_null);

							/* Do any clean up required by the running of asynchronous tasks */
							apr_pool_cleanup_register (pool_p, NULL, CleanUpTasks, apr_pool_cleanup_null);

		  				/*
		  				 * We should now have a set of all of the required locations that will each need
		  				 * an individual GrassrootsServer instance.
		  				 */
		  				if (apr_hash_do (ProcessLocationsHashEntry, server_p, s_locations_p) != TRUE)
		  					{
		  						ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL, server_p, "Failed to create all grassroots servers");
		  					}

						}
					else
						{
							FreeOutputStream (log_p);
							ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL , server_p, "AllocateApacheOutputStream for errors failed");
						}
				}
			else
				{
					ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL , server_p, "AllocateApacheOutputStream for log failed");
				}


		}		/* If (InitInformationSystem ()) */
	else
		{
			ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL , server_p, "InitInformationSystem failed");
		}

}


static apr_status_t ClearServerResources (void *value_p)
{
	//FreeServerResources ();

	return APR_SUCCESS;
}

static int GrassrootsPostConfig (apr_pool_t *config_pool_p, apr_pool_t *log_p, apr_pool_t *temp_p, server_rec *server_p)
{
  void *data_p = NULL;
  const char *userdata_key_s = "grassroots_post_config";
  int ret = HTTP_INTERNAL_SERVER_ERROR;
  apr_pool_t *server_pool_p = server_p -> process -> pool;

  /* Apache loads DSO modules twice. We want to wait until the second
   * load before setting up our global mutex and shared memory segment.
   * To avoid the first call to the post_config hook, we set some
   * dummy userdata in a pool that lives longer than the first DSO
   * load, and only run if that data is set on subsequent calls to
   * this hook. */
  apr_pool_userdata_get (&data_p, userdata_key_s, server_pool_p);

  if (data_p == NULL)
  	{
      /* WARNING: This must *not* be apr_pool_userdata_setn(). The
       * reason for this is because the static symbol section of the
       * DSO may not be at the same address offset when it is reloaded.
       * Since setn() does not make a copy and only compares addresses,
       * the get() will be unable to find the original userdata. */
      apr_pool_userdata_set ((const void *) 1, userdata_key_s, apr_pool_cleanup_null, server_pool_p);

      ret = OK; /* This would be the first time through */
  	}
  else
  	{
  		/*
  		 * We are now in the parent process before any child processes have been started, so this is
  		 * where any global shared memory should be allocated
       */
  		GrassrootsLocationConfig *config_p = (GrassrootsLocationConfig *) ap_get_module_config (server_p -> module_config, &grassroots_module);


  		ret = OK;

  		if (config_p -> glc_provider_name_s)
  			{
  				if (s_using_apr_servers_manager_flag)
  					{
  						APRServersManager *external_servers_manager_p = AllocateAPRServersManager (server_p, APR_SERVERS_MANAGER_CACHE_ID_S, config_pool_p, config_p -> glc_provider_name_s);

  						if (external_servers_manager_p)
  							{
  								if (!PostConfigAPRServersManager (external_servers_manager_p, config_pool_p, server_p, config_p -> glc_provider_name_s))
  									{
				  	  				ap_log_error (APLOG_MARK, APLOG_CRIT, ret, server_p, "PostConfigAPRServersManager failed for provided \"%s\"", config_p -> glc_provider_name_s);
  										ret = HTTP_INTERNAL_SERVER_ERROR;
  									}

  							}
  						else
  							{
		  	  				ap_log_error (APLOG_MARK, APLOG_CRIT, ret, server_p, "AllocateAPRServersManager failed for provided \"%s\"", config_p -> glc_provider_name_s);
  								ret = HTTP_INTERNAL_SERVER_ERROR;
  							}
  					}


  				/*
  	  		config_p -> glc_jobs_manager_p = InitAPRJobsManager (server_p, pool_p, config_p -> glc_provider_name_s);

  	  		if (config_p -> glc_jobs_manager_p)
  					{
  	  				s_jobs_manager_p = config_p -> glc_jobs_manager_p;

							PostConfigAPRJobsManager (s_jobs_manager_p, pool_p, server_p, config_p -> glc_provider_name_s);

							config_p -> glc_servers_manager_p = InitAPRServersManager (server_p, pool_p, config_p -> glc_provider_name_s);

							if (config_p -> glc_servers_manager_p)
								{
									s_servers_manager_p = config_p -> glc_servers_manager_p;

									PostConfigAPRServersManager (s_servers_manager_p, pool_p, server_p, config_p -> glc_provider_name_s);

									//PoolDebug (config_pool_p, log_p, temp_p, server_p);

									ret = OK;
								}
		  	  		else
		  	  			{
		  	  				ap_log_error (APLOG_MARK, APLOG_CRIT, ret, server_p, "Failed to create servers manager");
		  	  			}

  					}		//  if (config_p -> glc_jobs_manager_p)
  	  		else
  	  			{
  	  				ap_log_error (APLOG_MARK, APLOG_CRIT, ret, server_p, "Failed to create jobs manager");
  	  			}

  	  		*/
  			}
  		else
  			{
  				ap_log_error (APLOG_MARK, APLOG_CRIT, ret, server_p, "You need to specify an socache module to load for Grassroots to work");
  			}
  	}


	apr_pool_cleanup_register (config_pool_p, NULL, ClearServerResources, apr_pool_cleanup_null);

	ap_log_error (APLOG_MARK, APLOG_DEBUG, ret, server_p, "GrassrootsPostConfig exiting");

  return ret;
}


static apr_status_t CleanUpOutputStream (void *value_p)
{
	OutputStream *stream_p = (OutputStream *) value_p;
	FreeOutputStream (stream_p);

	return APR_SUCCESS;
}


/* Handler for the "GrassrootsRoot" directive */
static const char *SetGrassrootsRootPath (cmd_parms *cmd_p, void *cfg_p, const char *arg_s)
{
	GrassrootsLocationConfig *config_p = (GrassrootsLocationConfig *) cfg_p;

	return SetGrassrootsConfigString (cmd_p, & (config_p -> glc_root_path_s), arg_s, config_p);
}


/* Handler for the "GrassrootsConfigFile" directive */
static const char *SetGrassrootsConfigFilename (cmd_parms *cmd_p, void *cfg_p, const char *arg_s)
{
	GrassrootsLocationConfig *config_p = (GrassrootsLocationConfig *) cfg_p;

	return SetGrassrootsConfigString (cmd_p, & (config_p -> glc_config_s), arg_s, config_p);
}


/* Handler for the "GrassrootsServicesConfigPath" directive */
static const char *SetGrassrootsServiceConfigPath (cmd_parms *cmd_p, void *cfg_p, const char *arg_s)
{
	GrassrootsLocationConfig *config_p = (GrassrootsLocationConfig *) cfg_p;

	return SetGrassrootsConfigString (cmd_p, & (config_p -> glc_services_config_path_s), arg_s, config_p);
}


/* Handler for the "GrassrootsServicesPath" directive */
static const char *SetGrassrootsServicesPath (cmd_parms *cmd_p, void *cfg_p, const char *arg_s)
{
	GrassrootsLocationConfig *config_p = (GrassrootsLocationConfig *) cfg_p;

	return SetGrassrootsConfigString (cmd_p, & (config_p -> glc_services_path_s), arg_s, config_p);
}


/* Handler for the "GrassrootsReferenceServicesPath" directive */
static const char *SetGrassrootsReferencesPath (cmd_parms *cmd_p, void *cfg_p, const char *arg_s)
{
	GrassrootsLocationConfig *config_p = (GrassrootsLocationConfig *) cfg_p;

	return SetGrassrootsConfigString (cmd_p, & (config_p -> glc_services_config_path_s), arg_s, config_p);
}


static apr_status_t CleanUpTasks (void *value_p)
{
	apr_status_t status = CloseAllAsyncTasks () ? APR_SUCCESS : APR_EGENERAL;

	return status;
}


static apr_status_t CleanUpGrassrootServer (void *value_p)
{
	GrassrootsServer *grassroots_p = (GrassrootsServer *) value_p;
	FreeGrassrootsServer (grassroots_p);

	return APR_SUCCESS;
}


/* Get the cache provider that we are going to use for the jobs manager storage */
static const char *SetGrassrootsCacheProvider (cmd_parms *cmd_p, void *cfg_p, const char *arg_s)
{
	GrassrootsLocationConfig *config_p = (GrassrootsLocationConfig *) cfg_p;
  const char *err_msg_s = NULL; //ap_check_cmd_context (cmd_p, GLOBAL_ONLY);

  if (!err_msg_s)
  	{
  	  /* Argument is of form 'name:args' or just 'name'. */
  	  const char *sep_s = ap_strchr_c (arg_s, ':');

  	  if (sep_s)
  	  	{
  	  		config_p -> glc_provider_name_s = apr_pstrmemdup (cmd_p -> pool, arg_s, sep_s - arg_s);
  	      ++ sep_s;
  	  	}
  	  else
  	  	{
  	  		config_p -> glc_provider_name_s = apr_pstrdup (cmd_p -> pool, arg_s);
  	  	}

  	}		/* if (!err_msg_s)*/
  else
  	{
  		err_msg_s = apr_psprintf (cmd_p -> pool, "GrassrootsSOCache: %s", err_msg_s);
  	}

  if (err_msg_s)
  	{
  		ap_log_error (APLOG_MARK, APLOG_CRIT, APR_EGENERAL, NULL, "SetGrassrootsCacheProvider failed: \"%s\" from arg \"%s\"", err_msg_s, arg_s);
  	}

  return err_msg_s;
}


/* Handler for the "GrassrootsServersManager" directive */
static const char *SetGrassrootsServersManager (cmd_parms *cmd_p, void *cfg_p, const char *arg_s)
{
	GrassrootsLocationConfig *config_p = (GrassrootsLocationConfig *) cfg_p;

	const char *error_s = SetGrassrootsConfigString (cmd_p, & (config_p -> glc_servers_manager_s), arg_s, config_p);

	if (!error_s)
		{
			if (IsAPRServersManagerName (arg_s))
				{
					s_using_apr_servers_manager_flag = true;
				}
		}

	return error_s;
}


static const char *SetGrassrootsConfigString (cmd_parms *cmd_p, char **store_ss, const char *arg_s, GrassrootsLocationConfig *config_p)
{
	if (arg_s)
		{
			apr_pool_t *pool_p = cmd_p -> pool;
			*store_ss = apr_pstrdup (pool_p, arg_s);

			if (*store_ss)
				{
					if (config_p -> glc_context_s)
						{
							AddLocationConfig (pool_p, config_p -> glc_context_s, config_p);
						}

				}
		}
	else
		{
			*store_ss = NULL;
		}

	return NULL;
}


/* Handler for the "GrassrootsJobsManager" directive */
static const char *SetGrassrootsJobsManager (cmd_parms *cmd_p, void *cfg_p, const char *arg_s)
{
	GrassrootsLocationConfig *config_p = (GrassrootsLocationConfig *) cfg_p;

	return SetGrassrootsConfigString (cmd_p, & (config_p -> glc_jobs_manager_s), arg_s, config_p);
}



static bool AddLocationConfig (apr_pool_t *pool_p, const char *location_s, GrassrootsLocationConfig *config_p)
{
	bool success_flag = false;

	/*
	 * Does the location already exist?
	 */
	if (apr_hash_get (s_locations_p, location_s, APR_HASH_KEY_STRING))
		{
			success_flag = true;
		}
	else
		{
			char *value_s = apr_pstrdup (pool_p, location_s);

			if (value_s)
				{
					apr_hash_set (s_locations_p, value_s, APR_HASH_KEY_STRING, config_p);
					success_flag = true;
				}
		}


	return success_flag;
}


/* The handler function for our module.
 * This is where all the fun happens!
 */
static int GrassrootsHandler (request_rec *req_p)
{
	int res = DECLINED;

  /* First off, we need to check if this is a call for the grassroots handler.
   * If it is, we accept it and do our things, it not, we simply return DECLINED,
   * and Apache will try somewhere else.
   */
  if ((req_p -> handler) && (strcmp (req_p -> handler, "grassroots-handler") == 0))
  	{
  		json_t *json_req_p = NULL;
  		char *grassroots_uri_s = NULL;

  		switch (req_p -> method_number)
				{
  				case M_POST:
  					json_req_p = GetRequestBodyAsJSON (req_p);
  					grassroots_uri_s = req_p -> uri;
  					break;

  				case M_GET:
  					json_req_p = GetRequestParamsAsJSON (req_p, &grassroots_uri_s);
  					break;

  				default:
  					res = HTTP_METHOD_NOT_ALLOWED;
  					break;
				}

  		if (json_req_p && grassroots_uri_s)
  			{
					GrassrootsLocationConfig *config_p = ap_get_module_config (req_p -> per_dir_config, &grassroots_module);
					GrassrootsServer *grassroots_p = apr_hash_get (config_p -> glc_servers_p, grassroots_uri_s, APR_HASH_KEY_STRING);

					if (grassroots_p)
						{
							const char *error_s = NULL;
							json_t *res_p = ProcessServerJSONMessage (grassroots_p, json_req_p, &error_s);

							if (res_p)
								{
									char *res_s = json_dumps (res_p, JSON_INDENT (2));

									if (res_s)
										{
											res = OK;

											ap_rputs (res_s, req_p);


											#if MOD_GRASSROOTS_DEBUG >= STM_LEVEL_FINE
											PrintLog  (STM_LEVEL_FINE, __FILE__, __LINE__, "RETURNING: %s", res_s);
											#endif

											free (res_s);
										}		/* if (res_s) */
									else
										{
											res = HTTP_INTERNAL_SERVER_ERROR;
										}

									#if MOD_GRASSROOTS_DEBUG >= STM_LEVEL_FINER
									PrintJSONRefCounts (res_p, "pre decref res_p", STM_LEVEL_FINER, __FILE__, __LINE__);
									#endif

									json_decref (res_p);

								}		/* if (res_p) */
							else
								{
									ap_rprintf (req_p, "Error processing request: %s", error_s);
									res = HTTP_BAD_REQUEST;
								}

							#if MOD_GRASSROOTS_DEBUG >= STM_LEVEL_FINER
							PrintLog (STM_LEVEL_FINER, __FILE__, __LINE__, "json_req_p -> refcount %ld", json_req_p -> refcount);
							#endif
							json_decref (json_req_p);

						}		/* if (grassroots_p) */

				}		/* if (json_req_p) */
			else
				{
					ap_rprintf (req_p, "Error getting input data from request");
					res = HTTP_BAD_REQUEST;
				}

  	}		/* if ((req_p -> handler) && (strcmp (req_p -> handler, "grassroots-handler") == 0)) */

  return res;
}




/*
 * Based on code taken from http://marc.info/?l=apache-modules&m=107669698011831
 * sander@temme.net              http://www.temme.net/sander/
 */

static apr_status_t CleanUpPool (void *data_p)
{
  //ap_log_error (APLOG_MARK, APLOG_NOERRNO | APLOG_DEBUG, 0, NULL, "Pool cleanup in process %d: %s\n", getpid (), (char *) data_p);
	fprintf (stdout, "Pool cleanup in process %d: %s\n", getpid (), (char *) data_p);
	fflush (stdout);

	return OK;
}



static GrassrootsServer *GetOrCreateNamedGrassrootsServer (const char * const location_s, GrassrootsLocationConfig *config_p)
{
  /*
   * Does it already exist?
   */
  GrassrootsServer *grassroots_p = (GrassrootsServer *) apr_hash_get (config_p -> glc_servers_p, location_s, APR_HASH_KEY_STRING);

  if (!grassroots_p)
  	{
  		/*
  		 * It's a new server, so we need to create it
  		 */
  		if (config_p -> glc_root_path_s)
  			{
					apr_pool_t *pool_p = apr_hash_pool_get (config_p -> glc_servers_p);
  				JobsManager *jobs_manager_p = NULL;
  				MEM_FLAG jobs_manager_mem = MF_ALREADY_FREED;
  				APRServersManager *servers_manager_p = NULL;
  				MEM_FLAG servers_manager_mem = MF_ALREADY_FREED;


  				/*
  				 * Are we using the apache-supplied servers manager?
  				 */
  				if (IsAPRServersManagerName (config_p -> glc_servers_manager_s))
  					{
  						servers_manager_p = ChildInitAPRServersManager (location_s, pool_p, config_p);

  						if (servers_manager_p)
  							{
  								servers_manager_mem = MF_SHADOW_USE;
  							}
  						else
  							{


  							}

  					}		/* if (IsAPRServersManagerName (config_p -> glc_servers_manager_s)) */

  				grassroots_p = AllocateGrassrootsServer (config_p -> glc_root_path_s, config_p -> glc_config_s, config_p -> glc_services_config_path_s, config_p -> glc_services_path_s,
  					config_p -> glc_references_path_s, config_p -> glc_jobs_managers_path_s, config_p -> glc_servers_path_s, jobs_manager_p, jobs_manager_mem, & (servers_manager_p -> asm_base_manager), servers_manager_mem);

  				if (grassroots_p)
  					{
  						apr_pool_t *pool_p = apr_hash_pool_get (config_p -> glc_servers_p);
  						char *copied_location_s = apr_pstrdup (pool_p, location_s);

  						if (copied_location_s)
  							{
  								apr_hash_set (config_p -> glc_servers_p, copied_location_s, APR_HASH_KEY_STRING, grassroots_p);
  							}

  						apr_pool_cleanup_register (pool_p, grassroots_p, CleanUpGrassrootServer, apr_pool_cleanup_null);


  					}
  			}

  	}		/* if (!grassroots_p) */

	return grassroots_p;
}


static int ProcessLocationsHashEntry (void *data_p, const void *key_p, apr_ssize_t key_length, const void *value_p)
{
	int res = 0;
	server_rec *server_p = (server_rec *) data_p;
	const char *location_s = (const char *) key_p;
	GrassrootsLocationConfig *config_p = (GrassrootsLocationConfig *) value_p;
	GrassrootsServer *grassroots_p = GetOrCreateNamedGrassrootsServer (location_s, config_p);

	if (grassroots_p)
		{
			res = 1;
		}
	else
		{
			ap_log_error (APLOG_MARK, APLOG_CRIT, res, server_p, "Failed to create a Grassroots Server instance for \"%s\"", location_s);
		}

	return res;
}





