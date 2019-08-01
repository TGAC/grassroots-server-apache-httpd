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
/*
 * apr_grassroots_servers.c
 *
 *  Created on: 31 Jul 2019
 *      Author: billy
 */


#include "apr_grassroots_servers.h"


NamedGrassrootsServer *AllocateNamedGrassrootsServer (const char *location_s, GrassrootsServer *server_p, apr_pool_t *pool_p)
{
	char *copied_location_s = apr_pstrdup (pool_p, location_s);

	if (copied_location_s)
		{
			NamedGrassrootsServer *named_server_p = apr_palloc (pool_p, sizeof (NamedGrassrootsServer));

			if (named_server_p)
				{
					named_server_p -> ngsn_location_s = copied_location_s;
					named_server_p -> ngsn_server_p = server_p;

					return named_server_p;
				}


		}

	return NULL;
}


void FreeNamedGrassrootsServer (NamedGrassrootsServer *server_p)
{

}


