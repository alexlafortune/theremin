print("float pitches[] = {")

i = 110

while i < 10000:
    print(f"\t{i},")
    i *= 2 ** (1 / 12)
    
print("};")
