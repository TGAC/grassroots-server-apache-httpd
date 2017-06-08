/*
 * bzip2_util.c
 *
 *  Created on: 8 Jun 2017
 *      Author: billy
 */


#include "bzlib.h"

#include "memory_allocations.h"


/*
 * These routines compress and decompress data to bzip2 format.
 *
 * The actual compressed data begins with a uint64 value which is the length of
 * the uncompressed data followed by the actual data.
 */

unsigned char *CompressToBZ2 (char *src_s, const unsigned int src_length, unsigned int *dest_length_p)
{
	char *dest_p = (char *) AllocMemory ((src_length * sizeof (char)) + sizeof (uint64));

	if (dest_p)
		{
			const int block_size_100k = 9;
			const int verbosity = 0;
			const int work_factor = 0;

			int res = BZ2_bzBuffToBuffCompress (dest_p + sizeof (uint64), dest_length_p, src_s, src_length, block_size_100k, verbosity, work_factor);

			if (res == BZ_OK)
				{
					uint64 *temp_p = (uint64 *) dest_p;
					*temp_p = *dest_length_p;

					return ((unsigned char *) dest_p);
				}

		}		/* if (dest_p) */

	return NULL;
}



unsigned char *UncompressFromBZ2 (char *src_p, const unsigned int src_length, unsigned int *dest_length_p)
{
	uint64 *temp_p = (uint64 *) src_p;
	char *dest_p = (char *) AllocMemory (*temp_p * sizeof (char));

	if (dest_p)
		{
			const int small = 9;
			const int verbosity = 0;

			int res = BZ2_bzBuffToBuffDecompress (dest_p, dest_length_p, src_p, src_length, small, verbosity);

			if (res == BZ_OK)
				{
					return ((unsigned char *) dest_p);
				}

		}		/* if (dest_p) */

	return NULL;
}
