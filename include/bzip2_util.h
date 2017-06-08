/*
 * bzip2_util.h
 *
 *  Created on: 8 Jun 2017
 *      Author: billy
 */


unsigned char *CompressToBZ2 (char *src_s, const unsigned int src_length, unsigned int *dest_length_p);


unsigned char *UncompressFromBZ2 (char *src_s, const unsigned int src_length, unsigned int *dest_length_p);
