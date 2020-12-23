#include "stirrup/stream.hpp"

#include <array>

using std::filesystem::path;
using std::vector;

namespace stirrup
{

stream::stream(FILE * file)
: file_(file)
{

}

stream::~stream()
{
    if(file_)
    {
        std::fclose(file_);
    }
}

vector<char> stream::read(std::size_t read_size)
{
    vector<char> result;

    std::array<char, 16 * 1024> buffer;
    std::size_t read_count = std::fread(buffer.data(), sizeof(char), buffer.size(), file_);
    result.resize(read_count);
    std::copy(begin(buffer), begin(buffer)+read_count, begin(result));
    return result;
}

void stream::write(vector<char> const & buffer)
{
    std::fwrite(buffer.data(), sizeof(char), buffer.size(), file_);
}

void stream::flush()
{
    std::fflush(file_);
}

}