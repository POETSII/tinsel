import sys

from itertools import zip_longest, count

# https://docs.python.org/3/library/itertools.html#itertools-recipes
def grouper(iterable, n, *, incomplete='fill', fillvalue=None):
    "Collect data into non-overlapping fixed-length chunks or blocks"
    args = [iter(iterable)] * n
    if incomplete == 'fill':
        return zip_longest(*args, fillvalue=fillvalue)
    if incomplete == 'strict':
        return zip(*args, strict=True)
    if incomplete == 'ignore':
        return zip(*args)
    else:
        raise ValueError('Expected fill, strict, or ignore')

def fmt_words_in_verilog_memdump(file, fmt):
    output = ""
    lines = file.readlines()
    startaddr, *data = lines
    startaddr = startaddr.strip() # remove newline
    assert(startaddr[0] == "@") # verify it's probably a address
    startaddr = int(startaddr[1:], 16) # and that it's hex format

    data = " ".join(map(str.strip, data)).split(" ") # one word (list of 4 bytes) per index
    words = grouper(data, 4, fillvalue="00") # 0-padded 32b words
    hexwords = map(lambda b: "0x"+"".join(b[::-1]), words) # as C format literals, in the correct word order

    for addr, hexword in zip( count(startaddr, 4), hexwords):
        stmt = fmt.format(addr=addr, wordlit=hexword)
        output += stmt
    return output

app_path = sys.argv[1]

func = """
void writeapp_code() {
"""

codefmt = "  tinselWriteInstr({addr}, {wordlit});\n"
with open(app_path+"/code.v", "r") as f:
    func += fmt_words_in_verilog_memdump(f, codefmt)

func += """};

void writeapp_data() {
"""


datafmt = "  ((uint32_t *){addr})[0] = {wordlit};\n"
with open(app_path+"/data.v", "r") as f:
    func += fmt_words_in_verilog_memdump(f, datafmt)

func += "};"

print(func)
