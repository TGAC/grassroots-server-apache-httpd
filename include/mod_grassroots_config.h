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

/**
 * @file
 * @brief
 */
/*
 * mod_grassroots_config.h
 *
 *  Created on: 15 May 2015
 *      Author: billy
 */

#ifndef MOD_GRASSROOTS_CONFIG_H_
#define MOD_GRASSROOTS_CONFIG_H_

#include "apr_global_storage.h"
#include "httpd.h"
#include "http_config.h"
#include "apr_global_mutex.h"
#include "ap_provider.h"
#include "ap_socache.h"

#include "jobs_manager.h"
#include "servers_manager.h"
#include "string_utils.h"


/**
 * This datatype is used by the Apache HTTPD server
 * to be the Grassroots JobsManager.
 *
 * @extends JobsManager
 *
 * @ingroup httpd_server
 */
typedef struct APRJobsManager
{
	/** The base JobsManager */
	JobsManager ajm_base_manager;

	/**
	 * The APRGlobalStorage implementation where the
	 * ServiceJobs will be stored.
	 */
	APRGlobalStorage *ajm_store_p;
} APRJobsManager;


/**
 * This datatype is used by the Apache HTTPD server
 * to be the Grassroots ServersManager.
 *
 * @extends ServersManager
 *
 *
 * @ingroup httpd_server
 */
typedef struct APRServersManager
{
	/** The base ServersManager */
	ServersManager asm_base_manager;

	/**
	 * The APRGlobalStorage implementation where the
	 * Server definitions will be stored.
	 */
	APRGlobalStorage *asm_store_p;

} APRServersManager;


/** @publicsection */

/**
 * @brief The configuration for the Grassroots module.
 *
 * @ingroup httpd_server
 */
typedef struct
{
	/**
	 * For Location and Directory configs, this will be their value.
	 * For Serevr-wide configs, this will be NULL.
	 */
	char *glc_context_s;

	/** The path to the Grassroots installation */
	char *glc_root_path_s;

	/** The cache provider to use. */
	char *glc_provider_name_s;

	/** The JobsManager that the module is using. */
	char *glc_jobs_manager_s;

	/** The ServersManager that the module is using. */
	char *glc_servers_manager_s;

	/**
	 * The config filename that the GrassrootsServer should use.
	 * If this is <code>NULL</code> then the default of
	 * "grassroots.config" will be used.
	 */
	char *glc_config_s;

	/**
	 * The path to the folder containing the service config files.
	 * If this is <code>NULL</code> then the default of
	 * "config" will be used.
	 */
	char *glc_services_config_path_s;


	/**
   * The path to the folder containing the service module files.
   * If this is <code>NULL</code> then the default of
   * "services" will be used.
   */
	char *glc_services_path_s;


	/**
	 * The path to the folder containing the reference service files.
	 * If this is <code>NULL</code> then the default of
	 * "references" will be used.
	 */
	char *glc_references_path_s;

	/**
	 *  A lookup table where the keys are the paths for each Location and Directory directive
	 *  and the values are the NamedGrassrootsServers associated with these
	 */
	apr_hash_t *glc_servers_p;

	server_rec *glc_server_p;

} GrassrootsLocationConfig;


#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Get the Grassroots module.
 *
 * @return the GrassrootsModule.
 *
 * @ingroup httpd_server
 */
const module *GetGrassrootsModule (void);



#ifdef __cplusplus
}
#endif



#endif /* MOD_GRASSROOTS_CONFIG_H_ */
