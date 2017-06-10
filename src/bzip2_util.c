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

static void LogDataHead (char *data_p, unsigned int data_length, const char *prefix_s);

static char GetPrintableChar (const char c);


static int s_index = 0;

/*
 * These routines compress and decompress data to bzip2 format.
 *
 * The actual compressed data begins with a uint64 value which is the length of
 * the uncompressed data followed by the actual data.
 */

unsigned char *CompressToBZ2 (unsigned char *src_s, unsigned int src_length, unsigned int *dest_length_p, const char * const key_s)
{
	unsigned int temp_dest_length = src_length * sizeof (char);
	char *dest_p = (char *) AllocMemory (temp_dest_length + sizeof (unsigned int));

	if (dest_p)
		{
			const int block_size_100k = 9;
			const int verbosity = 4;
			const int work_factor = 0;
			int res;

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


		#if BZIP2_UTIL_DEBUG >= STM_LEVEL_FINER
		LogDataHead ((char *) src_s, src_length, "uncompressed src: ");
		#endif


			res = BZ2_bzBuffToBuffCompress (dest_p + sizeof (unsigned int), &temp_dest_length, (char *) src_s, src_length, block_size_100k, verbosity, work_factor);

			if (res == BZ_OK)
				{
					unsigned int *uncompressed_length_p = (unsigned int *) dest_p;


					*uncompressed_length_p = src_length;

					/* Add the length of the size chunk */
					*dest_length_p = temp_dest_length + sizeof (unsigned int);

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

					#if BZIP2_UTIL_DEBUG >= STM_LEVEL_FINER
					LogDataHead (dest_p, temp_dest_length, "compressed dest: ");
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



unsigned char *UncompressFromBZ2 (unsigned char *src_p, unsigned int src_length, unsigned int *dest_length_p, const char * const key_s)
{
	unsigned int *uncompressed_length_p = (unsigned int *) src_p;
	unsigned int uncompressed_length = *uncompressed_length_p;
	char *dest_p = (char *) AllocMemory (uncompressed_length * sizeof (char));

	#if BZIP2_UTIL_DEBUG >= STM_LEVEL_FINER
	PrintLog (STM_LEVEL_FINER, __FILE__, __LINE__, "Uncompressing from " UINT32_FMT " bytes long to " UINT32_FMT, src_length, *uncompressed_length_p);
	#endif


	#if BZIP2_UTIL_DEBUG >= STM_LEVEL_FINER
	LogDataHead ((char *) src_p, src_length, "compressed src: ");
	#endif

	if (dest_p)
		{
			const int small = 0;
			const int verbosity = 4;

			/* move past the uncompressed length value */
			src_p += sizeof (unsigned int);
			//src_length -= sizeof (unsigned int);

			#if BZIP2_UTIL_DEBUG >= STM_LEVEL_FINER
			LogDataHead ((char *) src_p, src_length, "shuffled compressed src: ");
			#endif


			int res = BZ2_bzBuffToBuffDecompress (dest_p, &uncompressed_length, (char *) src_p, src_length, small, verbosity);

			if (res == BZ_OK)
				{
					*dest_length_p = uncompressed_length;

					#if BZIP2_UTIL_DEBUG >= STM_LEVEL_FINER
					PrintLog (STM_LEVEL_FINER, __FILE__, __LINE__, "Uncompressed from " UINT32_FMT " bytes long to " UINT32_FMT, src_length, uncompressed_length);
					#endif

					#if BZIP2_UTIL_DEBUG >= STM_LEVEL_FINER
					LogDataHead (dest_p, uncompressed_length, "uncompressed dest: ");
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
	char buffer_s [4];

	snprintf (buffer_s, 3, "%d", s_index ++);

	char *filename_s = ConcatenateVarargsStrings (key_s, "-", buffer_s);

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



static void LogDataHead (char *data_p, unsigned int data_length, const char *prefix_s)
{
	unsigned int limit = data_length; //< 63 ? data_length : 63;
	unsigned int i;
	char text_buffer_s [5];

	PrintLog (STM_LEVEL_FINER, __FILE__, __LINE__, "data length: " UINT32_FMT " limit: " UINT32_FMT, data_length, limit);

	/* terminate the text buffer */
	* (text_buffer_s + 4) = '\0';

	for (i = 0; i <= limit; i += 4)
		{
			*text_buffer_s = GetPrintableChar (*data_p);
			* (text_buffer_s + 1) = GetPrintableChar (* (++ data_p));
			* (text_buffer_s + 2) = GetPrintableChar (* (++ data_p));
			* (text_buffer_s + 3) = GetPrintableChar (* (++ data_p));

			++ data_p;

			PrintLog (STM_LEVEL_FINER, __FILE__, __LINE__, "%s: %.5" UINT32_FMT_IDENT ": %08X: \"%s\"", prefix_s, i, * ((uint32 *) data_p), text_buffer_s);
		}
}


static char GetPrintableChar (const char c)
{
	if (isprint (c))
		{
			return c;
		}
	else
		{
			return ".";
		}
}


