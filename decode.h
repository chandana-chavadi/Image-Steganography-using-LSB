#ifndef DECODE_H
#define DECODE_H
#define MAGIC_STRING "#*"

#include "types.h" // Contains user defined types

/* 
 * Structure to store information required for
 * encoding secret file to source Image
 * Info about output and intermediate data is
 * also stored
 */

#define MAX_SECRET_BUF_SIZE 1
#define MAX_IMAGE_BUF_SIZE (MAX_SECRET_BUF_SIZE * 8)
#define MAX_FILE_SUFFIX 5

typedef struct _DecodeInfo
{
    /* Input: stego BMP image */
    char *stego_image_fname;    
    FILE *fptr_stego_image;     
    
    // char *magic_data;

    char image_data[MAX_IMAGE_BUF_SIZE];

    /* Output: secret file data */
    char *output_fname;         
    FILE *fptr_secret_out;     
    char extn_secret_file[MAX_FILE_SUFFIX]; 
    long size_secret_file;     
    int extn_size;

} DecodeInfo;

/* Prototypes (must match decode.c) */

/* Read and validate decode args: ./lsb_steg -d <stego.bmp> [output_file] */
Status read_and_validate_decode_args(int argc, char *argv[], DecodeInfo *decInfo);

/* Open stego image (and optionally output if name provided) */
Status open_files_decode(DecodeInfo *decInfo);

/* Decode a single byte from 8 image bytes (LSBs) */
Status decode_byte_from_lsb(char *image_buffer, char *data_byte);

/* Read `size` bytes from image by extracting LSBs */
Status decode_data_from_image(char *data, int size, FILE *fptr_stego_image);

/* Decode and verify magic string (reads MAGIC_STRING from image) */
Status decode_magic_string(DecodeInfo *decInfo);

/* Decode secret file extension (reads length then chars) */
Status decode_secret_file_extn(DecodeInfo *decInfo);

/* Decode secret file size (reads 4 little-endian bytes into decInfo->size_secret_file) */
Status decode_secret_file_size(DecodeInfo *decInfo);

/* Decode secret file data and write to output file */
Status decode_secret_file_data(DecodeInfo *decInfo);

/* High-level decode routine */
Status do_decoding(DecodeInfo *decInfo);

/* Decode integer size values from LSB of image bytes */
Status decode_size_from_lsb(char *buffer, int *size);

/* Decode file extn size */
Status decode_file_extn_size(DecodeInfo *decInfo);

#endif
