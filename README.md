# Image-Steganography-using-LSB
A modular C project that hides (encodes) a secret file inside a 24-bit BMP image and extracts (decodes) it back using LSB substitution. Implements both encoding and decoding workflows with strict validation and metadata embedding.

---

## 🧠 OVERVIEW

This project hides a secret payload (text/audio/pdf/video) inside a BMP image by modifying the **least significant bits** of pixel data.  
The decoder reconstructs the secret file by reversing the LSB extraction process.

Your implementation includes:

- A fixed **magic string** (`#*`) used to verify encoded images 
- Encoding and decoding structured through `EncodeInfo` and `DecodeInfo`  
- Full BMP header preservation  
- Embedding of:
  - Magic string  
  - Secret file extension size  
  - Secret file extension  
  - Secret file size  
  - Actual file data  

Everything follows the LSB-first bit order as implemented in `encode.c` and `decode.c`.

---

## ✨ FEATURES

### ✔ Encode Mode  ./stego -e <source.bmp> <secret_file> [stego.bmp]

Performs:
- Validation of file types (BMP + TXT/PDF/MP3/MP4)  
- Capacity check for the BMP image  
- BMP header copy  
- LSB embedding of:
  - Magic string  
  - Secret file extension size  
  - Secret file extension  
  - Secret file size  
  - Secret file data  
- Copy remaining image data untouched 

---

### ✔ Decode Mode  ./stego -d <stego.bmp> [output_file]

Performs:
- Validation of BMP file  
- Header skip (54 bytes)  
- Magic string verification  
- Extraction of:
  - Secret file extension size  
  - Secret file extension  
  - Secret file size  
  - Secret file data  
- Creates output file and writes payload  

---

## 🧩 CONCEPTS & TECHNIQUES USED

- LSB steganography  
- BMP image structure (54-byte header)  
- Bitwise operations  
- Metadata embedding  
- Dynamic memory allocation  
- Binary file I/O  
- Big endian / little endian bit mapping  
- Modular multi-file C design  
- Error handling using `Status` enum (e_success / e_failure) 

---

## 💡 FUTURE SCOPE

- Add password-based encryption before embedding  
- Add support for PNG or JPEG using DCT steganography  
- Add progress display for large files  
- Add support for embedding multiple secret files  
- Build a GUI for drag-and-drop encoding  

---

## 📌 CONCLUSION

This project demonstrates hands-on experience with low-level binary file manipulation, image processing fundamentals, and the complete workflow of LSB-based steganography.  
It also shows understanding of modular C design, file structures, and secure data hiding techniques.


