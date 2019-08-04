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
 * apr_grassroots_servers.h
 *
 *  Created on: 31 Jul 2019
 *      Author: billy
 */

#ifndef SERVERS_APACHE_SERVER_INCLUDE_APR_GRASSROOTS_SERVERS_H_
#define SERVERS_APACHE_SERVER_INCLUDE_APR_GRASSROOTS_SERVERS_H_


#include "grassroots_server.h"
#include "linked_list.h"

#include "apr_pools.h"


typedef struct NamedGrassrootsServer
{
	char *ngsn_location_s;

	GrassrootsServer *ngsn_server_p;
} NamedGrassrootsServer;


NamedGrassrootsServer *AllocateNamedGrassrootsServer (const char *location_s, GrassrootsServer *server_p);


void FreeNamedGrassrootsServer (NamedGrassrootsServer *server_p);



#endif /* SERVERS_APACHE_SERVER_INCLUDE_APR_GRASSROOTS_SERVERS_H_ */
