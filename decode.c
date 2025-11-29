#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "decode.h"
#include "types.h"
#include <stdint.h>
#include <stdlib.h>


/* -----------------------------------------------------------------------------
 * read_and_validate_decode_args
 *
 * Block description:
 *   Parse and validate command-line arguments for the decode mode.
 *   Expected usage: program -d <stego.bmp> [output_file]
 *
 * Inputs:
 *   - argc : number of command line arguments
 *   - argv : array of argument strings
 *   - decInfo : pointer to DecodeInfo structure to populate
 *
 * Behavior:
 *   1. Ensure decInfo pointer is not NULL.
 *   2. Ensure at least 3 arguments are provided (program, -d, stego.bmp).
 *   3. Validate that argv[2] contains ".bmp".
 *   4. Populate decInfo->stego_image_fname and optionally decInfo->output_fname.
 *
 * Outputs:
 *   - decInfo fields updated (stego_image_fname and output_fname)
 *
 * Return:
 *   - e_success on success, e_failure on validation errors
 * -------------------------------------------------------------------------- */
Status read_and_validate_decode_args(int argc ,char *argv[], DecodeInfo *decInfo) 
{ 
    if (!decInfo) // Ensure DecodeInfo pointer is valid 
    { 
        return e_failure; 
    } 
    if (argc < 3) // Require at least: program, -d, stego.bmp 
    { 
        fprintf(stderr, "ERROR: INSUFFICIENT ARGUMENTS\n"); 
        printf("USAGE: %s -d <.bmp_file> [output file]\n", argv[0]); 
        return e_failure; 
    } 
    if (strstr(argv[2], ".bmp") == NULL) // Validate .bmp extension 
    { 
        fprintf(stderr, "ERROR: SOURCE IMAGE FILE SHOULD BE .bmp\n"); 
        return e_failure; 
    } decInfo->stego_image_fname = argv[2]; // Store stego image filename 
    if (argc >= 4)
    {
        decInfo->output_fname = argv[3];   // user provided a name
    }
    else
    {
        decInfo->output_fname = NULL;      // no name given
    }
    return e_success;
}


/* -----------------------------------------------------------------------------
 * open_files_decode
 *
 * Block description:
 *   Open the stego image file for decoding and position the file pointer
 *   after the BMP header (54 bytes). Decoder expects pixel data to start
 *   from this position.
 *
 * Inputs:
 *   - decInfo : pointer to DecodeInfo with stego_image_fname field set
 *
 * Behavior:
 *   1. Open file in binary read mode ("rb").
 *   2. If opening fails, print diagnostics and return e_failure.
 *   3. Seek forward 54 bytes to skip the BMP header.
 *
 * Outputs:
 *   - decInfo->fptr_stego_image set to opened FILE* (positioned after header)
 *
 * Return:
 *   - e_success on success, e_failure otherwise
 * -------------------------------------------------------------------------- */
Status open_files_decode(DecodeInfo *decInfo)
{
    decInfo->fptr_stego_image = fopen(decInfo->stego_image_fname, "rb"); // Open file

    if (decInfo->fptr_stego_image == NULL) // Check if open succeeded
    {
    	perror("fopen");  // Print system error message
    	fprintf(stderr, "ERROR: Unable to open file %s\n", decInfo->stego_image_fname);
    	return e_failure;
    }
    
    fseek(decInfo->fptr_stego_image, 54, SEEK_SET); // Skip BMP header

    return e_success;
}


/* -----------------------------------------------------------------------------
 * decode_byte_from_lsb
 *
 * Block description:
 *   Reconstruct a single data byte by reading the LSBs of 8 image bytes.
 *
 * Important detail:
 *   - This implementation uses LSB-first ordering: image_buffer[0].LSB -> bit0,
 *     image_buffer[1].LSB -> bit1, ..., image_buffer[7].LSB -> bit7.
 *
 * Inputs:
 *   - image_buffer : pointer to 8 bytes read from the image
 *   - data_byte    : pointer to char where reconstructed byte will be stored
 *
 * Behavior:
 *   - Validate inputs
 *   - Assemble unsigned char by left-shifting bits into position i
 *   - Store assembled value into *data_byte
 *
 * Return:
 *   - e_success on success, e_failure on invalid input
 * -------------------------------------------------------------------------- */
Status decode_byte_from_lsb(char *image_buffer, char *data_byte)
{
    if (!image_buffer || !data_byte) 
    return e_failure;

    unsigned char ch = 0;                // use unsigned while assembling bits
    for (int i = 0; i < 8; i++)
    {
        int bit = image_buffer[i] & 1;   // grab LSB
        ch |= (unsigned char)(bit << i); // LSB-first to bit i
    }

    *data_byte = (char)ch;               // store the result
    return e_success;
}



/* -----------------------------------------------------------------------------
 * decode_size_from_lsb
 *
 * Block description:
 *   Decode a 32-bit integer value encoded across 32 image bytes (1 bit per byte).
 *
 * Important detail:
 *   - Bit order: buffer[0].LSB -> result bit 0, buffer[1].LSB -> result bit 1, ...
 *
 * Inputs:
 *   - buffer : pointer to at least 32 bytes (each contains one encoded bit in LSB)
 *   - size   : pointer to int where assembled value will be stored
 *
 * Behavior:
 *   - Validate inputs
 *   - Iterate 32 times, extract LSB and place into corresponding bit position
 *
 * Return:
 *   - e_success on success, e_failure on invalid input
 * -------------------------------------------------------------------------- */
Status decode_size_from_lsb(char *buffer, int *size)
{
    if (!buffer || !size)  // Validate input
    {
        return e_failure;
    }

    unsigned int value = 0;   // use unsigned while assembling

    for (int i = 0; i < 32; i++) // Loop through 32 bits
    {
        int bit = buffer[i] & 1;      // Extract LSB
        value |= (bit << i);          // Place in correct bit position (LSB-first)
    }

    *size = (int)value;  // store final result
    return e_success;
}


/* -----------------------------------------------------------------------------
 * decode_data_from_image
 *
 * Block description:
 *   Decode 'size' bytes from the stego image by reading 8 image bytes per
 *   decoded byte and using decode_byte_from_lsb.
 *
 * Inputs:
 *   - data : buffer to receive decoded bytes (must have space for 'size' bytes)
 *   - size : number of bytes to decode
 *   - fptr_stego_image : FILE* positioned to the first encoded image byte
 *
 * Behavior:
 *   - For each byte to decode:
 *       a. Read 8 bytes from fptr_stego_image
 *       b. If fewer than 8 bytes read => failure
 *       c. Call decode_byte_from_lsb to recover one data byte
 *
 * Return:
 *   - e_success on success, e_failure on read or parameter errors
 * -------------------------------------------------------------------------- */
Status decode_data_from_image(char *data, int size, FILE *fptr_stego_image)
{
    if(!data || !fptr_stego_image) 
    {
        return e_failure; // Validate inputs
    }

    unsigned char buffer[8]; // Temporary storage for 8 image bytes
    for (int i = 0; i < size; i++)
    {
        int r = fread(buffer, 1, 8, fptr_stego_image); // Read 8 bytes
        if (r != 8)                     // Ensure correct read
        {
            return e_failure; 
        }                    
        decode_byte_from_lsb((char *)buffer, &data[i]);   // Decode 1 byte
    }
    
    return e_success;
}

/* -----------------------------------------------------------------------------
 * decode_magic_string
 *
 * Block description:
 *   Decode and verify the magic string signature embedded at the start of the
 *   encoded pixel stream. The magic string identifies that the image contains
 *   steganographically embedded data using the expected format.
 *
 * Inputs:
 *   - decInfo : pointer to DecodeInfo (must have fptr_stego_image set and
 *               positioned where encoded data begins)
 *
 * Behavior:
 *   - Decode exactly strlen(MAGIC_STRING) bytes and compare against MAGIC_STRING
 *
 * Return:
 *   - e_success if match, e_failure otherwise
 * -------------------------------------------------------------------------- */
Status decode_magic_string(DecodeInfo *decInfo)
{
    if (!decInfo || !decInfo->fptr_stego_image) 
    {
        return e_failure; // Validate inputs
    }
    const int len = (int)strlen(MAGIC_STRING); 
    char buffer[16];  // Temporary buffer for decoded magic string

    // Decode bytes for the magic string
    if (decode_data_from_image(buffer, len, decInfo->fptr_stego_image) != e_success)
        return e_failure;

    // Debugging: suppressed (uncomment for troubleshooting)
   // printf("DEBUG: magic string length is %d\n",len);
    //printf("DEBUG: magic string is %s\n",buffer);

    // Compare decoded string with expected MAGIC_STRING
    if (strcmp(buffer, MAGIC_STRING) != 0)
        return e_failure;

    return e_success;
}

/* -----------------------------------------------------------------------------
 * decode_file_extn_size
 *
 * Block description:
 *   Decode the secret file extension length, which is stored as a 32-bit
 *   integer encoded across 32 image bytes (1 bit per byte).
 *
 * Inputs:
 *   - decInfo : pointer to DecodeInfo (must have fptr_stego_image positioned)
 *
 * Behavior:
 *   - Read 32 bytes from image, call decode_size_from_lsb to assemble integer,
 *     store into decInfo->extn_size and sanity-check the value.
 *
 * Return:
 *   - e_success if extn_size decoded and within expected range, e_failure otherwise
 * -------------------------------------------------------------------------- */
Status decode_file_extn_size(DecodeInfo *decInfo)
{
    if (!decInfo) // Ensure DecodeInfo is valid
    {
        return e_failure;
    }

    char arr[32];
    int length;

    // Read 32 bytes from image (each byte contains one encoded bit of a 32-bit length)
    fread(arr, 32, 1, decInfo->fptr_stego_image);

    // Decode 32 LSBs into a 32-bit integer value
    decode_size_from_lsb(arr, &length);
    decInfo->extn_size = length;

    // Basic sanity check on decoded extension length (reject unreasonable values)
    if (length <=0 || length > 10)
        return e_failure;
    else
        return e_success;
}

/* -----------------------------------------------------------------------------
 * decode_secret_file_extn
 *
 * Block description:
 *   Using the previously decoded extn_size (32-bit), decode the file extension
 *   characters from the image and store them (null-terminated) in decInfo.
 *
 * Inputs:
 *   - decInfo : pointer to DecodeInfo with extn_size and fptr_stego_image set
 *
 * Behavior:
 *   - Validate extn_size within MAX_FILE_SUFFIX
 *   - Decode file_extn bytes using decode_data_from_image
 *   - Null-terminate decInfo->extn_secret_file
 *
 * Return:
 *   - e_success on success, e_failure otherwise
 * -------------------------------------------------------------------------- */
Status decode_secret_file_extn(DecodeInfo *decInfo)
{
    if (!decInfo) return e_failure;

    int file_extn = decInfo->extn_size;  // length decoded earlier (32-bit field)

    // sanity check (use your MAX_FILE_SUFFIX)
    if (file_extn <= 0 || file_extn >= MAX_FILE_SUFFIX)
        return e_failure;

    // read 'file_extn' bytes into the struct's fixed array
    if (decode_data_from_image(decInfo->extn_secret_file, file_extn,decInfo->fptr_stego_image) != e_success)
    {
        return e_failure;
    }

    // null-terminate
    decInfo->extn_secret_file[file_extn] = '\0';
    return e_success;
}



/* -----------------------------------------------------------------------------
 * decode_secret_file_size
 *
 * Block description:
 *   Decode the payload size (number of bytes of the secret file) that was
 *   stored as a 32-bit integer encoded across 32 image bytes (LSB-first).
 *
 * Inputs:
 *   - decInfo : pointer to DecodeInfo with fptr_stego_image positioned after extension
 *
 * Behavior:
 *   - Read 32 bytes from image into buffer
 *   - Call decode_size_from_lsb to assemble integer
 *   - Store resulting value in decInfo->size_secret_file
 *
 * Return:
 *   - e_success on success
 * -------------------------------------------------------------------------- */
Status decode_secret_file_size(DecodeInfo *decInfo)
{
    if (!decInfo || !decInfo->fptr_stego_image) return e_failure; // Validate inputs

    
    // Buffer for size bytes
    char arr[32];
    // Variable for file size
    int size = 0;

    // Read 32 bytes from iamge
    fread(arr, 32, 1, decInfo->fptr_stego_image);

    // Decode integer value (LSB-first bit order)
    decode_size_from_lsb(arr, &size);

    // Store size in the structure
    decInfo->size_secret_file = size;

    return e_success;
}


/* -----------------------------------------------------------------------------
 * decode_secret_file_data
 *
 * Block description:
 *   Decode the actual secret file content byte-by-byte from the image and write
 *   the decoded bytes into the output file decInfo->fptr_secret_out.
 *
 * Inputs:
 *   - decInfo : pointer to DecodeInfo with size_secret_file, fptr_stego_image,
 *               and fptr_secret_out properly set.
 *
 * Behavior:
 *   - Loop size_secret_file times
 *   - For each iteration: decode 1 byte (8 image bytes read) and fwrite it
 *
 * Notes:
 *   - This function does not strictly check fwrite return values (preserves original behavior)
 *
 * Return:
 *   - e_success on completion
 * -------------------------------------------------------------------------- */
Status decode_secret_file_data(DecodeInfo *decInfo)
{
    char ch; // Temporary storage for one decoded character
    for (long i = 0; i < decInfo->size_secret_file; i++)
    {
        decode_data_from_image(&ch, 1, decInfo->fptr_stego_image); // Decode 1 byte
        fwrite(&ch, 1, 1, decInfo->fptr_secret_out);              // Write byte to output file
    }

    // Inform user that decoding was successful
    printf("INFO: Secret file data decoded successfully\n");

    return e_success;
}



/* -----------------------------------------------------------------------------
 * do_decoding
 *
 * Block description:
 *   Top-level driver that coordinates the entire decoding workflow:
 *     1) Open stego image and position file pointer
 *     2) Decode and verify magic string
 *     3) Decode extension length (32-bit), extension string
 *     4) Build output filename and open output file
 *     5) Decode payload size (32-bit)
 *     6) Decode payload data and write to output
 *     7) Close files and return success
 *
 * Inputs:
 *   - decInfo : pointer to DecodeInfo with stego image name and optional output name
 *
 * Notes:
 *   - Uses goto FAILURE_CLOSE for cleanup on errors (preserves original behavior)
 * -------------------------------------------------------------------------- */
Status do_decoding(DecodeInfo *decInfo) 
{
            if (!decInfo) 
            { 
                printf("ERROR: NULL decInfo passed to do_decoding\n"); 
                return e_failure; 
            } 
            /* 1) Open files (open the stego image; output file may be created later) */ 
            printf("INFO: Opening required files \n"); 
            if (open_files_decode(decInfo) != e_success) 
            { 
                printf("ERROR: open_files_decode failed \n"); 
                return e_failure; 
            } 
            printf("INFO: Opened %s \n", decInfo->stego_image_fname); 
            /* 2) Decode magic string */ 
            printf("INFO: Decoding Magic String Signature\n"); 
            if (decode_magic_string(decInfo) != e_success) 
            { 
                printf("ERROR: decode_magic_string failed\n"); 
                goto FAILURE_CLOSE; 
            } 
            else 
            { 
                printf("INFO: Magic string OK\n"); 
            } 
        /* 3) Decode file extension size (32 bits) */
        printf("INFO: Decoding File Extension Size\n");
        if (decode_file_extn_size(decInfo) != e_success) {
            printf("ERROR: decode_file_extn_size failed\n");
            goto FAILURE_CLOSE;
        }
        printf("INFO: Extension length = %d\n", decInfo->extn_size);

        /* 4) Decode file extension string */
        printf("INFO: Decoding File Extension\n");
        if (decode_secret_file_extn(decInfo) != e_success) {
            printf("ERROR: decode_secret_file_extn failed\n");
            goto FAILURE_CLOSE;
        }
        printf("INFO: Extension = %s\n", decInfo->extn_secret_file);

                
        /* Build final output filename:
        - Take user name (or "decoded" if none)
        - Strip any extension the user typed (using strtok)
        - Append the decoded extension (e.g., ".txt") */
        char base[256];
        char final_name[256];

        /* 1) Choose base name */
        if (decInfo->output_fname == NULL || decInfo->output_fname[0] == '\0')
        {
            /* No name given -> default */
            strcpy(base, "decoded");
        }
        else
        {
        /* Copy user-given name into a modifiable buffer */
        strncpy(base, decInfo->output_fname, sizeof(base) - 1);
        base[sizeof(base) - 1] = '\0';

        /* Strip extension using strtok (only handles one dot, e.g., "decoded.txt") */
        char *token = strtok(base, ".");
        if (token && token[0] != '\0')
        {
            strcpy(base, token);   // keep part before the dot
        }
        else
        {
            strcpy(base, "decoded");   // fallback if empty or just "."
        }
    }

    /* 2) Append decoded extension (includes the dot, e.g., ".txt") */
    strcpy(final_name, base);
    strcat(final_name, decInfo->extn_secret_file);

    /* 3) Open the final file (overwrite if exists) */
    decInfo->fptr_secret_out = fopen(final_name, "wb");
    if (!decInfo->fptr_secret_out)
    {
        perror("fopen output");
        printf("ERROR: Unable to open output file %s\n", final_name);
        goto FAILURE_CLOSE;
    }
    printf("INFO: Opened %s\n", final_name);
    printf("INFO: Done. Opened all required files\n"); 

    /* 4) Decode file size */ 
    printf("INFO: Decoding File Size\n"); 
    if (decode_secret_file_size(decInfo) != e_success) 
    { 
        printf("ERROR: decode_secret_file_size failed\n"); 
        goto FAILURE_CLOSE; 
    } 
    else 
    { 
      printf("INFO: Secret size = %ld bytes\n", decInfo->size_secret_file); 
    } 
    /* 5) Decode the secret file data and write to output */ 
    printf("INFO: Decoding File Data\n"); 
    if (decode_secret_file_data(decInfo) != e_success) 
    { 
        printf("ERROR: decode_secret_file_data failed\n"); 
        goto FAILURE_CLOSE; 
    } 
    else 
    { 
        printf("INFO: Done\n"); 
    }
    /* 6) Close files */ 
    if (decInfo->fptr_stego_image) 
    { 
        fclose(decInfo->fptr_stego_image); 
    } 
    if (decInfo->fptr_secret_out) 
    { 
        fclose(decInfo->fptr_secret_out); 
    } 
    printf("INFO: ## Decoding Done Successfully ##\n"); 
    return e_success; 
                
    FAILURE_CLOSE: 
    if (decInfo->fptr_stego_image) 
    fclose(decInfo->fptr_stego_image); 
    if (decInfo->fptr_secret_out) 
    fclose(decInfo->fptr_secret_out); 
    return e_failure;
    }
