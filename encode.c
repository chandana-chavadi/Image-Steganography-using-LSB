#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "encode.h"
#include "types.h"


/* -----------------------------------------------------------------------------
 * get_image_size_for_bmp
 *
 * Block description:
 *   Determine the total usable pixel data size of a 24-bit BMP image.
 *
 * Inputs:
 *   - fptr_image : pointer to an open BMP file (function will seek within it)
 *
 * Returns:
 *   - Total pixel data capacity in bytes = width * height * 3
 *
 * Behavior/details:
 *   - Seeks to offset 18 (BMP header field for width), reads width (4 bytes).
 *   - Immediately reads height (4 bytes) that follows width.
 *   - Multiplies width*height*3 (RGB channels) to compute total usable bytes.
 *   - Caller must ensure the file is a valid 24-bit BMP. This function does not
 *     validate other header fields (bits per pixel, compression, etc.).
 * -------------------------------------------------------------------------- */
uint get_image_size_for_bmp(FILE *fptr_image)
{
    uint width, height;
    fseek(fptr_image, 18, SEEK_SET);         // Move to width offset in BMP header
    fread(&width, sizeof(int), 1, fptr_image); 
    // width read (bytes per row may be padded on disk, but we use logical width)
    //printf("width = %u\n", width);
    fread(&height, sizeof(int), 1, fptr_image);
    // height read
    //printf("height = %u\n", height);

    return width * height * 3;               // 3 bytes per pixel (R,G,B)
}

/* -----------------------------------------------------------------------------
 * open_files
 *
 * Block description:
 *   Open the three files required for encoding: source BMP, secret file, and
 *   stego output BMP. Sets corresponding FILE* fields in encInfo.
 *
 * Inputs:
 *   - encInfo : pointer to EncodeInfo structure pre-filled with filenames
 *
 * Outputs:
 *   - encInfo->fptr_src_image, encInfo->fptr_secret, encInfo->fptr_stego_image
 *
 * Returns:
 *   - e_success on successful opening of all files
 *   - e_failure on any error (and prints diagnostics)
 *
 * Notes:
 *   - Source and secret opened as "rb" (read binary). Stego opened "wb"
 *     (write binary) which will overwrite an existing file with that name.
 * -------------------------------------------------------------------------- */
Status open_files(EncodeInfo *encInfo)
{
    encInfo->fptr_src_image = fopen(encInfo->src_image_fname, "rb");   // Open source image
    if (encInfo->fptr_src_image == NULL)
    {
        perror("fopen");
        fprintf(stderr, "ERROR: Unable to open file %s\n", encInfo->src_image_fname);
        return e_failure;
    }

    encInfo->fptr_secret = fopen(encInfo->secret_fname, "rb");        // Open secret file
    if (encInfo->fptr_secret == NULL)
    {
        perror("fopen");
        fprintf(stderr, "ERROR: Unable to open file %s\n", encInfo->secret_fname);
        return e_failure;
    }

    encInfo->fptr_stego_image = fopen(encInfo->stego_image_fname, "wb"); // Open stego image for writing
    if (encInfo->fptr_stego_image == NULL)
    {
        perror("fopen");
        fprintf(stderr, "ERROR: Unable to open file %s\n", encInfo->stego_image_fname);
        return e_failure;
    }

    return e_success;  // All files opened successfully
}

/* -----------------------------------------------------------------------------
 * read_and_validate_encode_args
 *
 * Block description:
 *   Parse and validate command-line arguments for encode mode.
 *
 * Inputs:
 *   - argc, argv: command-line arguments
 *   - encInfo    : pointer to EncodeInfo to populate with filenames
 *
 * Behavior:
 *   - Ensure minimum arguments provided
 *   - Validate file extensions for source (.bmp) and secret (.txt)
 *   - Accept optional stego output filename (must end with .bmp) or default to "stego.bmp"
 *
 * Returns:
 *   - e_success if arguments valid; e_failure otherwise
 * -------------------------------------------------------------------------- */
Status read_and_validate_encode_args(int argc ,char *argv[], EncodeInfo *encInfo)
{
    if (argc < 4)  // Minimum arguments: program, -e, src_image, secret_file
    {
        fprintf(stderr, "ERROR: INSUFFICIENT ARGUMENTS\n");
        return e_failure;
    }

    if(strstr(argv[2],".bmp") == NULL)  // Check source image extension
    {
        fprintf(stderr,"ERROR : SOURCE IMAGE FILE SHOULD BE .bmp \n");
        return e_failure;
    }
    encInfo->src_image_fname = argv[2];  // Store source image filename

    if (strstr(argv[3], ".txt") == NULL &&
    strstr(argv[3], ".pdf") == NULL &&
    strstr(argv[3], ".mp3") == NULL &&
    strstr(argv[3], ".mp4") == NULL)
    {
        fprintf(stderr, "ERROR: SECRET FILE SHOULD BE .txt/.pdf/.mp3/.mp4\n");
        return e_failure;
    }

    encInfo->secret_fname = argv[3];     // Store secret file filename

    if(argc >= 5)                        // If stego image filename provided
    {
        encInfo->stego_image_fname = argv[4];
        if(strstr(encInfo->stego_image_fname,".bmp") == NULL)  // Validate extension
        {
            fprintf(stderr,"ERROR : SOURCE IMAGE FILE SHOULD BE .bmp \n");
            return e_failure;
        }
    }
    else
    {
        encInfo->stego_image_fname = "stego.bmp"; // Default stego image filename
    }

    return e_success;
}

/* -----------------------------------------------------------------------------
 * get_file_size
 *
 * Block description:
 *   Determine the size of an open file in bytes.
 *
 * Inputs:
 *   - fptr : pointer to an open FILE (seekable)
 *
 * Returns:
 *   - file size in bytes as uint
 *
 * Notes:
 *   - Uses fseek to seek to end and ftell to get size, then rewinds to start.
 * -------------------------------------------------------------------------- */
uint get_file_size(FILE *fptr)
{
    fseek(fptr, 0, SEEK_END);      // Move to end
    long file_size = ftell(fptr);  // Get current position = size
    rewind(fptr);                  // Reset pointer to beginning for later reads
    return (uint)file_size;
}

/* -----------------------------------------------------------------------------
 * copy_bmp_header
 *
 * Block description:
 *   Copy the BMP header (first 54 bytes) from the source image to the destination
 *   (stego) image without modification.
 *
 * Inputs:
 *   - fptr_src_image  : source image FILE* (readable)
 *   - fptr_dest_image : destination image FILE* (writable)
 *
 * Returns:
 *   - e_success on successful copy
 *
 * Notes:
 *   - Seeks both file pointers to the start before copying to ensure correct alignment.
 * -------------------------------------------------------------------------- */
Status copy_bmp_header(FILE *fptr_src_image, FILE *fptr_dest_image)
{
    unsigned char buffer[54];
    fseek(fptr_src_image, 0, SEEK_SET);   // Source pointer to start
    fseek(fptr_dest_image, 0, SEEK_SET);  // Destination pointer to start
    fread(buffer, 1, 54, fptr_src_image); // Read header (54 bytes)
    fwrite(buffer, 1, 54, fptr_dest_image);// Write header
    return e_success;
}

/* -----------------------------------------------------------------------------
 * check_capacity
 *
 * Block description:
 *   Verify the source BMP image has enough capacity to hide the secret file.
 *
 * Inputs:
 *   - encInfo : pointer to EncodeInfo with opened fptr_src_image and fptr_secret
 *
 * Returns:
 *   - e_success if image can hold secret + metadata; e_failure otherwise
 *
 * Calculation:
 *   - image_capacity = width * height * 3 (bytes)
 *   - required_bytes = image_capacity / 8 (since 1 secret bit per image byte)
 *   - We require required_bytes >= secret_file_size + 14 (metadata overhead)
 * -------------------------------------------------------------------------- */
Status check_capacity(EncodeInfo *encInfo)
{
    uint image_capacity = get_image_size_for_bmp(encInfo->fptr_src_image); // Total image bytes
    uint secret_file_size = get_file_size(encInfo->fptr_secret);           // Secret file size
    uint required_bytes = image_capacity / 8;                             // 1 bit per image byte

    if(required_bytes < (secret_file_size + 14))  // Check if enough space for payload + metadata
    {
        return e_failure;
    }
    return e_success;
}

/* -----------------------------------------------------------------------------
 * encode_byte_to_lsb
 *
 * Block description:
 *   Embed a single data byte into the least significant bits of 8 image bytes.
 *
 * Inputs:
 *   - data         : the byte to embed
 *   - image_buffer : pointer to at least 8 bytes of image data; modified in-place
 *
 * Behavior:
 *   - For i=0..7, set image_buffer[i].LSB = (data >> i) & 1 (LSB-first order)
 *
 * Returns:
 *   - e_success always (function does not currently detect errors)
 * -------------------------------------------------------------------------- */
Status encode_byte_to_lsb(char data, char *image_buffer)
{
    for(int i = 0; i < 8; i++)
    {
        image_buffer[i] = (image_buffer[i] & ~1) | ((data >> i) & 1); // Embed bit i
    }
    return e_success;
}

/* -----------------------------------------------------------------------------
 * encode_size_to_lsb
 *
 * Block description:
 *   Embed a 32-bit integer value into the LSBs of 32 consecutive image bytes.
 *
 * Inputs:
 *   - size         : integer value to embed
 *   - image_buffer : pointer to at least 32 bytes to be modified in-place
 *
 * Behavior:
 *   - For i=0..31, set image_buffer[i].LSB = (size >> i) & 1 (LSB-first order)
 *
 * Returns:
 *   - e_success always (function does not currently detect errors)
 * -------------------------------------------------------------------------- */
Status encode_size_to_lsb(int size, char *image_buffer)
{
    for (int i = 0; i < 32; i++)
    {
        image_buffer[i] = (image_buffer[i] & (~1)) | ((size >> i) & 1); // Embed each bit
    }
    return e_success;
}

/* -----------------------------------------------------------------------------
 * encode_data_to_image
 *
 * Block description:
 *   Encode 'size' bytes from 'data' into the source image and write modified
 *   bytes to the stego image. Each data byte uses 8 image bytes (1 bit each).
 *
 * Inputs:
 *   - data            : pointer to data bytes to encode
 *   - size            : length in bytes of data
 *   - fptr_src_image  : FILE* for the source image (read position must be set)
 *   - fptr_stego_image: FILE* for the destination stego image (write position must be set)
 *
 * Behavior:
 *   - For each data byte:
 *       a) Read 8 bytes from source image into buffer
 *       b) Call encode_byte_to_lsb to modify buffer's LSBs
 *       c) Write the 8-byte modified buffer to the stego image
 *
 * Returns:
 *   - e_success on completion; returns e_failure if encode_byte_to_lsb indicates failure
 * -------------------------------------------------------------------------- */
Status encode_data_to_image(char *data, int size, FILE *fptr_src_image, FILE *fptr_stego_image)
{
    unsigned char buffer[8];
    for(int i = 0; i < size; i++)
    {
        fread(buffer, 1, 8, fptr_src_image);           // Read 8 bytes from source image
        if(encode_byte_to_lsb((unsigned char)data[i], (char *) buffer) != e_success)
            return e_failure;                          // Encode byte into buffer
        fwrite(buffer, 1, 8, fptr_stego_image);       // Write modified buffer to stego image
    }
    return e_success;
}

/* -----------------------------------------------------------------------------
 * encode_magic_string
 *
 * Block description:
 *   Embed a predefined magic string into the image to mark presence of hidden data.
 *
 * Inputs:
 *   - magic_string : null-terminated string used as signature
 *   - encInfo      : pointer to EncodeInfo (file pointers must be positioned correctly)
 *
 * Behavior:
 *   - Encodes strlen(magic_string) bytes (the string characters)
 *
 * Returns:
 *   - e_success on success; e_failure if inputs invalid or encoding failed
 * -------------------------------------------------------------------------- */
Status encode_magic_string(const char *magic_string, EncodeInfo *encInfo)
{
    if(!magic_string || !encInfo)
        return e_failure;

    //printf("DEBUG: magic string is %s \n", magic_string);
    int length = strlen(magic_string);
    //printf("DEBUG: length of magic string = %d\n", length);

    if(encode_data_to_image((char *)magic_string, length, encInfo->fptr_src_image, encInfo->fptr_stego_image) != e_success)
        return e_failure;

    return e_success;
}

/* -----------------------------------------------------------------------------
 * encode_secret_file_extn_size
 *
 * Block description:
 *   Encode the length of the secret file extension as a 32-bit integer in the
 *   LSBs of 32 consecutive image bytes.
 *
 * Inputs:
 *   - size             : length (in bytes) of extension string (e.g., 4 for ".txt")
 *   - fptr_src_image   : source image FILE* (read position must be set)
 *   - fptr_stego_image : stego image FILE* (write position must be set)
 *
 * Behavior:
 *   - Reads 32 bytes from source, calls encode_size_to_lsb on that array,
 *     then writes the 32 modified bytes to the stego image.
 *
 * Returns:
 *   - e_success on completion; e_failure on invalid inputs
 * -------------------------------------------------------------------------- */
Status encode_secret_file_extn_size(int size, FILE *fptr_src_image, FILE *fptr_stego_image)
{
    if(!fptr_src_image || !fptr_stego_image)
        return e_failure;

    char arr[32];
    fread(arr, 1, 32, fptr_src_image);       // Read 32 bytes from source
    encode_size_to_lsb(size, arr);           // Embed extension size bits (LSB-first)
    fwrite(arr, 1, 32, fptr_stego_image);    // Write modified bytes to stego
    return e_success;
}

/* -----------------------------------------------------------------------------
 * encode_secret_file_extn
 *
 * Block description:
 *   Encode the secret file extension characters into the image.
 *
 * Inputs:
 *   - file_extn : extension string including the dot (e.g., ".txt")
 *   - encInfo   : pointer to EncodeInfo with file pointers positioned correctly
 *
 * Behavior:
 *   - Encodes strlen(file_extn) bytes directly using encode_data_to_image
 *
 * Returns:
 *   - e_success on success, e_failure on invalid input or encoding failure
 * -------------------------------------------------------------------------- */
Status encode_secret_file_extn(const char *file_extn, EncodeInfo *encInfo)
{
    if(!file_extn || !encInfo)
        return e_failure;

    //printf("DEBUG: file_extn = '%s'\n", file_extn);

    if(encode_data_to_image((char *)file_extn, strlen(file_extn), encInfo->fptr_src_image, encInfo->fptr_stego_image) != e_success)
        return e_failure;

    return e_success;
}

/* -----------------------------------------------------------------------------
 * encode_secret_file_size
 *
 * Block description:
 *   Encode the secret file size (integer number of bytes) into 32 image bytes
 *   (one bit per image byte) using LSB substitution.
 *
 * Inputs:
 *   - file_size : size in bytes (int)
 *   - encInfo   : pointer to EncodeInfo with file pointers positioned correctly
 *
 * Behavior:
 *   - Reads 32 bytes from source, calls encode_size_to_lsb with file_size,
 *     writes modified bytes to stego image.
 *
 * Returns:
 *   - e_success on success, e_failure on invalid input
 * -------------------------------------------------------------------------- */
Status encode_secret_file_size(int file_size, EncodeInfo *encInfo)
{
    if(file_size < 0)
        return e_failure;

    char arr[32];
    fread(arr, 32, 1, encInfo->fptr_src_image);    // Read 32 bytes from source image
    encode_size_to_lsb(file_size, arr);            // Embed the 32-bit size (LSB-first)
    fwrite(arr, 32, 1, encInfo->fptr_stego_image);// Write modified bytes to stego image
    return e_success;
}

/* -----------------------------------------------------------------------------
 * encode_secret_file_data
 *
 * Block description:
 *   Read the secret file in chunks and encode each chunk into the image.
 *
 * Inputs:
 *   - encInfo : pointer to EncodeInfo with fptr_secret, fptr_src_image, and fptr_stego_image
 *
 * Behavior:
 *   - Rewinds secret file, reads up to 512 bytes per iteration into buffer,
 *     encodes that chunk using encode_data_to_image which consumes 8*chunk bytes
 *     from the source image and writes them to the stego image.
 *   - After loop checks for read errors using ferror.
 *
 * Returns:
 *   - e_success on success, e_failure on read/encode errors
 * -------------------------------------------------------------------------- */
Status encode_secret_file_data(EncodeInfo *encInfo)
{
    if(!encInfo || !encInfo->fptr_secret)
    {
        printf("ERROR: encode_secret_file_data: invalid input\n");
        return e_failure;
    }

    rewind(encInfo->fptr_secret);   // Reset secret file pointer to the beginning

    unsigned char buffer[512];      // Buffer to hold a chunk of secret data
    long nread;                     // Number of bytes read in each iteration

    // Read the secret file in chunks until EOF
    while ((nread = fread(buffer, 1, 512, encInfo->fptr_secret)) > 0)
    {
        // For each chunk read, encode those bytes into the source image and write to stego image
        if(encode_data_to_image((char *)buffer, (int)nread, encInfo->fptr_src_image, encInfo->fptr_stego_image) != e_success)
            return e_failure; // Propagate failure if encoding of any chunk fails
    }

    // If loop ended due to a read error (not EOF), indicate failure
    if(ferror(encInfo->fptr_secret))
        return e_failure;

    return e_success;
}

/* -----------------------------------------------------------------------------
 * copy_remaining_img_data
 *
 * Block description:
 *   Copy any remaining image bytes (after encoding) from source to stego image.
 *
 * Inputs:
 *   - fptr_src  : source image FILE* (positioned after the bytes used for encoding)
 *   - fptr_dest : destination stego image FILE*
 *
 * Behavior:
 *   - Reads in 512-byte chunks from source and writes out to destination
 *   - Validates that write count equals read count, and checks for read errors
 *
 * Returns:
 *   - e_success on successful copy, e_failure on errors
 * -------------------------------------------------------------------------- */
Status copy_remaining_img_data(FILE *fptr_src, FILE *fptr_dest)
{
    if(!fptr_src || !fptr_dest)   // Validate that both file pointers are not NULL
    {
        printf("ERROR: copy_remaining_img_data: invalid file pointers\n");
        return e_failure;         // Return failure if invalid pointers are detected
    }

    unsigned char buffer[512];    // Buffer to temporarily hold chunks of image data
    long nread;                   // Variable to store number of bytes read in each iteration

    // Continue reading until end of source file
    while((nread = fread(buffer, 1, 512, fptr_src)) > 0)
    {
        long nwritten = fwrite(buffer, 1, nread, fptr_dest); // Write read bytes to destination file
        if(nwritten != nread)       // Check if write count matches read count
            return e_failure;       // Return failure if any write error occurs
    }

    if(ferror(fptr_src))           // Check for read error after loop ends
        return e_failure;          // Return failure if reading failed unexpectedly

    return e_success;              // Return success if copying completed without errors
}

/* -----------------------------------------------------------------------------
 * do_encoding
 *
 * Block description:
 *   Top-level driver that carries out the full encoding workflow:
 *     1) Open files
 *     2) Ensure secret file non-empty
 *     3) Check image capacity
 *     4) Copy BMP header
 *     5) Encode magic string
 *     6) Encode extension length and extension
 *     7) Encode secret file size
 *     8) Encode secret data
 *     9) Copy the remaining image data and close files
 *
 * Inputs:
 *   - encInfo : pointer to EncodeInfo pre-populated with filenames
 *
 * Notes:
 *   - Uses goto FAILURE_CLOSE for cleanup on error (preserves original behavior)
 * -------------------------------------------------------------------------- */
Status do_encoding(EncodeInfo *encInfo)
{
    if(!encInfo)  // Check if the EncodeInfo structure pointer is valid
    {
        printf("ERROR: NULL encInfo passed to do_encoding\n");
        return e_failure;  // Return failure if NULL pointer detected
    }

    printf("INFO: Opening required files \n");
    if(open_files(encInfo) != e_success)  // Try to open all necessary files (source, secret, stego)
    {
        printf("ERROR: open_files failed \n");
        return e_failure;  // Stop execution if file opening failed
    }

    // Confirmation logs for opened files
    printf("INFO: Opened %s  \n", encInfo->src_image_fname);
    printf("INFO: Opened %s  \n", encInfo->secret_fname);
    printf("INFO: Opened %s \n", encInfo->stego_image_fname);
    printf("INFO: Done \n");

    printf("INFO: ## Encoding Procedure Started ##  \n");

    encInfo->size_secret_file = get_file_size(encInfo->fptr_secret); // Calculate size of secret file
    if(encInfo->size_secret_file <= 0)  // Check if secret file is empty or unreadable
    {
        printf("ERROR: Secret file empty or unreadable\n");
        goto FAILURE_CLOSE;  // Jump to failure cleanup block
    }
    else
        printf("INFO: Done. Not Empty\n");  // Confirm secret file is valid

    if(check_capacity(encInfo) != e_success) // Verify if image has enough capacity to hold secret
    {
        printf("ERROR: Image does not have enough capacity to hold the secret\n");
        goto FAILURE_CLOSE;  // Abort if insufficient capacity
    }
    else
        printf("INFO: Done. Found OK\n");  // Capacity check passed

    rewind(encInfo->fptr_src_image); // Reset source image file pointer to the start for encoding

    printf("INFO: Copying Image Header\n");
    // Copy BMP header (first 54 bytes) from source to stego image
    if(copy_bmp_header(encInfo->fptr_src_image, encInfo->fptr_stego_image) != e_success)
    {
        printf("ERROR: copy_bmp_header failed\n");
        goto FAILURE_CLOSE;
    }
    else
        printf("INFO: Done \n");

    // Embed magic string (used as a marker to detect encoded data)
    printf("INFO: Encoding Magic String Signature\n");
    if(encode_magic_string(MAGIC_STRING, encInfo) != e_success)
    {
        printf("ERROR: encode_magic_string failed\n");
        goto FAILURE_CLOSE;
    }
    else
        printf("INFO: Done \n");

    // Extract file extension (e.g., ".txt") from secret file name
    strcpy(encInfo->extn_secret_file, strstr(encInfo->secret_fname, "."));

    // Encode the length of the secret file extension
    printf("INFO: Encoding %s File Extenstion Size\n", encInfo -> secret_fname);
    if(encode_secret_file_extn_size(strlen(encInfo->extn_secret_file),encInfo->fptr_src_image,encInfo->fptr_stego_image) != e_success)
    {
        printf("ERROR : Failed to encode secret file extn size\n");
        return e_failure;
    }
    else
        printf("INFO: Done\n");

    // Encode the actual file extension (e.g., "txt")
    printf("INFO: Encoding %s File Extenstion\n", encInfo -> secret_fname);
    if(encode_secret_file_extn(encInfo->extn_secret_file, encInfo) != e_success)
    {
        printf("ERROR : Failed to encode secret file extn\n");
        return e_failure;
    }
    else
        printf("INFO: Done\n");

    // Encode size of the secret file (number of bytes)
    printf("INFO: Encoding %s File Size\n", encInfo -> secret_fname);
    if(encode_secret_file_size(encInfo->size_secret_file, encInfo) != e_success)
    {
        printf("ERROR: encode_secret_file_size failed\n");
        goto FAILURE_CLOSE;
    }
    else
        printf("INFO: Done \n");

    // Encode actual secret file data into image pixels
    printf("INFO: Encoding %s File Data\n", encInfo -> secret_fname);
    if(encode_secret_file_data(encInfo) != e_success)
    {
        printf("ERROR: encode_secret_file_data failed\n");
        goto FAILURE_CLOSE;
    }
    else
        printf("INFO: Done \n");

    // Copy the remaining unused pixel data from source image to stego image
    printf("INFO: Copying Left Over Data\n");
    if(copy_remaining_img_data(encInfo->fptr_src_image, encInfo->fptr_stego_image) != e_success)
    {
        printf("ERROR: copy_remaining_img_data failed\n");
        goto FAILURE_CLOSE;
    }
    else
        printf("INFO: Done \n");

    // Close all opened files after successful encoding
    fclose(encInfo->fptr_src_image);
    fclose(encInfo->fptr_secret);
    fclose(encInfo->fptr_stego_image);

    printf("INFO: ## Encoding Done Successfully ##  \n");
    return e_success;  // Return success if everything executed properly

FAILURE_CLOSE:
    // Cleanup section in case of any error
    if(encInfo->fptr_src_image) 
        fclose(encInfo->fptr_src_image);   // Close source image if open
    if(encInfo->fptr_secret)   
        fclose(encInfo->fptr_secret);      // Close secret file if open
    if(encInfo->fptr_stego_image) 
        fclose(encInfo->fptr_stego_image); // Close stego image if open
    return e_failure;  // Return failure after cleanup
}





