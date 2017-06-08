/*
 * bzip2_util.c
 *
 *  Created on: 8 Jun 2017
 *      Author: billy
 */


#include "bzlib.h"

#include "memory_allocations.h"


unsigned char *CompressAsBZ2 (char *src_s, const unsigned int src_length, unsigned int *dest_length_p)
{
	char *dest_p = (char *) AllocMemory (src_length * sizeof (char));

	if (dest_p)
		{
			const int block_size_100k = 9;
			const int verbosity = 0;
			const int work_factor = 0;

			int res = BZ2_bzBuffToBuffCompress (dest_p, dest_length_p, src_s, src_length, block_size_100k, verbosity, work_factor);

			if (res == BZ_OK)
				{

				}

		}		/* if (dest_p) */

	return ((unsigned char *) dest_p);
}
