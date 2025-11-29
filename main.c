/*
NAME : C CHANDANA
ADMISSION ID : 25017_241

******************************************************************************
 * PROJECT: Image Steganography using LSB (Least Significant Bit) technique
 *
 * DESCRIPTION:
 *   This project implements a steganography system to hide (encode) a secret
 *   text file inside a 24-bit BMP image and retrieve (decode) it back from
 *   the stego image.
 *
 * ENCODING WORKFLOW:
 *   1. Validate command line arguments (source BMP, secret file, optional stego image).
 *   2. Open source image, secret file, and prepare stego image for writing.
 *   3. Copy BMP header (first 54 bytes) to stego image unchanged.
 *   4. Perform capacity check to ensure the source image can store the secret file.
 *   5. Encode using LSB technique:
 *        - Magic string (identifier)
 *        - Secret file extension size
 *        - Secret file extension
 *        - Secret file size
 *        - Secret file data (payload)
 *   6. Copy remaining image data not used for encoding to the stego image.
 *   7. Close all files.
 *
 * DECODING WORKFLOW:
 *   1. Validate command line arguments (stego BMP, optional output file).
 *   2. Open stego image and skip BMP header.
 *   3. Decode in sequence:
 *        - Magic string (verify data authenticity)
 *        - Secret file extension
 *        - Secret file size
 *        - Secret file data (payload)
 *   4. Reconstruct secret file and write it to disk.
 *   5. Close all files.
 *
 * CORE TECHNIQUE:
 *   - Each byte of secret data is split into 8 bits.
 *   - Each bit is embedded into the LSB of consecutive bytes of image data.
 *   - Minimal visual distortion occurs in the cover image.
 *
 * FILES:
 *   - encode.c / encode.h : Encoding functions
 *   - decode.c / decode.h : Decoding functions
 *   - types.h             : Data structures and type definitions
 *   - main.c              : Entry point, workflow controller
 *
 * RETURN VALUES:
 *   - Functions return Status (e_success / e_failure) for error handling.
 *
 * USAGE:
 *   Encoding : ./stego -e <source.bmp> <secret.txt> [stego.bmp]
 *   Decoding : ./stego -d <stego.bmp> [output_file]
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "encode.h"
#include "decode.h"
#include "types.h"

/* Check the operation type: encode or decode */
OperationType check_operation_type(int argc , char *argv[])
{
    if (argc < 2)
        return e_unsupported;

    if (strcasecmp(argv[1], "-e") == 0)
        return e_encode;
    else if (strcasecmp(argv[1], "-d") == 0)
        return e_decode;
    else
        return e_unsupported;
}

int main(int argc , char *argv[])
{
    // Determine whether user wants to encode or decode
    OperationType res = check_operation_type(argc , argv);

    if (res == e_encode)
    {
        EncodeInfo encInfo;
        memset(&encInfo, 0, sizeof(encInfo));  // Initialize all fields to 0/NULL

        // Validate encoding arguments
        if (read_and_validate_encode_args(argc, argv, &encInfo) == e_success)
        {
            // Perform the encoding procedure
            if (do_encoding(&encInfo) != e_success)
            {
                printf("ERROR: ENCODING FAILED\n");
                return 1; // Exit with error
            }
            return 0; // Encoding successful
        }
        else
        {
            // Invalid arguments for encoding
            printf("ERROR: INVALID ARGUMENTS FOR ENCODE\n");
            printf("USAGE: %s -e <src.bmp> <secret.txt> [stego.bmp]\n", argv[0]);
            return 1;
        }
    }
    else if (res == e_decode)
    {
        DecodeInfo decInfo;
        memset(&decInfo, 0, sizeof(decInfo));  // Initialize all fields

        // Validate decoding arguments
        if (read_and_validate_decode_args(argc, argv, &decInfo) == e_success)
        {
            // Perform decoding procedure
            if (do_decoding(&decInfo) != e_success)
            {
                printf("ERROR: DECODING FAILED\n");
                return 1; // Exit with error
            }
            return 0; // Decoding successful
        }
        else
        {
            // Invalid arguments for decoding
            printf("ERROR: INVALID ARGUMENTS FOR DECODE\n");
            printf("USAGE: %s -d <stego.bmp> [output_file]\n", argv[0]);
            return 1;
        }
    }
    else
    {
        // Unsupported operation flag
        printf("ERROR: Unsupported operation\n");
        printf("Usage:\n  %s -e <src.bmp> <secret.txt> [stego.bmp]   (encode)\n  %s -d <stego.bmp> [output_file]            (decode)\n", argv[0], argv[0]);
        return 1;
    }

    return 0;
}





