/*
 * bzip2_util.h
 *
 *  Created on: 8 Jun 2017
 *      Author: billy
 */

#ifndef BZIP2_UTIL_H
#define BZIP2_UTIL_H

#include "typedefs.h"


unsigned char *CompressToBZ2 (unsigned char *src_s, const unsigned int src_length, unsigned int *dest_length_p);


unsigned char *UncompressFromBZ2 (unsigned char *src_s, const unsigned int src_length, unsigned int *dest_length_p);


#endif		/* #ifndef BZIP2_UTIL_H */
