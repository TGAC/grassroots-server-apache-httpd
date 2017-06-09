/*
 * bzip2_util.c
 *
 *  Created on: 8 Jun 2017
 *      Author: billy
 */

#include <string.h>
#include <stdio.h>

#include "bzlib.h"

#include "bzip2_util.h"
#include "memory_allocations.h"
#include "streams.h"
#include "string_utils.h"
#include "filesystem_utils.h"


#ifdef _DEBUG
#define BZIP2_UTIL_DEBUG	(STM_LEVEL_FINEST)
#else
#define BZIP2_UTIL_DEBUG	(STM_LEVEL_NONE)
#endif


static bool SaveBZ2Data (const char *data_p, const unsigned int data_length, const char *key_s);


/*
 * These routines compress and decompress data to bzip2 format.
 *
 * The actual compressed data begins with a uint64 value which is the length of
 * the uncompressed data followed by the actual data.
 */

unsigned char *CompressToBZ2 (unsigned char *src_s, const unsigned int src_length, unsigned int *dest_length_p, const char * const key_s)
{
	unsigned int temp_dest_length = src_length * sizeof (char);
	char *dest_p = (char *) AllocMemory (temp_dest_length + sizeof (unsigned int));

	if (dest_p)
		{
			const int block_size_100k = 9;
			const int verbosity = 4;
			const int work_factor = 0;

			#if BZIP2_UTIL_DEBUG >= STM_LEVEL_FINEST
				{
					char *filename_s = ConcatenateStrings (key_s, ".json");

					if (filename_s)
						{
							SaveBZ2Data ((char *) src_s, src_length, filename_s);

							FreeCopiedString (filename_s);
						}
				}
			#endif


			int res = BZ2_bzBuffToBuffCompress (dest_p + sizeof (unsigned int), &temp_dest_length, (char *) src_s, src_length, block_size_100k, verbosity, work_factor);

			if (res == BZ_OK)
				{
					unsigned int *uncompressed_length_p = (unsigned int *) dest_p;

					*uncompressed_length_p = src_length;
					*dest_length_p = temp_dest_length;

					#if BZIP2_UTIL_DEBUG >= STM_LEVEL_FINER
						{
							char buffer_s [32];

							memset (buffer_s, 0, 32);

							snprintf (buffer_s,  src_length < 31 ? src_length : 31, "%s", src_s);

							PrintLog (STM_LEVEL_FINER, __FILE__, __LINE__, "Compressed \"%s ...\" " UINT32_FMT " bytes long to " UINT32_FMT, buffer_s, src_length, *dest_length_p);

						}
					#endif

					#if BZIP2_UTIL_DEBUG >= STM_LEVEL_FINEST
						{
							char *filename_s = ConcatenateStrings (key_s, ".bz2");

							if (filename_s)
								{
									SaveBZ2Data (dest_p + sizeof (unsigned int), temp_dest_length, filename_s);

									FreeCopiedString (filename_s);
								}
						}
					#endif


					return ((unsigned char *) dest_p);
				}
			else
				{
					switch (res)
						{
							case BZ_CONFIG_ERROR:
								PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to compress entry, the library has been mis-compiled");
								break;

							case BZ_PARAM_ERROR:
								PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to compress entry, parameter error, dest_p %8X, temp_dest_length " UINT32_FMT " block_size_100k %d verbosity %d work_factor %d", dest_p, temp_dest_length, block_size_100k, verbosity, work_factor);;
								break;

							case BZ_MEM_ERROR:
								PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to compress entry, insufficient memory is available");
								break;

							case BZ_OUTBUFF_FULL:
								PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to compress entry, the size of the compressed data exceeds " UINT32_FMT, temp_dest_length);
								break;

							default:
								PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to compress entry, unknown error %d", res);
								break;
						}
				}

		}		/* if (dest_p) */
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate memory to compress entry \"%s\"", src_s);
		}

	return NULL;
}



unsigned char *UncompressFromBZ2 (unsigned char *src_p, const unsigned int src_length, unsigned int *dest_length_p, const char * const key_s)
{
	unsigned int *uncompressed_length_p = (unsigned int *) src_p;
	unsigned int uncompressed_length = *uncompressed_length_p;
	char *dest_p = (char *) AllocMemory (uncompressed_length * sizeof (char));

	#if BZIP2_UTIL_DEBUG >= STM_LEVEL_FINER
	PrintLog (STM_LEVEL_FINER, __FILE__, __LINE__, "Uncompressing from " UINT32_FMT " bytes long to " UINT32_FMT, src_length, *uncompressed_length_p);
	#endif

	if (dest_p)
		{
			const int small = 0;
			const int verbosity = 4;

			/* move past the uncompressed length value */
			src_p += sizeof (unsigned int);

			int res = BZ2_bzBuffToBuffDecompress (dest_p, &uncompressed_length, (char *) src_p, src_length, small, verbosity);

			if (res == BZ_OK)
				{
					*dest_length_p = uncompressed_length;

					#if BZIP2_UTIL_DEBUG >= STM_LEVEL_FINER
					PrintLog (STM_LEVEL_FINER, __FILE__, __LINE__, "Uncompressed from " UINT32_FMT " bytes long to " UINT32_FMT, src_length, uncompressed_length);
					#endif

					return ((unsigned char *) dest_p);
				}
			else
				{
					switch (res)
						{
							case BZ_CONFIG_ERROR:
								PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to decompress entry, the library has been mis-compiled");
								break;

							case BZ_PARAM_ERROR:
								PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to decompress entry, parameter error, dest_p %8X, temp_dest_length " UINT32_FMT " small %d verbosity %d", dest_p, uncompressed_length, small, verbosity);
								break;

							case BZ_MEM_ERROR:
								PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to decompress entry, insufficient memory is available");
								break;

							case BZ_DATA_ERROR_MAGIC:
								PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to decompress entry, the compressed data doesn't begin with the right magic bytes %8X", * ((uint32 *) src_p));
								break;

							case BZ_DATA_ERROR:
								PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to decompress entry, a data integrity error was detected in the compressed datas %8X", * ((uint32 *) src_p));
								break;

							case BZ_UNEXPECTED_EOF:
								PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to decompress entry, the compressed data ends unexpectedly");
								break;

							default:
								PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to decompress entry, unknown error %d", res);
								break;
						}
				}

		}		/* if (dest_p) */
	else
		{
			PrintErrors (STM_LEVEL_SEVERE, __FILE__, __LINE__, "Failed to allocate memory to decompress entry");
		}

	return NULL;
}



static bool SaveBZ2Data (const char *data_p, const unsigned int data_length, const char *key_s)
{
	bool success_flag = false;
	char *filename_s = MakeFilename ("/home/billy", key_s);

	if (filename_s)
		{
			FILE *out_f = fopen (filename_s, "w");

			if (out_f)
				{
					size_t res = fwrite (data_p, sizeof (char), data_length, out_f);

					if (res != data_length)
						{

						}

					fclose (out_f);
				}

			FreeCopiedString (filename_s);
		}		/* if (filename_s) */

	return success_flag;
}



