#include "sha1.h"
#include <sstream>
#include <iomanip>
#include <fcntl.h>
#include <unistd.h>
#include <vector>

using namespace std;

static const size_t BLOCK_INTS = 16;
static const size_t BLOCK_BYTES = BLOCK_INTS * 4;

static void buffer_to_block(const string &buffer, uint32_t block[BLOCK_INTS])
{
    for (size_t i = 0; i < BLOCK_INTS; i++)
    {
        block[i] = (buffer[4 * i + 3] & 0xff) | (buffer[4 * i + 2] & 0xff) << 8 | (buffer[4 * i + 1] & 0xff) << 16 | (buffer[4 * i + 0] & 0xff) << 24;
    }
}

SHA1::SHA1()
{
    digest[0] = 0x67452301;
    digest[1] = 0xefcdab89;
    digest[2] = 0x98badcfe;
    digest[3] = 0x10325476;
    digest[4] = 0xc3d2e1f0;
    transforms = 0;
}

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))
#define blk(i) (block[i & 15] = rol(block[(i + 13) & 15] ^ block[(i + 8) & 15] ^ block[(i + 2) & 15] ^ block[i & 15], 1))

#define R0(v, w, x, y, z, i)                                      \
    z += ((w & (x ^ y)) ^ y) + block[i] + 0x5a827999 + rol(v, 5); \
    w = rol(w, 30);
#define R1(v, w, x, y, z, i)                                    \
    z += ((w & (x ^ y)) ^ y) + blk(i) + 0x5a827999 + rol(v, 5); \
    w = rol(w, 30);
#define R2(v, w, x, y, z, i)                            \
    z += (w ^ x ^ y) + blk(i) + 0x6ed9eba1 + rol(v, 5); \
    w = rol(w, 30);
#define R3(v, w, x, y, z, i)                                          \
    z += (((w | x) & y) | (w & x)) + blk(i) + 0x8f1bbcdc + rol(v, 5); \
    w = rol(w, 30);
#define R4(v, w, x, y, z, i)                            \
    z += (w ^ x ^ y) + blk(i) + 0xca62c1d6 + rol(v, 5); \
    w = rol(w, 30);

void transform(uint32_t digest[], uint32_t block[BLOCK_INTS], uint64_t &transforms);

void SHA1::update(const string &s)
{
    update(s.c_str(), s.length());
}

void SHA1::update(const char *data, size_t len)
{
    string str(data, len);
    buffer += str;
    while (buffer.size() >= BLOCK_BYTES)
    {
        uint32_t block[BLOCK_INTS];
        buffer_to_block(buffer, block);
        transform(digest, block, transforms);
        buffer.erase(0, BLOCK_BYTES);
    }
}

string SHA1::final()
{
    uint64_t total_bits = (transforms * BLOCK_BYTES + buffer.size()) * 8;
    buffer += 0x80;
    size_t orig_size = buffer.size();
    while (buffer.size() < BLOCK_BYTES)
    {
        buffer += (char)0x00;
    }
    uint32_t block[BLOCK_INTS];
    buffer_to_block(buffer, block);
    if (orig_size > BLOCK_BYTES - 8)
    {
        transform(digest, block, transforms);
        for (size_t i = 0; i < BLOCK_INTS - 2; i++)
            block[i] = 0;
    }
    block[BLOCK_INTS - 1] = total_bits;
    block[BLOCK_INTS - 2] = (total_bits >> 32);
    transform(digest, block, transforms);
    ostringstream result;
    for (size_t i = 0; i < sizeof(digest) / sizeof(digest[0]); i++)
    {
        result << hex << setfill('0') << setw(8) << digest[i];
    }
    return result.str();
}

string SHA1::from_file(const string &filename)
{
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0)
        return "";
    SHA1 checksum;
    char buffer[4096];
    int bytes_read = 0;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0)
    {
        checksum.update(buffer, bytes_read);
    }
    close(fd);
    return checksum.final();
}

string SHA1::from_data(const char *data, size_t len)
{
    string s(data, len);
    SHA1 checksum;
    checksum.update(s);
    return checksum.final();
}

void transform(uint32_t digest[], uint32_t block[BLOCK_INTS], uint64_t &transforms)
{
    uint32_t a = digest[0], b = digest[1], c = digest[2], d = digest[3], e = digest[4];
    R0(a, b, c, d, e, 0);
    R0(e, a, b, c, d, 1);
    R0(d, e, a, b, c, 2);
    R0(c, d, e, a, b, 3);
    R0(b, c, d, e, a, 4);
    R0(a, b, c, d, e, 5);
    R0(e, a, b, c, d, 6);
    R0(d, e, a, b, c, 7);
    R0(c, d, e, a, b, 8);
    R0(b, c, d, e, a, 9);
    R0(a, b, c, d, e, 10);
    R0(e, a, b, c, d, 11);
    R0(d, e, a, b, c, 12);
    R0(c, d, e, a, b, 13);
    R0(b, c, d, e, a, 14);
    R0(a, b, c, d, e, 15);
    R1(e, a, b, c, d, 16);
    R1(d, e, a, b, c, 17);
    R1(c, d, e, a, b, 18);
    R1(b, c, d, e, a, 19);
    R2(a, b, c, d, e, 20);
    R2(e, a, b, c, d, 21);
    R2(d, e, a, b, c, 22);
    R2(c, d, e, a, b, 23);
    R2(b, c, d, e, a, 24);
    R2(a, b, c, d, e, 25);
    R2(e, a, b, c, d, 26);
    R2(d, e, a, b, c, 27);
    R2(c, d, e, a, b, 28);
    R2(b, c, d, e, a, 29);
    R2(a, b, c, d, e, 30);
    R2(e, a, b, c, d, 31);
    R2(d, e, a, b, c, 32);
    R2(c, d, e, a, b, 33);
    R2(b, c, d, e, a, 34);
    R2(a, b, c, d, e, 35);
    R2(e, a, b, c, d, 36);
    R2(d, e, a, b, c, 37);
    R2(c, d, e, a, b, 38);
    R2(b, c, d, e, a, 39);
    R3(a, b, c, d, e, 40);
    R3(e, a, b, c, d, 41);
    R3(d, e, a, b, c, 42);
    R3(c, d, e, a, b, 43);
    R3(b, c, d, e, a, 44);
    R3(a, b, c, d, e, 45);
    R3(e, a, b, c, d, 46);
    R3(d, e, a, b, c, 47);
    R3(c, d, e, a, b, 48);
    R3(b, c, d, e, a, 49);
    R3(a, b, c, d, e, 50);
    R3(e, a, b, c, d, 51);
    R3(d, e, a, b, c, 52);
    R3(c, d, e, a, b, 53);
    R3(b, c, d, e, a, 54);
    R3(a, b, c, d, e, 55);
    R3(e, a, b, c, d, 56);
    R3(d, e, a, b, c, 57);
    R3(c, d, e, a, b, 58);
    R3(b, c, d, e, a, 59);
    R4(a, b, c, d, e, 60);
    R4(e, a, b, c, d, 61);
    R4(d, e, a, b, c, 62);
    R4(c, d, e, a, b, 63);
    R4(b, c, d, e, a, 64);
    R4(a, b, c, d, e, 65);
    R4(e, a, b, c, d, 66);
    R4(d, e, a, b, c, 67);
    R4(c, d, e, a, b, 68);
    R4(b, c, d, e, a, 69);
    R4(a, b, c, d, e, 70);
    R4(e, a, b, c, d, 71);
    R4(d, e, a, b, c, 72);
    R4(c, d, e, a, b, 73);
    R4(b, c, d, e, a, 74);
    R4(a, b, c, d, e, 75);
    R4(e, a, b, c, d, 76);
    R4(d, e, a, b, c, 77);
    R4(c, d, e, a, b, 78);
    R4(b, c, d, e, a, 79);
    digest[0] += a;
    digest[1] += b;
    digest[2] += c;
    digest[3] += d;
    digest[4] += e;
    transforms++;
}