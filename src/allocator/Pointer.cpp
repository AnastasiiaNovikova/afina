#include <afina/allocator/Pointer.h>

namespace Afina {
namespace Allocator {


Pointer::Pointer()
{
    ptr = nullptr;
}

Pointer::Pointer(point *new_ptr)
{
    if (new_ptr != nullptr)
    {
        ptr = new_ptr;
    }
    else ptr = nullptr;
}

void* Pointer::get() const
{
    if (ptr == nullptr)
    {
        return nullptr;
    }
    return ptr->first;
}

point* Pointer::get_ptr() const
{
    return ptr;
}

size_t Pointer::get_size()
{
    if(ptr == nullptr)
        return 0;

    if(ptr->first == nullptr)
        return 0;

    return static_cast<char *>(ptr->last) - static_cast<char *>(ptr->first);
}

} // namespace Allocator
} // namespace Afina
