import string
import re

def hex_to_readable_string(hex_input):
    # Normalize: Remove all whitespace and ensure even length
    hex_input = re.sub(r'\s+', '', hex_input).lower()
    if len(hex_input) % 2 != 0:
        raise ValueError("Hex input must contain an even number of hex characters.")

    result = ""
    readable_only = ""

    for i in range(0, len(hex_input), 2):
        byte_hex = hex_input[i:i+2]
        try:
            byte_int = int(byte_hex, 16)
            char = chr(byte_int)

            # Add printable chars, else keep as hex representation
            if char in string.printable and char not in '\t\r\x0b\x0c':
                result += char
                readable_only += char
            else:
                result += f"\\x{byte_hex.upper()}"
        except ValueError:
            result += f"\\x{byte_hex.upper()}"

    return result, readable_only

if __name__ == "__main__":
    print("Enter raw MMS hex string (spaced or not):")
    raw_input = input("> ")

    try:
        full_str, readable_str = hex_to_readable_string(raw_input)
        print("\nDecoded Output:")
        print(full_str)

        print("\nReadable Text Only:")
        print(readable_str)
    except ValueError as e:
        print("Error:", e)
