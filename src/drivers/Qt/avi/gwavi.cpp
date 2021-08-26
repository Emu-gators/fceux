/*
 * Copyright (c) 2008-2011, Michael Kohn
 * Copyright (c) 2013, Robin Hahling
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the author nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This is the file containing gwavi library functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gwavi.h"

/**
 * This is the first function you should call when using gwavi library.
 * It allocates memory for a gwavi_t structure and returns it and takes care of
 * initializing the AVI header with the provided information.
 *
 * When you're done creating your AVI file, you should call gwavi_close()
 * function to free memory allocated for the gwavi_t structure and properly
 * close the output file.
 *
 * @param filename This is the name of the AVI file which will be generated by
 * this library.
 * @param width Width of a frame.
 * @param height Height of a frame.
 * @param fourcc FourCC representing the codec of the video encoded stream. a
 * FourCC is a sequence of four chars used to uniquely identify data formats.
 * For more information, you can visit www.fourcc.org.
 * @param fps Number of frames per second of your video. It needs to be > 0.
 * @param audio This parameter is optionnal. It is used for the audio track. If
 * you do not want to add an audio track to your AVI file, simply pass NULL for
 * this argument.
 *
 * @return Structure containing required information in order to create the AVI
 * file. If an error occured, NULL is returned.
 */

gwavi_t::gwavi_t(void)
{
	in = out = NULL;
	memset( &avi_header     , 0, sizeof(struct gwavi_header_t) );
	memset( &stream_header_v, 0, sizeof(struct gwavi_stream_header_t) );
	memset( &stream_format_v, 0, sizeof(struct gwavi_stream_format_v_t) );
	memset( &stream_header_a, 0, sizeof(struct gwavi_stream_header_t) );
	memset( &stream_format_a, 0, sizeof(struct gwavi_stream_format_a_t) );
	memset(  fourcc, 0, sizeof(fourcc) );
	marker = 0;
	offsets_ptr = 0;
	offsets_len = 0;
	offsets_start = 0;
	offsets = 0;
	offset_count = 0;
	bits_per_pixel = 24;
}

gwavi_t::~gwavi_t(void)
{
	if ( in != NULL )
	{
		fclose(in); in = NULL;
	}
	if ( out != NULL )
	{
		fclose(out); out = NULL;
	}

}

int
gwavi_t::openIn(const char *filename)
{
	if ((in = fopen(filename, "rb")) == NULL) 
	{
		perror("gwavi_open: failed to open file for reading");
		return -1;
	}
	return 0;
}


int
gwavi_t::open(const char *filename, unsigned int width, unsigned int height,
	   const char *fourcc, double fps, struct gwavi_audio_t *audio)
{
	int size = 0;
	unsigned int usec;

	memset( this->fourcc, 0, sizeof(this->fourcc) );
	strcpy( this->fourcc, fourcc );

	if (check_fourcc(fourcc) != 0)
	{
		(void)fprintf(stderr, "WARNING: given fourcc does not seem to "
			      "be valid: %s\n", fourcc);
	}

	if (fps < 1)
	{
		return -1;
	}
	if ((out = fopen(filename, "wb+")) == NULL) 
	{
		perror("gwavi_open: failed to open file for writing");
		return -1;
	}
	usec = (unsigned int)((1000000.0 / fps)+0.50);
	printf("FPS: %f  %u\n", fps, usec );

	/* set avi header */
	avi_header.time_delay= usec;
	avi_header.data_rate = width * height * 3 * (((unsigned int)fps)+1);
	avi_header.flags = 0x10;

	if (audio)
	{
		avi_header.data_streams = 2;
	}
	else
	{
		avi_header.data_streams = 1;
	}

	if ( strcmp( fourcc, "I420" ) == 0 )
	{  // I420   YUV 4:2:0
		bits_per_pixel = 12;
	}
	else if ( strcmp( fourcc, "X264" ) == 0 )
	{  // X264   H.264
		bits_per_pixel = 12;
	}
	else
	{	// Plain RGB24
		bits_per_pixel = 24;
	}
	size = (width * height * bits_per_pixel);

	if ( (size % 8) != 0 )
	{
		printf("Warning: Video Buffer Size not on an 8 bit boundary: %ux%u:%i\n", width, height, bits_per_pixel);
	}
	size = size / 8;

	/* this field gets updated when calling gwavi_close() */
	avi_header.number_of_frames = 0;
	avi_header.width = width;
	avi_header.height = height;
	avi_header.buffer_size = size;

	/* set stream header */
	(void)strcpy(stream_header_v.data_type, "vids");
	(void)memcpy(stream_header_v.codec, fourcc, 4);
	stream_header_v.time_scale = usec;
	stream_header_v.data_rate = 1000000;
	stream_header_v.buffer_size = size;
	stream_header_v.data_length = 0;

	/* set stream format */
	stream_format_v.header_size = 40;
	stream_format_v.width = width;
	stream_format_v.height = height;
	stream_format_v.num_planes = 1;
	stream_format_v.bits_per_pixel = bits_per_pixel;
	stream_format_v.compression_type =
		((unsigned int)fourcc[3] << 24) +
		((unsigned int)fourcc[2] << 16) +
		((unsigned int)fourcc[1] << 8) +
		((unsigned int)fourcc[0]);
	stream_format_v.image_size = size;
	stream_format_v.colors_used = 0;
	stream_format_v.colors_important = 0;

	stream_format_v.palette = 0;
	stream_format_v.palette_count = 0;

	if (audio) 
	{
		/* set stream header */
		memcpy(stream_header_a.data_type, "auds", 4);
		stream_header_a.codec[0] = 1;
		stream_header_a.codec[1] = 0;
		stream_header_a.codec[2] = 0;
		stream_header_a.codec[3] = 0;
		stream_header_a.time_scale = 1;
		stream_header_a.data_rate = audio->samples_per_second;
		stream_header_a.buffer_size =
			audio->channels * (audio->bits / 8) * audio->samples_per_second;
		/* when set to -1, drivers use default quality value */
		stream_header_a.audio_quality = -1;
		stream_header_a.sample_size =
			(audio->bits / 8) * audio->channels;

		/* set stream format */
		stream_format_a.format_type = 1;
		stream_format_a.channels = audio->channels;
		stream_format_a.sample_rate = audio->samples_per_second;
		stream_format_a.bytes_per_second =
			audio->channels * (audio->bits / 8) * audio->samples_per_second;
		stream_format_a.block_align =
			audio->channels * (audio->bits / 8);
		stream_format_a.bits_per_sample = audio->bits;
		stream_format_a.size = 0;
	}

	if (write_chars_bin(out, "RIFF", 4) == -1)
		goto write_chars_bin_failed;
	if (write_int(out, 0) == -1) {
		(void)fprintf(stderr, "gwavi_info: write_int() failed\n");
		return -1;
	}
	if (write_chars_bin(out, "AVI ", 4) == -1)
		goto write_chars_bin_failed;

	if (write_avi_header_chunk() == -1) {
		(void)fprintf(stderr, "gwavi_info: write_avi_header_chunk "
			      "failed\n");
		return -1;
	}

	if (write_chars_bin(out, "LIST", 4) == -1)
		goto write_chars_bin_failed;
	if ((marker = ftell(out)) == -1) {
		perror("gwavi_info (ftell)");
		return -1;
	}
	if (write_int(out, 0) == -1) {
		(void)fprintf(stderr, "gwavi_info: write_int() failed\n");
		return -1;
	}
	if (write_chars_bin(out, "movi", 4) == -1)
		goto write_chars_bin_failed;

	offsets_len = 1024;
	if ((offsets = (unsigned int *)malloc((size_t)offsets_len *
				      sizeof(unsigned int)))
			== NULL) {
		(void)fprintf(stderr, "gwavi_info: could not allocate memory "
			      "for gwavi offsets table\n");
		return -1;
	}

	offsets_ptr = 0;

	return 0;

write_chars_bin_failed:
	(void)fprintf(stderr, "gwavi_open: write_chars_bin() failed\n");
	return -1;
}

/**
 * This function allows you to add an encoded video frame to the AVI file.
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 * @param buffer Video buffer size.
 * @param len Video buffer length.
 *
 * @return 0 on success, -1 on error.
 */
int
gwavi_t::add_frame( unsigned char *buffer, size_t len)
{
	size_t maxi_pad;  /* if your frame is raggin, give it some paddin' */
	size_t t;

	if ( !buffer) {
		(void)fputs("gwavi and/or buffer argument cannot be NULL",
			    stderr);
		return -1;
	}
	//if (len < 256)
	//{
	//	(void)fprintf(stderr, "WARNING: specified buffer len seems "
	//		      "rather small: %d. Are you sure about this?\n",
	//		      (int)len);
	//}

	offset_count++;
	stream_header_v.data_length++;

	maxi_pad = len % 4;
	if (maxi_pad > 0)
		maxi_pad = 4 - maxi_pad;

	if (offset_count >= offsets_len)
	{
		void *tmpPtr;
		offsets_len += 1024;
		tmpPtr = realloc( offsets,
				(size_t)offsets_len *
					sizeof(unsigned int));

		if ( tmpPtr )
		{
			offsets = (unsigned int *)tmpPtr;
		}
		else
		{
			fprintf(stderr, "gwavi_add_frame: realloc() failed\n");
		}
	}

	offsets[offsets_ptr++] = (unsigned int)(len + maxi_pad);

	if (write_chars_bin(out, "00dc", 4) == -1) {
		(void)fprintf(stderr, "gwavi_add_frame: write_chars_bin() "
			      "failed\n");
		return -1;
	}
	if (write_int(out, (unsigned int)(len + maxi_pad)) == -1) {
		(void)fprintf(stderr, "gwavi_add_frame: write_int() failed\n");
		return -1;
	}

	if ((t = fwrite(buffer, 1, len, out)) != len) {
		(void)fprintf(stderr, "gwavi_add_frame: fwrite() failed\n");
		return -1;
	}

	for (t = 0; t < maxi_pad; t++)
	{
		if (fputc(0, out) == EOF) {
			(void)fprintf(stderr, "gwavi_add_frame: fputc() failed\n");
			return -1;
		}
	}

	return 0;
}

/**
 * This function allows you to add the audio track to your AVI file.
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 * @param buffer Audio buffer size.
 * @param len Audio buffer length.
 *
 * @return 0 on success, -1 on error.
 */
int
gwavi_t::add_audio( unsigned char *buffer, size_t len)
{
	size_t maxi_pad;  /* in case audio bleeds over the 4 byte boundary  */
	size_t t;

	if ( !buffer) {
		(void)fputs("gwavi and/or buffer argument cannot be NULL",
			    stderr);
		return -1;
	}

	offset_count++;

	maxi_pad = len % 4;
	if (maxi_pad > 0)
	{
		maxi_pad = 4 - maxi_pad;
	}

	if (offset_count >= offsets_len)
	{
		void *tmpPtr;
		offsets_len += 1024;
		tmpPtr = realloc( offsets,
				(size_t)offsets_len *
				sizeof(unsigned int));
		if ( tmpPtr )
		{
			offsets = (unsigned int *)tmpPtr;
		}
		else
		{
			fprintf(stderr, "gwavi_add_audio: realloc() failed\n");
		}
	}

	offsets[offsets_ptr++] =
		(unsigned int)((len + maxi_pad) | 0x80000000);

	if (write_chars_bin(out,"01wb",4) == -1) {
		(void)fprintf(stderr, "gwavi_add_audio: write_chars_bin() "
			      "failed\n");
		return -1;
	}
	if (write_int(out,(unsigned int)(len + maxi_pad)) == -1) {
		(void)fprintf(stderr, "gwavi_add_audio: write_int() failed\n");
		return -1;
	}

	if ((t = fwrite(buffer, 1, len, out)) != len ) {
		(void)fprintf(stderr, "gwavi_add_audio: fwrite() failed\n");
		return -1;
	}

	for (t = 0; t < maxi_pad; t++)
		if (fputc(0,out) == EOF) {
			(void)fprintf(stderr, "gwavi_add_audio: fputc() failed\n");
			return -1;
		}

	stream_header_a.data_length += (unsigned int)(len + maxi_pad);

	return 0;
}

/**
 * This function should be called when the program is done adding video and/or
 * audio frames to the AVI file. It frees memory allocated for gwavi_open() for
 * the main gwavi_t structure. It also properly closes the output file.
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 *
 * @return 0 on success, -1 on error.
 */
int
gwavi_t::close(void)
{
	long t;

	if ((t = ftell(out)) == -1)
		goto ftell_failed;
	if (fseek(out, marker, SEEK_SET) == -1)
		goto fseek_failed;
	if (write_int(out, (unsigned int)(t - marker - 4)) == -1) {
		(void)fprintf(stderr, "gwavi_close: write_int() failed\n");
		return -1;
	}
	if (fseek(out,t,SEEK_SET) == -1)
		goto fseek_failed;

	if (write_index(out, offset_count, offsets) == -1) {
		(void)fprintf(stderr, "gwavi_close: write_index() failed\n");
		return -1;
	}

	free(offsets);

	/* reset some avi header fields */
	avi_header.number_of_frames = stream_header_v.data_length;

	if ((t = ftell(out)) == -1)
		goto ftell_failed;
	if (fseek(out, 12, SEEK_SET) == -1)
		goto fseek_failed;
	if (write_avi_header_chunk() == -1) {
		(void)fprintf(stderr, "gwavi_close: write_avi_header_chunk() "
			      "failed\n");
		return -1;
	}
	if (fseek(out, t, SEEK_SET) == -1)
		goto fseek_failed;

	if ((t = ftell(out)) == -1)
		goto ftell_failed;
	if (fseek(out, 4, SEEK_SET) == -1)
		goto fseek_failed;
	if (write_int(out, (unsigned int)(t - 8)) == -1) {
		(void)fprintf(stderr, "gwavi_close: write_int() failed\n");
		return -1;
	}
	if (fseek(out, t, SEEK_SET) == -1)
		goto fseek_failed;

	if (stream_format_v.palette != 0)
		free(stream_format_v.palette);

	if (fclose(out) == EOF) {
		perror("gwavi_close (fclose)");
		return -1;
	}
	out = NULL;

	return 0;

ftell_failed:
	perror("gwavi_close: (ftell)");
	return -1;

fseek_failed:
	perror("gwavi_close (fseek)");
	return -1;
}

/**
 * This function allows you to reset the framerate. In a standard use case, you
 * should not need to call it. However, if you need to, you can call it to reset
 * the framerate after you are done adding frames to your AVI file and before
 * you call gwavi_close().
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 * @param fps Number of frames per second of your video.
 *
 * @return 0 on success, -1 on error.
 */
int
gwavi_t::set_framerate(double fps)
{
	unsigned int usec;

	usec = (unsigned int)((1000000.0 / fps)+0.50);

	stream_header_v.time_scale = usec;
	stream_header_v.data_rate = 1000000;
	avi_header.time_delay = usec;

	return 0;
}

/**
 * This function allows you to reset the video codec. In a standard use case,
 * you should not need to call it. However, if you need to, you can call it to
 * reset the video codec after you are done adding frames to your AVI file and
 * before you call gwavi_close().
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 * @param fourcc FourCC representing the codec of the video encoded stream. a
 *
 * @return 0 on success, -1 on error.
 */
int
gwavi_t::set_codec( const char *fourcc)
{
	if (check_fourcc(fourcc) != 0)
	{
		(void)fprintf(stderr, "WARNING: given fourcc does not seem to "
			      "be valid: %s\n", fourcc);
	}
	memset( this->fourcc, 0, sizeof(this->fourcc) );
	strcpy( this->fourcc, fourcc );

	memcpy(stream_header_v.codec, fourcc, 4);
	stream_format_v.compression_type =
		((unsigned int)fourcc[3] << 24) +
		((unsigned int)fourcc[2] << 16) +
		((unsigned int)fourcc[1] << 8) +
		((unsigned int)fourcc[0]);

	return 0;
}

/**
 * This function allows you to reset the video size. In a standard use case, you
 * should not need to call it. However, if you need to, you can call it to reset
 * the video height and width set in the AVI file after you are done adding
 * frames to your AVI file and before you call gwavi_close().
 *
 * @param gwavi Main gwavi structure initialized with gwavi_open()-
 * @param width Width of a frame.
 * @param height Height of a frame.
 *
 * @return 0 on success, -1 on error.
 */
int
gwavi_t::set_size( unsigned int width, unsigned int height)
{
	unsigned int size = (width * height * bits_per_pixel) / 8;

	avi_header.data_rate = size;
	avi_header.width = width;
	avi_header.height = height;
	avi_header.buffer_size = size;
	stream_header_v.buffer_size = size;
	stream_format_v.width = width;
	stream_format_v.height = height;
	stream_format_v.image_size = size;

	return 0;
}

int gwavi_t::printHeaders(void)
{
	char fourcc[8];
	int  ret, fileSize, size;

	if ( in == NULL )
	{
		return -1;
	}

	if (read_chars_bin(in, fourcc, 4) == -1)
		return -1;

	fourcc[4] = 0;
	printf("RIFF Begin: '%s'\n", fourcc );

	if (read_int(in, fileSize) == -1)
	{
		(void)fprintf(stderr, "gwavi_info: read_int() failed\n");
		return -1;
	}
	size = fileSize;
	printf("FileSize: %i\n", fileSize );

	if (read_chars_bin(in, fourcc, 4) == -1)
		return -1;

	size -= 4;
	fourcc[4] = 0;
	printf("FileType: '%s'\n", fourcc );

	while ( size > 0 )
	{
		if (read_chars_bin(in, fourcc, 4) == -1)
			return -1;


		fourcc[4] = 0;
		printf("Block: '%s'  %i\n", fourcc, size );

		size -= 4;

		if ( strcmp( fourcc, "LIST") == 0 )
		{
			ret = readList(0);

			if ( ret < 0 )
			{
				return -1;
			}
			size      -= ret;
		}
		else
		{
			ret = readChunk( fourcc, 0 );

			if ( ret < 0 )
			{
				return -1;
			}
			size      -= ret;
		}
	}

	return 0;
}

int gwavi_t::readList(int lvl)
{
	char fourcc[8], listType[8], pad[4];
	int size, ret, listSize=0;
	int bytesRead=0;
	char indent[256];

	memset( indent, ' ', lvl*3);
	indent[lvl*3] = 0;

	if (read_int(in, listSize) == -1)
	{
		(void)fprintf(stderr, "readList: read_int() failed\n");
		return -1;
	}
	size = listSize;

	if (read_chars_bin(in, listType, 4) == -1)
		return -1;

	listType[4] = 0;
	size -= 4;
	bytesRead += 4;

	printf("%sList Start: '%s'  %i\n", indent, listType, listSize );

	while ( size >= 4 )
	{
		if (read_chars_bin(in, fourcc, 4) == -1)
			return -1;

		size -= 4;
		bytesRead += 4;

		fourcc[4] = 0;
		//printf("Block: '%s'\n", fourcc );

		if ( strcmp( fourcc, "LIST") == 0 )
		{
			ret = readList(lvl+1);

			if ( ret < 0 )
			{
				return -1;
			}
			size      -= ret;
			bytesRead += ret;
		}
		else
		{
			ret = readChunk( fourcc, lvl+1 );

			if ( ret < 0 )
			{
				return -1;
			}
			size      -= ret;
			bytesRead += ret;
		}
	}

	if ( size > 0 )
	{
		int r = size % 4;

		if (read_chars_bin(in, pad, r) == -1)
		{
			(void)fprintf(stderr, "readChunk: read_int() failed\n");
			return -1;
		}
		size -= r;
		bytesRead += r;
	}
	printf("%sList End: %s   %i\n", indent, listType, bytesRead);

	return bytesRead;
}

int gwavi_t::readChunk(const char *id, int lvl)
{
	int ret, size, chunkSize, dataWord, bytesRead=0;
	char indent[256];

	memset( indent, ' ', lvl*3);
	indent[lvl*3] = 0;

	if (read_int(in, chunkSize) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_int() failed\n");
		return -1;
	}
	printf("%sChunk Start: %s   %i\n", indent, id, chunkSize);

	size = chunkSize;

	if ( strcmp( id, "strh") == 0 )
	{
		ret = readStreamHeader();

		if ( ret < 0 )
		{
			return -1;
		}
		size -= ret;
		bytesRead += ret;
	}

	while ( size >= 4 )
	{
		if (read_int(in, dataWord) == -1)
		{
			(void)fprintf(stderr, "readChunk: read_int() failed\n");
			return -1;
		}
		size -= 4;
		bytesRead += 4;
	}

	if ( size > 0 )
	{
		char pad[4];
		int r = size % 4;

		if (read_chars_bin(in, pad, r) == -1)
		{
			(void)fprintf(stderr, "readChunk: read_int() failed\n");
			return -1;
		}
		size -= r;
		bytesRead += r;
	}

	printf("%sChunk End: %s   %i\n", indent, id, bytesRead);

	return bytesRead+4;
}

int gwavi_t::readStreamHeader(void)
{
	gwavi_AVIStreamHeader hdr;
	
	printf("HDR Size: '%zi'\n", sizeof(hdr) );

	if (read_chars_bin(in, hdr.fccType, 4) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_chars_bin() failed\n");
		return -1;
	}

	if (read_chars_bin(in, hdr.fccHandler, 4) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_chars_bin() failed\n");
		return -1;
	}

	if (read_uint(in, hdr.dwFlags) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return -1;
	}

	if (read_ushort(in, hdr.wPriority) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_ushort() failed\n");
		return -1;
	}

	if (read_ushort(in, hdr.wLanguage) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_ushort() failed\n");
		return -1;
	}

	if (read_uint(in, hdr.dwInitialFrames) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return -1;
	}

	if (read_uint(in, hdr.dwScale) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return -1;
	}

	if (read_uint(in, hdr.dwRate) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return -1;
	}

	if (read_uint(in, hdr.dwStart) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return -1;
	}

	if (read_uint(in, hdr.dwLength) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return -1;
	}

	if (read_uint(in, hdr.dwSuggestedBufferSize) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return -1;
	}

	if (read_uint(in, hdr.dwQuality) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return -1;
	}

	if (read_uint(in, hdr.dwSampleSize) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_uint() failed\n");
		return -1;
	}

	if (read_short(in, hdr.rcFrame.left) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_ushort() failed\n");
		return -1;
	}

	if (read_short(in, hdr.rcFrame.top) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_ushort() failed\n");
		return -1;
	}

	if (read_short(in, hdr.rcFrame.right) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_ushort() failed\n");
		return -1;
	}

	if (read_short(in, hdr.rcFrame.bottom) == -1)
	{
		(void)fprintf(stderr, "readChunk: read_ushort() failed\n");
		return -1;
	}

	printf("fccType   : '%c%c%c%c'\n",
			hdr.fccType[0], hdr.fccType[1],
				hdr.fccType[2], hdr.fccType[3] );
	printf("fccHandler: '%c%c%c%c'\n",
			hdr.fccHandler[0], hdr.fccHandler[1],
				hdr.fccHandler[2], hdr.fccHandler[3] );
	printf("dwFlags              : '%u'\n", hdr.dwFlags );
	printf("wPriority            : '%u'\n", hdr.wPriority );
	printf("wLanguage            : '%u'\n", hdr.wLanguage );
	printf("dwInitialFrames      : '%u'\n", hdr.dwInitialFrames );
	printf("dwScale              : '%u'\n", hdr.dwScale );
	printf("dwRate               : '%u'\n", hdr.dwRate  );
	printf("dwStart              : '%u'\n", hdr.dwStart );
	printf("dwLength             : '%u'\n", hdr.dwLength );
	printf("dwSuggestedBufferSize: '%u'\n", hdr.dwSuggestedBufferSize );
	printf("dwQuality            : '%u'\n", hdr.dwQuality );
	printf("dwSampleSize         : '%u'\n", hdr.dwSampleSize );
	printf("rcFrame.left         : '%i'\n", hdr.rcFrame.left   );
	printf("rcFrame.top          : '%i'\n", hdr.rcFrame.top    );
	printf("rcFrame.right        : '%i'\n", hdr.rcFrame.right  );
	printf("rcFrame.bottom       : '%i'\n", hdr.rcFrame.bottom );

	return sizeof(gwavi_AVIStreamHeader);
}
