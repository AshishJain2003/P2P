#ifndef SHA1_HPP
#define SHA1_HPP

#include <cstdint>
#include <string>

using namespace std;

class SHA1
{
public:
    SHA1();
    void update(const string &s);
    void update(const char *data, size_t len);
    string final();
    static string from_file(const string &filename);
    static string from_data(const char* data, size_t len);

private:
    uint32_t digest[5];
    string buffer;
    uint64_t transforms;
};

#endif