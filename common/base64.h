/**
* Copyright (C) 2008 Happy Fish / YuQing
*
* FastDFS may be copied only under the terms of the GNU General
* Public License V3, which may be found in the FastDFS source kit.
* Please visit the FastDFS Home Page http://www.csource.org/ for more detail.
**/

//base64.h

#ifndef _BASE64_H
#define _BASE64_H

#include "common_define.h"

struct base64_context
{
	char line_separator[16];
	int line_sep_len;

	/**
	 * max chars per line, excluding line_separator.  A multiple of 4.
	 */
	int line_length;

	/**
	 * letter of the alphabet used to encode binary values 0..63
	 */
	unsigned char valueToChar[64];

	/**
	 * binary value encoded by a given letter of the alphabet 0..63
	 */
	int charToValue[256];
	int pad_ch;
};

#ifdef __cplusplus
extern "C" {
#endif

/** use stardand base64 charset
 */
#define base64_init(context, nLineLength) \
        base64_init_ex(context, nLineLength, '+', '/', '=')


/** stardand base64 encode
 */
#define base64_encode(context, src, nSrcLen, dest, dest_len) \
        base64_encode_ex(context, src, nSrcLen, dest, dest_len, true)

/** base64 init function, before use base64 function, you should call 
 *  this function
 *  parameters:
 *           context:     the base64 context
 *           nLineLength: length of a line, 0 for never add line seperator
 *           chPlus:      specify a char for base64 char plus (+)
 *           chSplash:    specify a char for base64 char splash (/)
 *           chPad:       specify a char for base64 padding char =
 *  return: none
 */
void base64_init_ex(struct base64_context *context, const int nLineLength, \
		const unsigned char chPlus, const unsigned char chSplash, \
		const unsigned char chPad);

/** calculate the encoded length of the source buffer
 *  parameters:
 *           context:     the base64 context
 *           nSrcLen:     the source buffer length
 *  return: the encoded length of the source buffer
 */
int base64_get_encode_length(struct base64_context *context, const int nSrcLen);

/** base64 encode buffer
 *  parameters:
 *           context:   the base64 context
 *           src:       the source buffer
 *           nSrcLen:   the source buffer length
 *           dest:      the dest buffer
 *           dest_len:  return dest buffer length
 *           bPad:      if padding
 *  return: the encoded buffer
 */
char *base64_encode_ex(struct base64_context *context, const char *src, \
		const int nSrcLen, char *dest, int *dest_len, const bool bPad);

/** base64 decode buffer, work only with padding source string
 *  parameters:
 *           context:   the base64 context
 *           src:       the source buffer with padding
 *           nSrcLen:   the source buffer length
 *           dest:      the dest buffer
 *           dest_len:  return dest buffer length
 *  return: the decoded buffer
 */
char *base64_decode(struct base64_context *context, const char *src, \
		const int nSrcLen, char *dest, int *dest_len);

/** base64 decode buffer, can work with no padding source string
 *  parameters:
 *           context:   the base64 context
 *           src:       the source buffer with padding or no padding
 *           nSrcLen:   the source buffer length
 *           dest:      the dest buffer
 *           dest_len:  return dest buffer length
 *  return: the decoded buffer
 */
char *base64_decode_auto(struct base64_context *context, const char *src, \
		const int nSrcLen, char *dest, int *dest_len);

/** set line separator string, such as \n or \r\n
 *  parameters:
 *           context:   the base64 context
 *           pLineSeparator: the line separator string
 *  return: none
 */
void base64_set_line_separator(struct base64_context *context, \
		const char *pLineSeparator);

/** set line length, 0 for never add line separators
 *  parameters:
 *           context:   the base64 context
 *           length:    the line length
 *  return: none
 */
void base64_set_line_length(struct base64_context *context, const int length);

#ifdef __cplusplus
}
#endif

#endif

