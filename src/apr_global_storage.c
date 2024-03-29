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
 * apr_global_storage.c
 *
 *  Created on: 16 Jun 2015
 *      Author: tyrrells
 */

#include "apr_global_storage.h"

#include "memory_allocations.h"
#include "apr_thread_mutex.h"
#include "typedefs.h"
#include "streams.h"
#include "string_utils.h"
#include "util_mutex.h"
#include "filesystem_utils.h"

#include "uuid_util.h"

#ifdef _DEBUG
#define APR_GLOBAL_STORAGE_DEBUG	(STM_LEVEL_FINEST)
#else
#define APR_GLOBAL_STORAGE_DEBUG	(STM_LEVEL_NONE)
#endif

static void *FindObjectFromAPRGlobalStorage (APRGlobalStorage *storage_p, const void *raw_key_p, unsigned int raw_key_length, const bool remove_flag);

static char *GetKeyAsValidString (char *raw_key_p, unsigned int key_length, bool *alloc_key_flag);

static apr_status_t IterateOverSOCache (ap_socache_instance_t *instance,
    server_rec *s,
    void *userctx,
    const unsigned char *id,
    unsigned int idlen,
    const unsigned char *data,
    unsigned int datalen,
    apr_pool_t *pool);


static bool SetLargestEntrySize (APRGlobalStorage *storage_p, const unsigned int size);

static bool GetLargestEntrySize (APRGlobalStorage *storage_p, unsigned int *size_p);


/***************************************************/


APRGlobalStorage *AllocateAPRGlobalStorage (apr_pool_t *pool_p, apr_hashfunc_t hash_fn, unsigned char *(*make_key_fn) (const void *data_p, uint32 raw_key_length, uint32 *key_len_p), void (*free_key_and_value_fn) (unsigned char *key_p, void *value_p), server_rec *server_p, const char *mutex_filename_s, const char *cache_id_s, const char *provider_name_s,
	unsigned char *(*compress_fn) (unsigned char *src_s, unsigned int src_length, unsigned int *dest_length_p, const char * const key_s),
	unsigned char *(*decompress_fn) (unsigned char *src_s, unsigned int src_length, unsigned int *dest_length_p, const char * const key_s))
{
	APRGlobalStorage *store_p = (APRGlobalStorage *) AllocMemory (sizeof (APRGlobalStorage));

	if (store_p)
		{
			memset (store_p, 0, sizeof (APRGlobalStorage));

			if (InitAPRGlobalStorage (store_p, pool_p, hash_fn, make_key_fn, free_key_and_value_fn, server_p, mutex_filename_s, cache_id_s, provider_name_s, compress_fn, decompress_fn))
				{
					return store_p;
				}

			FreeAPRGlobalStorage (store_p);
		}
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate APRGlobalStorage");
		}

	return NULL;
}


bool InitAPRGlobalStorage (APRGlobalStorage *storage_p, apr_pool_t *pool_p, apr_hashfunc_t hash_fn, unsigned char *(*make_key_fn) (const void *data_p, uint32 raw_key_length, uint32 *key_len_p), void (*free_key_and_value_fn) (unsigned char *key_p, void *value_p), server_rec *server_p, const char *mutex_filename_s, const char *cache_id_s, const char *provider_name_s,
	unsigned char *(*compress_fn) (unsigned char *src_s, unsigned int src_length, unsigned int *dest_length_p, const char * const key_s),
	unsigned char *(*decompress_fn) (unsigned char *src_s, unsigned int src_length, unsigned int *dest_length_p, const char * const key_s))
{
	ap_socache_provider_t *provider_p = ap_lookup_provider (AP_SOCACHE_PROVIDER_GROUP, provider_name_s, AP_SOCACHE_PROVIDER_VERSION);

	if (provider_p)
		{
			apr_status_t status = apr_global_mutex_create (& (storage_p -> ags_mutex_p), mutex_filename_s, APR_THREAD_MUTEX_UNNESTED, pool_p);

			if (status == APR_SUCCESS)
				{
					char *current_dir_s = GetCurrentWorkingDirectory ();

					if (current_dir_s)
						{
							storage_p ->ags_mapped_mem_p = AllocateSharedMemory (current_dir_s, sizeof (unsigned int), 0666);

							FreeCopiedString (current_dir_s);

							if (storage_p ->ags_mapped_mem_p)
								{
									storage_p -> ags_entries_p = apr_hash_make_custom (pool_p, hash_fn);

										if (storage_p -> ags_entries_p)
											{
												storage_p -> ags_pool_p = pool_p;
												storage_p -> ags_server_p = server_p;
												storage_p -> ags_make_key_fn = make_key_fn;
												storage_p -> ags_free_key_and_value_fn = free_key_and_value_fn;

												storage_p -> ags_cache_id_s = cache_id_s;
												storage_p -> ags_mutex_lock_filename_s = mutex_filename_s;

												storage_p -> ags_socache_instance_p = NULL;
												storage_p -> ags_socache_provider_p = provider_p;

												storage_p -> ags_compress_fn = compress_fn;
												storage_p -> ags_decompress_fn = decompress_fn;

												apr_pool_cleanup_register (pool_p, storage_p, (const void *) FreeAPRGlobalStorage, apr_pool_cleanup_null);

												return true;
											}		/* if (storage_p -> ags_entries_p) */
										else
											{
												PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate shared memory hash table");
											}

									FreeSharedMemory (storage_p -> ags_mapped_mem_p);
								}		/* if (storage_p -> ags_largest_entry_memory_id != -1) */
							else
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate shared memory for largest chunk size");
								}

						}		/* if (mem_key_s) */
					else
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to create memory key from \"%s\" and \".memory\"", cache_id_s);
						}

					apr_global_mutex_destroy (storage_p -> ags_mutex_p);
					storage_p -> ags_mutex_p = NULL;
				}		/* if (status == APR_SUCCESS) */
			else
				{
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to create global mutex for shared memory at %s", mutex_filename_s);
				}

		}		/* if (provider_p) */
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to find provider \"%s\"", provider_name_s ? provider_name_s : "NULL");
		}

	return false;
}


apr_status_t FreeAPRGlobalStorage (void *data_p)
{
	APRGlobalStorage *storage_p = (APRGlobalStorage *) data_p;
	DestroyAPRGlobalStorage (storage_p);
	FreeMemory (storage_p);

	return APR_SUCCESS;
}


void DestroyAPRGlobalStorage (APRGlobalStorage *storage_p)
{
	if (storage_p)
		{
			if (storage_p -> ags_entries_p)
				{
					unsigned char *key_s = NULL;
					apr_ssize_t keylen = 0;
					void *value_p = NULL;
					apr_hash_index_t *index_p =	apr_hash_first (storage_p -> ags_pool_p, storage_p -> ags_entries_p);

					while (index_p)
						{
							apr_hash_this (index_p, (const void **) &key_s, &keylen, (void **) &value_p);

							storage_p -> ags_free_key_and_value_fn (key_s, value_p);

							index_p = apr_hash_next (index_p);
						}

					apr_hash_clear (storage_p -> ags_entries_p);
				}

			storage_p -> ags_pool_p = NULL;

			if (storage_p -> ags_mutex_p)
				{
					apr_global_mutex_destroy (storage_p -> ags_mutex_p);
					storage_p -> ags_mutex_p = NULL;
				}

			if (storage_p -> ags_socache_instance_p)
				{
					storage_p -> ags_socache_provider_p -> destroy (storage_p -> ags_socache_instance_p, storage_p -> ags_server_p);
				}

			if (storage_p -> ags_mapped_mem_p)
				{
					FreeSharedMemory (storage_p -> ags_mapped_mem_p);
				}
		}
}


unsigned int HashUUIDForAPR (const char *key_s, apr_ssize_t *len_p)
{
	unsigned int res = 0;
	const uuid_t *id_p = (const uuid_t *) key_s;
	char *uuid_s = GetUUIDAsString (*id_p);

	if (uuid_s)
		{
			apr_ssize_t len = APR_HASH_KEY_STRING;
			res = apr_hashfunc_default (uuid_s, &len);

			#if APR_GLOBAL_STORAGE_DEBUG >= STM_LEVEL_FINER
			PrintLog (STM_LEVEL_FINER, __FILE__, __LINE__, "uuid \"%s\" res %u len %u", uuid_s, res, len);
			#endif

			FreeCopiedString (uuid_s);
		}

	return res;
}


unsigned char *MakeKeyFromUUID (const void *data_p, uint32 raw_key_length, uint32 *key_len_p)
{
	unsigned char *res_p = NULL;
	const uuid_t *id_p = (const uuid_t *) data_p;
	char *uuid_s = GetUUIDAsString (*id_p);

	if (uuid_s)
		{
			res_p = (unsigned char *) uuid_s;
			*key_len_p = UUID_STRING_BUFFER_SIZE - 1;
		}

	#if APR_GLOBAL_STORAGE_DEBUG >= STM_LEVEL_FINER
	PrintLog (STM_LEVEL_FINER, __FILE__, __LINE__, "uuid \"%s\" len %u res \"%s\"", uuid_s, *key_len_p, res_p);
	#endif


	return res_p;
}


void PrintAPRGlobalStorage (APRGlobalStorage *storage_p)
{
	PrintLog (STM_LEVEL_FINE, __FILE__, __LINE__, "Begin iterating storage");

	storage_p -> ags_socache_provider_p -> iterate (storage_p -> ags_socache_instance_p,
                                                  storage_p -> ags_server_p,
                                                  NULL,
                                                  IterateOverSOCache,
                                                  storage_p -> ags_pool_p);

	PrintLog (STM_LEVEL_FINE, __FILE__, __LINE__, "End iterating storage");
}


static apr_status_t IterateOverSOCache (ap_socache_instance_t *instance,
    server_rec *s,
    void *userctx,
    const unsigned char *id,
    unsigned int idlen,
    const unsigned char *data,
    unsigned int datalen,
    apr_pool_t *pool)
{
	apr_status_t ret = OK;

	char *copied_id_s = CopyToNewString ((const char *) id, idlen, false);

	if (copied_id_s)
		{
			char *copied_data_s = CopyToNewString ((const char *) data, datalen, false);

			if (copied_data_s)
				{
					PrintLog (STM_LEVEL_FINE, __FILE__, __LINE__, "id \"%s\" idlen %u data \"%s\" datalen %u  %X", copied_id_s, idlen, copied_data_s, datalen, data);

					FreeCopiedString (copied_data_s);
				}
			else
				{
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to copy data \"%s\"", (char *) data);
					ret = APR_EGENERAL;
				}

			FreeCopiedString (copied_id_s);
		}
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to copy id \"%s\"", (char *) id);
			ret = APR_EGENERAL;
		}

	return ret;
}


static bool SetLargestEntrySize (APRGlobalStorage *storage_p, const unsigned int size)
{
	bool success_flag = false;
	unsigned int *largest_size_p = (unsigned int *) OpenSharedMemory (storage_p -> ags_mapped_mem_p, 0);

	if (largest_size_p)
		{
			if (*largest_size_p < size)
				{
					*largest_size_p = size;
				}

			CloseSharedMemory (storage_p -> ags_mapped_mem_p, largest_size_p);
			success_flag = true;
		}
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to open shared memory for largest size entry");
		}

	return success_flag;
}



static bool GetLargestEntrySize (APRGlobalStorage *storage_p, unsigned int *size_p)
{
	bool success_flag = false;
	unsigned int *largest_size_p = (unsigned int *) OpenSharedMemory (storage_p -> ags_mapped_mem_p, 0);

	if (largest_size_p)
		{
			*size_p =	*largest_size_p;
			 
			CloseSharedMemory (storage_p -> ags_mapped_mem_p, largest_size_p);
			success_flag = true;
		}
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to open shared memory for largest size entry");
		}

	return success_flag;
}


bool AddObjectToAPRGlobalStorage (APRGlobalStorage *storage_p, const void *raw_key_p, unsigned int raw_key_length, unsigned char *value_p, unsigned int value_length)
{
	bool success_flag = false;
	unsigned int key_len = 0;
	unsigned char *key_p = NULL;

	if (storage_p -> ags_make_key_fn)
		{
			key_p = storage_p -> ags_make_key_fn (raw_key_p, raw_key_length, &key_len);
		}
	else
		{
			key_p = (unsigned char *) raw_key_p;
			key_len = raw_key_length;
		}

	if (key_p)
		{
			bool alloc_key_flag = false;
			char *key_s = GetKeyAsValidString ((char *) key_p, key_len, &alloc_key_flag);
			apr_status_t status = apr_global_mutex_lock (storage_p -> ags_mutex_p);

			if (status == APR_SUCCESS)
				{
					apr_time_t end_of_time = APR_INT64_MAX;

					/* store it */
					if (storage_p -> ags_compress_fn)
						{
							unsigned int compressed_data_length = 0;
							unsigned char *compressed_data_p = storage_p -> ags_compress_fn (value_p, value_length, &compressed_data_length, key_s);

							if (compressed_data_p)
								{
									status = storage_p -> ags_socache_provider_p -> store (storage_p -> ags_socache_instance_p,
																							 storage_p -> ags_server_p,
																								key_p,
																								key_len,
																								end_of_time,
																								compressed_data_p,
																								compressed_data_length,
																								storage_p -> ags_pool_p);

									FreeMemory (compressed_data_p);
								}
							else
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to compress data, unable to store");
									status = APR_ENOMEM;
								}
						}
					else
						{
							status = storage_p -> ags_socache_provider_p -> store (storage_p -> ags_socache_instance_p,
																					 storage_p -> ags_server_p,
																						key_p,
																						key_len,
																						end_of_time,
																						value_p,
																						value_length,
																						storage_p -> ags_pool_p);
						}


					if (status == APR_SUCCESS)
						{
							success_flag = true;

							if (!SetLargestEntrySize (storage_p, value_length))
								{
									PrintErrors (STM_LEVEL_FINE, __FILE__, __LINE__, "Failed to possibly set largest entry size to %u", value_length);
								}

							#if APR_GLOBAL_STORAGE_DEBUG >= STM_LEVEL_FINE
							PrintLog (STM_LEVEL_FINE, __FILE__, __LINE__, "Added \"%s\" length %u, value %.16X length %u to global store", key_s, key_len, value_p, value_length);
							#endif

						}
					else
						{
							PrintErrors (STM_LEVEL_FINE, __FILE__, __LINE__, "Failed to add \"%s\" length %u, value %.16X length %u to global store", key_s, key_len, value_p, value_length);
						}

					status = apr_global_mutex_unlock (storage_p -> ags_mutex_p);

					if (status != APR_SUCCESS)
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to unlock mutex, status %s after adding %s", status, key_s);
						} /* if (status != APR_SUCCESS) */


				}		/* if (status == APR_SUCCESS) */
			else
				{
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to lock mutex, status %s to add %s", status, key_s);
				}

			if (alloc_key_flag)
				{
					FreeCopiedString (key_s);
				}

			/*
			 * If the key_p isn't pointing to the same address
			 * as raw_key_p it must be new, so delete it.
			 */
			if (key_p != raw_key_p)
				{
					FreeMemory (key_p);
				}
		} /* if (key_p) */
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to make key");
		}

	#if APR_GLOBAL_STORAGE_DEBUG >= STM_LEVEL_FINEST
	PrintAPRGlobalStorage (storage_p);
	#endif

	return success_flag;
}



void *GetObjectFromAPRGlobalStorage (APRGlobalStorage *storage_p, const void *raw_key_p, unsigned int raw_key_length)
{
	return FindObjectFromAPRGlobalStorage (storage_p, raw_key_p, raw_key_length, false);
}


void *RemoveObjectFromAPRGlobalStorage (APRGlobalStorage *storage_p, const void *raw_key_p, unsigned int raw_key_length)
{
	return FindObjectFromAPRGlobalStorage (storage_p, raw_key_p, raw_key_length, true);
}



static char *GetKeyAsValidString (char *raw_key_p, unsigned int key_length, bool *alloc_key_flag_p)
{
	char *key_s = NULL;

	if (* (raw_key_p + key_length) == '\0')
		{
			key_s = (char *) raw_key_p;
		}
	else
		{
			key_s = CopyToNewString ((const char * const) raw_key_p, key_length, false);

			if (key_s)
				{
					*alloc_key_flag_p = true;
				}
			else
				{
					key_s = "DUMMY KEY";
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to make key");
				}
		}

	#if APR_GLOBAL_STORAGE_DEBUG >= STM_LEVEL_FINER
	PrintLog (STM_LEVEL_FINER, __FILE__, __LINE__, "Added \"%s\" from raw key of length %u", key_s, key_length);
	#endif

	return key_s;
}


static void *FindObjectFromAPRGlobalStorage (APRGlobalStorage *storage_p, const void *raw_key_p, unsigned int raw_key_length, const bool remove_flag)
{
	void *result_p = NULL;
	unsigned int key_len = 0;
	unsigned char *key_p = NULL;

	if (storage_p -> ags_make_key_fn)
		{
			key_p = storage_p -> ags_make_key_fn (raw_key_p, raw_key_length, &key_len);
		}
	else
		{
			key_p = (unsigned char *) raw_key_p;
			key_len = raw_key_length;
		}

	if (key_p)
		{
			apr_status_t status;
			bool alloc_key_flag = false;
			char *key_s = GetKeyAsValidString ((char *) key_p, key_len, &alloc_key_flag);

			#if APR_GLOBAL_STORAGE_DEBUG >= STM_LEVEL_FINEST
			PrintLog (STM_LEVEL_FINEST,  __FILE__, __LINE__,"Made key: %s", key_s);
			#endif


			status = apr_global_mutex_lock (storage_p -> ags_mutex_p);

			if (status == APR_SUCCESS)
				{
					unsigned char *temp_p = NULL;
					unsigned int array_size = 0;

					#if APR_GLOBAL_STORAGE_DEBUG >= STM_LEVEL_FINEST
					PrintLog (STM_LEVEL_FINEST,  __FILE__, __LINE__,"Locked mutex");
					#endif

					if (GetLargestEntrySize (storage_p, &array_size))
						{
							/* We don't know how big the value might be so allocate the largest value that we've seen so far */
							temp_p = (unsigned char *) AllocMemoryArray (array_size, sizeof (unsigned char));

							if (temp_p)
								{
									/* get the value */
									status = storage_p -> ags_socache_provider_p -> retrieve (storage_p -> ags_socache_instance_p,
																							 storage_p -> ags_server_p,
									                              key_p,
									                              key_len,
									                              temp_p,
									                              &array_size,
									                              storage_p -> ags_pool_p);


									#if APR_GLOBAL_STORAGE_DEBUG >= STM_LEVEL_FINEST
									PrintLog (STM_LEVEL_FINEST,  __FILE__, __LINE__,"status %d key %s length %u result_p %0.16X remove_flag %d", status, key_s, key_len, temp_p, remove_flag);
									#endif

									if (status == APR_SUCCESS)
										{
											if (storage_p -> ags_decompress_fn)
												{
													unsigned int uncompressed_length = 0;
													unsigned char *uncompressed_data_p = storage_p -> ags_decompress_fn (temp_p, array_size, &uncompressed_length, key_s);

													if (uncompressed_data_p)
														{
															result_p = uncompressed_data_p;
														}
													else
														{
															PrintErrors (STM_LEVEL_SEVERE,  __FILE__, __LINE__,"Failed to uncompress data for \"%s\"", key_s);
														}

													FreeMemory (temp_p);
													temp_p = NULL;

												}
											else
												{
													result_p = temp_p;
												}

											if (remove_flag == true)
												{
													status = storage_p -> ags_socache_provider_p -> remove (storage_p -> ags_socache_instance_p,
													                                                        storage_p -> ags_server_p,
													                                                        key_p,
													                                                        key_len,
													                                                        storage_p -> ags_pool_p);


													#if APR_GLOBAL_STORAGE_DEBUG >= STM_LEVEL_FINEST
													PrintLog (STM_LEVEL_FINEST,  __FILE__, __LINE__,"status after removal %d", status);
													#endif
												}
										}
									else
										{
											PrintErrors (STM_LEVEL_SEVERE,  __FILE__, __LINE__,"Failed to allocate temporary memory when looking up key \"%s\"", key_s);
											FreeMemory (temp_p);
										}

								}		/* if (temp_p) */
							else
								{
									PrintErrors (STM_LEVEL_SEVERE,  __FILE__, __LINE__,"status %d key %s result_p %0.16X remove_flag %d", status, key_s, temp_p, remove_flag);
									FreeMemory (temp_p);
								}

						}		/* if (GetLargestEntrySize (storage_p, &array_size)) */
					else
						{
							PrintErrors (STM_LEVEL_SEVERE,  __FILE__, __LINE__,"Failed to get largest entry size when adding \"%s\"", key_s);
						}

				}		/* if (status == APR_SUCCESS) */
			else
				{
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to lock mutex for %s, status %s", key_s, status);
				}


			status = apr_global_mutex_unlock (storage_p -> ags_mutex_p);

			if (status != APR_SUCCESS)
				{
					PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to unlock mutex for %s, status %s after finding %s", key_s, status);
				} /* if (status != APR_SUCCESS) */

			if (key_p != raw_key_p)
				{
					FreeMemory (key_p);
				}

			if (alloc_key_flag)
				{
					FreeCopiedString (key_s);
				}
		} /* if (key_p) */

	#if APR_GLOBAL_STORAGE_DEBUG >= STM_LEVEL_FINEST
	PrintAPRGlobalStorage (storage_p);
	#endif


	return result_p;
}


bool IterateOverAPRGlobalStorage (APRGlobalStorage *storage_p, ap_socache_iterator_t *iterator_p, void *data_p)
{
	bool did_all_elements_flag = true;
	apr_status_t status = apr_global_mutex_lock (storage_p -> ags_mutex_p);

	if (status == APR_SUCCESS)
		{
			status = storage_p -> ags_socache_provider_p -> iterate (storage_p -> ags_socache_instance_p, storage_p -> ags_server_p, data_p, iterator_p, storage_p -> ags_pool_p);

			if (status != APR_SUCCESS)
				{
					did_all_elements_flag = false;
				}

			status = apr_global_mutex_unlock (storage_p -> ags_mutex_p);
		}		/* if (status == APR_SUCCESS) */


	return did_all_elements_flag;
}



bool PreConfigureGlobalStorage (APRGlobalStorage *storage_p, apr_pool_t *config_pool_p)
{
	bool success_flag = false;
	apr_status_t status = ap_mutex_register (config_pool_p, storage_p -> ags_cache_id_s, NULL, APR_LOCK_DEFAULT, 0);

	if (status == APR_SUCCESS)
		{
			storage_p -> ags_socache_provider_p = ap_lookup_provider (AP_SOCACHE_PROVIDER_GROUP, AP_SOCACHE_DEFAULT_PROVIDER, AP_SOCACHE_PROVIDER_VERSION);
			success_flag = true;
		}
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "failed to register %s mutex", storage_p -> ags_cache_id_s);
		}

	return success_flag;
}


bool PostConfigureGlobalStorage  (APRGlobalStorage *storage_p, apr_pool_t *server_pool_p, server_rec *server_p, const char *provider_name_s, struct ap_socache_hints *cache_hints_p)
{
	apr_status_t res;
	bool success_flag = true;

	/*
	 * Have we got a valid set of config directives?
	 */
	if (storage_p -> ags_socache_provider_p)
		{
			/* We have socache_provider, but do not have socache_instance. This should
			 * happen only when using "default" socache_provider, so create default
			 * socache_instance in this case. */
			if (! (storage_p -> ags_socache_instance_p))
				{
					const char *err_msg_s = storage_p -> ags_socache_provider_p -> create (& (storage_p -> ags_socache_instance_p), NULL, server_pool_p, server_pool_p);

					if (err_msg_s)
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "failed to create mod_socache_shmcb socache instance: %s", err_msg_s);
							success_flag = false;
						}
				}

			if (success_flag)
				{
					res = ap_global_mutex_create (& (storage_p -> ags_mutex_p), NULL, storage_p -> ags_cache_id_s, NULL, server_p, server_pool_p, 0);

					if (res == APR_SUCCESS)
						{
							res = storage_p -> ags_socache_provider_p -> init (storage_p -> ags_socache_instance_p, storage_p -> ags_cache_id_s, cache_hints_p, server_p, server_pool_p);

							if (res != APR_SUCCESS)
								{
									PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to initialise %s cache", storage_p -> ags_cache_id_s);
									success_flag = false;
								}
						}
					else
						{
							PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "failed to create %s mutex", storage_p -> ags_cache_id_s);
							success_flag = false;
						}
				}
		}
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Please select a socache provider with AuthnCacheSOCache (no default found on this platform). Maybe you need to load mod_socache_shmcb or another socache module first");
			success_flag = false;
		}


	return success_flag;
}


bool InitAPRGlobalStorageForChild (APRGlobalStorage *storage_p, apr_pool_t *pool_p)
{
	bool success_flag = false;

	/* Now that we are in a child process, we have to reconnect
	 * to the global mutex and the shared segment. We also
	 * have to find out the base address of the segment, in case
	 * it moved to a new address. */
	apr_status_t res = apr_global_mutex_child_init (& (storage_p -> ags_mutex_p), storage_p -> ags_mutex_lock_filename_s, pool_p);

	if (res != APR_SUCCESS)
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to attach grassroots child to global mutex file '%s', res %d", storage_p -> ags_mutex_lock_filename_s, res);
		}
	else
		{
			success_flag = true;
		}

	return success_flag;
}
