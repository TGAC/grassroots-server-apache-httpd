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

#include <string.h>


#include "key_value_pair.h"

#include "httpd.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_request.h"
#include "http_log.h"

#include "ap_release.h"

#include "apr_strings.h"
#include "apr_tables.h"
#include "util_script.h"


#include "typedefs.h"
//#include "byte_buffer.h"
#include "streams.h"
#include "json_util.h"
#include "operation.h"
#include "schema_version.h"
#include "json_tools.h"


#ifdef _DEBUG
#define KEY_VALUE_PAIR_DEBUG	(STM_LEVEL_FINE)
#else
#define KEY_VALUE_PAIR_DEBUG	(STM_LEVEL_NONE)
#endif


typedef struct JsonRequest
{
	json_t *jr_json_p;
	request_rec *jr_req_p;
} JsonRequest;


/**********************************/
/******** STATIC PROTOTYPES *******/
/**********************************/


#if AP_SERVER_MAJORVERSION_NUMBER >= 2

#if AP_SERVER_MINORVERSION_NUMBER == 4
static bool ConvertFormPairToKeyValuePair (request_rec *req_p, ap_form_pair_t *pair_p, KeyValuePair *key_value_pair_p);

static json_t *ConvertGetParametersToJSON (request_rec *req_p);

static json_t *ConvertPostParametersToJSON (request_rec *req_p);

#elif AP_SERVER_MINORVERSION_NUMBER == 2		/* #if AP_SERVER_MINORVERSION_NUMBER == 4 */

#endif		/* #else #if AP_SERVER_MINORVERSION_NUMBER == 2 */

#endif		/* #if AP_SERVER_MAJORVERSION_NUMBER >= 2 */


static int AddParameter (void *rec_p, const char *key_s, const char *value_s);

static bool AddJsonChild (json_t *parent_p, const char *key_s, const char *value_s, request_rec *req_p);


static int ReadRequestBody (request_rec *req_p, ByteBuffer *buffer_p);

static int AddParamsToJSON (void *rec_p, const char *key_s, const char *value_s);


static json_t *GetServiceRequestFromURI (const char *service_name_s, apr_table_t *params_table_p);

static json_t *GetOperationRequestFromURI (const char *operation_s, apr_table_t *params_table_p);


/**********************************/
/********** API METHODS ***********/
/**********************************/


json_t *GetRequestBodyAsJSON (request_rec *req_p)
{
	json_t *params_p = NULL;
	ByteBuffer *buffer_p = AllocateByteBuffer (1024);

	if (buffer_p)
		{
			int res = ReadRequestBody (req_p, buffer_p);

			if (res == 0)
				{
					json_error_t err;
					const char *data_s = GetByteBufferData (buffer_p);
					params_p = json_loads (data_s, 0, &err);

#if KEY_VALUE_PAIR_DEBUG >= STM_LEVEL_FINER
					PrintLog (STM_LEVEL_FINER, __FILE__, __LINE__, "Request Body:\\n%s\\n", data_s);
#endif

					if (!params_p)
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "error decoding response: \"%s\"\n\"%s\"\n\"%s\"\n%d %d %d\n", data_s, err.text, err.source, err.line, err.column, err.position);
							ap_rprintf (req_p, "error decoding response: \"%s\"\n\"%s\"\n\"%s\"\n%d %d %d\n", data_s, err.text, err.source, err.line, err.column, err.position);
						}
				}
			else
				{
					PrintLog (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to read http request body: %d", res);
				}

			FreeByteBuffer (buffer_p);
		}
	else
		{
			PrintLog (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate byte buffer to read http request body");
		}


	return params_p;
}


json_t *GetRequestParamsAsJSON (request_rec *req_p, char **grassroots_uri_ss)
{
	json_t *json_req_p = NULL;
	apr_table_t *params_table_p = NULL;
	const char *path_s = req_p -> path_info;

	/*
	 * Then get all of the params
	 */
	ap_args_to_table (req_p, &params_table_p);

	/*
	 * The first level is the service name
	 */
	if (path_s)
		{
			const char *SERVICE_S = "/service/";
			const char *OPERATION_S = "/operation/";
			const char *api_s = Strrstr (path_s, SERVICE_S);
			char *root_uri_s = NULL;

			if (api_s)
				{
					const char *service_s = api_s + strlen (SERVICE_S);

					json_req_p = GetServiceRequestFromURI (service_s, params_table_p);

					if (!json_req_p)
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "GetServiceRequestFromURI failed for \"%s\"", path_s);
						}


				}		/* if (strncmp (path_s, SERVICE_S, l) == 0) */
			else if ((api_s = Strrstr (path_s, OPERATION_S)) != NULL)
				{
					const char *operation_s = api_s + strlen (OPERATION_S);

					json_req_p = GetOperationRequestFromURI (operation_s, params_table_p);

					if (!json_req_p)
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "GetOperationRequestFromURI failed for \"%s\"", path_s);
						}
				}


			root_uri_s = apr_pstrndup (req_p -> pool, req_p -> uri, strlen (req_p -> uri) - strlen (path_s));

			if (root_uri_s)
				{
					*grassroots_uri_ss = root_uri_s;
				}
			else
				{
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to copy first " SIZET_FMT " chars from \"%s\"", api_s - path_s, path_s);
				}

		}

	return json_req_p;
}



/**********************************/
/********* STATIC METHODS *********/
/**********************************/


static int AddParamsToJSON (void *rec_p, const char *key_s, const char *value_s)
{
	json_t *param_p = json_object ();

	if (param_p)
		{
			if (SetJSONString (param_p, PARAM_NAME_S, key_s))
				{
					if (SetJSONString (param_p, PARAM_CURRENT_VALUE_S, value_s))
						{
							if (SetJSONBoolean (param_p, PARAM_VALUE_SET_FROM_TEXT_S, true))
								{
									json_t *params_p = (json_t *) rec_p;

									if (json_array_append_new (params_p, param_p) == 0)
										{
											return 1;
										}
								}
						}
				}

			json_decref (param_p);
		}

	return 0;
}


#if AP_SERVER_MAJORVERSION_NUMBER >= 2

#if AP_SERVER_MINORVERSION_NUMBER == 4

static bool ConvertFormPairToKeyValuePair (request_rec *req_p, ap_form_pair_t *pair_p, KeyValuePair *key_value_pair_p)
{
	bool success_flag = false;
	apr_off_t len;
	apr_size_t size;
	char *buffer_s = NULL;

	apr_brigade_length (pair_p -> value, 1, &len);
	size = (apr_size_t) len;

	buffer_s = apr_palloc (req_p -> pool, size + 1);

	if (buffer_s)
		{
			char *name_s = apr_pstrdup (req_p -> pool, pair_p -> name);

			if (name_s)
				{
					apr_brigade_flatten (pair_p -> value, buffer_s, &size);
					* (buffer_s + len) = '\0';

					key_value_pair_p -> kvp_key_s = name_s;
					key_value_pair_p -> kvp_value_s = buffer_s;										

					success_flag = true;
				}
		}


	return success_flag;
}


static json_t *ConvertGetParametersToJSON (request_rec *req_p)
{
	json_t *root_p = json_object ();

	if (root_p)
		{
			apr_table_t *params_p = NULL;
			JsonRequest json_req;

			json_req.jr_req_p = req_p;
			json_req.jr_json_p = root_p;

			ap_args_to_table (req_p, &params_p);  

			if (apr_table_do (AddParameter, &json_req, params_p, NULL) == TRUE)
				{

				}
			else
				{

				}

		}		/* if (root_p) */
		else
			{

			}

	return root_p;
}


static json_t *ConvertPostParametersToJSON (request_rec *req_p)
{
	json_t *root_p = NULL;
	apr_array_header_t *pairs_p = NULL;
	int res;

	/* get the form parameters */
	res = ap_parse_form_data (req_p, NULL, &pairs_p, -1, HUGE_STRING_LEN);

	if ((res == OK) && pairs_p)
		{
			/* 
			 * Check to see it the params are individual json objects 
			 * or if they are a single entry
			 */
			if (pairs_p -> nelts == 1)
				{
					KeyValuePair kvp;

					ap_form_pair_t *pair_p = (ap_form_pair_t *) apr_array_pop (pairs_p);

					if (ConvertFormPairToKeyValuePair (req_p, pair_p, &kvp))
						{
							if ((kvp.kvp_key_s) && (strcmp (kvp.kvp_key_s, "json") == 0))
								{
									json_error_t error;
									root_p = json_loads (kvp.kvp_value_s, JSON_PRESERVE_ORDER, &error);

									if (!root_p)
										{
											ap_log_rerror (APLOG_MARK, APLOG_ERR, 0, req_p, "Failed to parse \"%s\", error: %s, source %s line %d columd %d offset %d", 
																		 kvp.kvp_value_s, error.text, error.source, error.line, error.column, error.position);
										}
								}
							else
								{
									json_t *root_p = json_object ();

									if (root_p)
										{
											AddJsonChild (root_p, kvp.kvp_key_s, kvp.kvp_value_s, req_p);
										}		/* if (root_p) */	
									else
										{
											ap_log_rerror (APLOG_MARK, APLOG_ERR, 0, req_p, "Not enough memory to allocate root json object");									
										}		
								}

						}		/* if (ConvertFormPairToKeyValuePair (req_p, pair_p, &kvp)) */
					else
						{

						}
				}
			else
				{
					json_t *root_p = json_object ();

					if (root_p)
						{
							bool success_flag = true;

							/* Pop each parameter pair and add it to our json array */
							while (pairs_p && !apr_is_empty_array (pairs_p)) 
								{
									KeyValuePair kvp;

									ap_form_pair_t *pair_p = (ap_form_pair_t *) apr_array_pop (pairs_p);

									if (ConvertFormPairToKeyValuePair (req_p, pair_p, &kvp))
										{
											success_flag = AddJsonChild (root_p, kvp.kvp_key_s, kvp.kvp_value_s, req_p);											
										}
									else
										{

										}

								}		/* while (pairs_p && !apr_is_empty_array (pairs_p)) */

						}		/* if (root_p) */	
					else
						{
							ap_log_rerror (APLOG_MARK, APLOG_ERR, 0, req_p, "Not enough memory to allocate root json object");									
						}		

				}

		}		/* if ((res == OK) && pairs_p)  */			
	else
		{
			ap_log_rerror (APLOG_MARK, APLOG_ERR, 0, req_p, "Failed to get post parameters from %s", req_p -> uri);									
		}  			 

	return root_p;
}

#endif		/* #if AP_SERVER_MINORVERSION_NUMBER == 4 */

#endif		/* #if AP_SERVER_MAJORVERSION_NUMBER >= 2 */



static int AddParameter (void *rec_p, const char *key_s, const char *value_s)
{
	int res_flag = FALSE;
	JsonRequest *json_req_p = (JsonRequest *) rec_p;

	if (AddJsonChild (json_req_p -> jr_json_p, key_s, value_s, json_req_p -> jr_req_p))
		{
			res_flag = TRUE;
		}

	return res_flag;
}



/*
 * break
 *
 * 	foo.bar.stuff = bob
 *
 * into
 *
 * 	foo {
 * 		bar {
 * 			stuff = bob
 *		}
 * 	}
 */

static bool AddJsonChild (json_t *parent_p, const char *key_s, const char *value_s, request_rec *req_p)
{
	bool success_flag = true;		
	char *copied_key_s = apr_pstrdup (req_p -> pool, key_s);

	if (copied_key_s)
		{
			char *last_p = NULL;
			char *this_token_p = apr_strtok (copied_key_s, ".", &last_p);
			char *next_token_p = NULL;
			bool loop_flag = (this_token_p != NULL);
			json_t *child_p = NULL;

			while (loop_flag && success_flag)
				{
					next_token_p = apr_strtok (NULL, ".", &last_p);

					child_p = json_object_get (parent_p, this_token_p);

					if (!child_p)
						{
							success_flag = false;

							if (next_token_p)
								{
									child_p = json_object ();
								}
							else
								{
									child_p = json_string (value_s);									
								}


							if (child_p)
								{
									if (json_object_set_new (parent_p, this_token_p, child_p) == 0)
										{
											success_flag = true;
										}
									else
										{
											PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Couldn't add json value for %s to json parameters", this_token_p);
											ap_log_rerror (APLOG_MARK, APLOG_ERR, 0, req_p, "Couldn't add json value for %s to json parameters", this_token_p);

											json_decref (child_p);
											child_p = NULL;
										}
								}
							else
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Not enough memory to allocate json child  for %s", this_token_p);
									ap_log_rerror (APLOG_MARK, APLOG_ERR, 0, req_p, "Not enough memory to allocate json child  for %s", this_token_p);
								}
						}		/* if (!child_p) */

					if (success_flag)
						{
							parent_p = child_p;

							if (next_token_p)
								{
									this_token_p = next_token_p;
									loop_flag = (this_token_p != NULL);									
								}
							else
								{
									loop_flag = false;
								}
						}

				}		/* while (loop_flag) */

		}		/* if (copied_key_s) */
	else
		{
			ap_log_rerror (APLOG_MARK, APLOG_ERR, 0, req_p, "Not enough memory to copy %s", key_s);
			success_flag = false;
		}
	return success_flag;
}


static int ReadRequestBody (request_rec *req_p, ByteBuffer *buffer_p)
{
	int ret = ap_setup_client_block (req_p, REQUEST_CHUNKED_ERROR);

	if (ret == OK)
		{
			if (ap_should_client_block (req_p))
				{
					/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
					char         temp_s [HUGE_STRING_LEN] = { 0 };
					apr_off_t    rsize, len_read, rpos = 0;
					apr_off_t length = req_p->remaining;
					/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
					const char *buffer_s = (const char *) apr_pcalloc (req_p -> pool, (apr_size_t) (length + 1));

					while (((len_read = ap_get_client_block (req_p, temp_s, HUGE_STRING_LEN)) > 0) && (ret == OK))
						{
							if((rpos + len_read) > length)
								{
									rsize = length - rpos;
								}
							else
								{
									rsize = len_read;
								}

							memcpy ((char *) buffer_s + rpos, temp_s, (size_t) rsize);
							rpos += rsize;
						}

					if (!AppendToByteBuffer (buffer_p, buffer_s, length))
						{
							ret = HTTP_INTERNAL_SERVER_ERROR;
						}

				}
		}

	return ret;
}



int ReadBody (request_rec *req_p, ByteBuffer *buffer_p)
{
	int ret = ap_setup_client_block (req_p, REQUEST_CHUNKED_ERROR);

	if (ret == OK)
		{
			if (ap_should_client_block (req_p))
				{
					/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
					char         temp_s [HUGE_STRING_LEN];
					apr_off_t    rsize, len_read, rpos = 0;
					apr_off_t length = req_p->remaining;
					/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

					while (((len_read = ap_get_client_block (req_p, temp_s, sizeof(temp_s))) > 0) && (ret == OK))
						{
							if((rpos + len_read) > length)
								{
									rsize = length - rpos;
								}
							else
								{
									rsize = len_read;
								}

							if (!AppendToByteBuffer (buffer_p, temp_s, rsize))
								{
									ret = HTTP_INTERNAL_SERVER_ERROR;
								}
						}
				}
		}

	return ret;
}


static json_t *GetServiceRequestFromURI (const char *service_name_s, apr_table_t *params_table_p)
{
	json_t *service_req_p = json_object ();

	if (service_req_p)
		{
			if (SetJSONString (service_req_p, SERVICE_NAME_S, service_name_s))
				{
					if (SetJSONString (service_req_p, SERVICE_ALIAS_S, service_name_s))
						{
							if (SetJSONBoolean (service_req_p, SERVICE_RUN_S, true))
								{
									json_t *parameter_set_json_p = json_object ();

									if (parameter_set_json_p)
										{
											if (json_object_set_new (service_req_p, PARAM_SET_KEY_S, parameter_set_json_p) == 0)
												{
													json_t *params_json_p = json_array ();

													if (params_json_p)
														{
															if (json_object_set_new (parameter_set_json_p, PARAM_SET_PARAMS_S, params_json_p) == 0)
																{
																	int res = apr_table_do (AddParamsToJSON, params_json_p, params_table_p, NULL);

																	if (res == TRUE)
																		{
																			json_t *root_p = json_object ();

																			if (root_p)
																				{
																					json_t *array_p = json_array ();

																					if (array_p)
																						{
																							if (json_object_set_new (root_p, SERVICES_NAME_S, array_p) == 0)
																								{
																									if (json_array_append_new (array_p, service_req_p) == 0)
																										{
																											return root_p;
																										}
																								}
																							else
																								{
																									json_decref (array_p);
																								}
																						}

																					json_decref (root_p);
																				}
																		}
																}
															else
																{
																	json_decref (params_json_p);
																}
														}
												}
											else
												{
													json_decref (parameter_set_json_p);
												}
										}

								}		/* if (SetJSONBoolean (service_req_p, SERVICE_RUN_S, true)) */

						}		/* if (SetJSONString (service_req_p, SERVICE_ALIAS_S, service_name_s)) */


				}		/* if (SetJSONString (service_req_p, SERVICE_NAME_S, service_name_s)) */

			json_decref (service_req_p);
		}		/* if (service_req_p) */

	return NULL;
}


static json_t *GetOperationRequestFromURI (const char *operation_s, apr_table_t *params_table_p)
{
	json_t *json_req_p = NULL;
	SchemaVersion *sv_p = AllocateSchemaVersion (CURRENT_SCHEMA_VERSION_MAJOR, CURRENT_SCHEMA_VERSION_MINOR);

	if (sv_p)
		{
			Operation op;

			op = GetOperationFromString (operation_s);

			switch (op)
				{
					case OP_LIST_ALL_SERVICES:
						json_req_p = GetOperationAsJSON (op, sv_p);
						break;

					case OP_GET_SERVICE_INFO:
						{
							const char *service_name_s = apr_table_get (params_table_p, SERVICE_NAME_S);

							if (service_name_s)
								{
									json_t *service_names_p = json_array ();

									if (service_names_p)
										{
											json_t *service_name_json_p = json_string (service_name_s);

											if (service_name_json_p)
												{
													if (json_array_append_new (service_names_p, service_name_json_p) == 0)
														{
															json_req_p = GetServicesRequest (NULL, op, SERVICES_NAME_S, service_names_p, sv_p);

															if (json_req_p)
																{

																}
														}
													else
														{
															json_decref (service_name_json_p);
														}
												}

											if (!json_req_p)
												{
													json_decref (service_names_p);
												}
										}


								}

						}
						break;

					case OP_NONE:
						break;

					default:
						break;
				}

			FreeSchemaVersion (sv_p);
		}
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "AllocateSchemaVersion failed");
		}

	return json_req_p;
}

