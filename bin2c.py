import sys

with open(sys.argv[1], 'rb') as f:
    data = f.read()

array_name = sys.argv[2] if len(sys.argv) > 2 else 'data'
print(f"const unsigned char {array_name}[] = {{")
print(','.join(f'0x{b:02x}' for b in data))
print(f', 0x00')
print(f"}};\nconst unsigned int {array_name}_len = {len(data)};")
