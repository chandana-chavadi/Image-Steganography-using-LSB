#ifndef ENCODE_H
#define ENCODE_H
#define MAGIC_STRING "#*"

#include "types.h" // Contains user defined types

#define MAX_SECRET_BUF_SIZE 1
#define MAX_IMAGE_BUF_SIZE (MAX_SECRET_BUF_SIZE * 8)
#define MAX_FILE_SUFFIX 5

typedef struct _EncodeInfo
{
    char *src_image_fname;
    FILE *fptr_src_image;
    uint image_capacity;
    uint bits_per_pixel;
    char image_data[MAX_IMAGE_BUF_SIZE];

    char *secret_fname;
    FILE *fptr_secret;
    char extn_secret_file[MAX_FILE_SUFFIX];
    char secret_data[MAX_SECRET_BUF_SIZE];
    long size_secret_file;

    char *stego_image_fname;
    FILE *fptr_stego_image;

} EncodeInfo;

/* Read and validate Encode args from argv */
Status read_and_validate_encode_args(int argc ,char *argv[], EncodeInfo *encInfo);

/* Get File pointers for i/p and o/p files */
Status open_files(EncodeInfo *encInfo);

/* Get image size */
uint get_image_size_for_bmp(FILE *fptr_image);

/* Get file size */
uint get_file_size(FILE *fptr);

/* check capacity */
Status check_capacity(EncodeInfo *encInfo);

/* Copy bmp image header */
Status copy_bmp_header(FILE *fptr_src_image, FILE *fptr_dest_image);

/* Encode a byte into LSB of image data array */
Status encode_byte_to_lsb(char data, char *image_buffer);

/* Encode function, which does the real encoding */
Status encode_data_to_image(char *data, int size, FILE *fptr_src_image, FILE *fptr_stego_image);

/* Store Magic String */
Status encode_magic_string(const char *magic_string, EncodeInfo *encInfo);

/* Perform the encoding */
Status do_encoding(EncodeInfo *encInfo);

/* Encode a 32-bit integer size into 32 bytes of image data using LSB */
Status encode_size_to_lsb(int size, char *image_buffer);

/* Encode secret file extension size (length of extension) */
Status encode_secret_file_extn_size(int size, FILE*fptr_src_image, FILE *fptr_stego_image);

/* Encode secret file extension */
Status encode_secret_file_extn(const char *file_extn, EncodeInfo *encInfo);

/* Encode secret file size */
Status encode_secret_file_size(int file_size, EncodeInfo *encInfo);

/* Encode secret file data */
Status encode_secret_file_data(EncodeInfo *encInfo);

/* Copy remaining image bytes from src to stego image after encoding */
Status copy_remaining_img_data(FILE *fptr_src, FILE *fptr_dest);

#endif
