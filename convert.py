import sys
import struct

def fix_bmp_header(bmp_file):
    with open(bmp_file, 'rb') as file:
        header = file.read(54)  # BMP header is typically 54 bytes long
        
    # Unpack file header and DIB header
    file_header = struct.unpack('<2sIHHI', header[:14])
    dib_header = struct.unpack('<IIIHHIIIIII', header[14:54])
    
    if file_header[0] != b'BM':
        print("Not a BMP file")
        return

    # Example fix: Correct the size field in the BMP file header if incorrect
    filesize = file_header[1]
    calculated_size = len(header) + dib_header[6]  # Image size + header size
    if filesize != calculated_size:
        print("Fixing file size in header")
        file_header = (file_header[0], calculated_size) + file_header[2:]
        new_header = struct.pack('<2sIHHI', *file_header) + header[14:54]
    else:
        new_header = header

    # Save the new file
    new_file_name = bmp_file.replace('.bmp', '_fixed.bmp')
    with open(new_file_name, 'wb') as new_file:
        new_file.write(new_header)
        file.seek(54)
        new_file.write(file.read())  # write the rest of the original file

    print(f"File saved as {new_file_name}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python script.py <filename.bmp>")
        sys.exit(1)
    
    bmp_filename = sys.argv[1]
    fix_bmp_header(bmp_filename)
