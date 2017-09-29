#include <afina/allocator/Simple.h>
#include <afina/allocator/Pointer.h>
#include <afina/allocator/Error.h>

namespace Afina {
namespace Allocator {


Simple::Simple(void *base, size_t base_size)
{
    buff.first = base;
    buff.last = static_cast<char *>(base) + base_size - sizeof(point);
    root = static_cast<point *>(buff.last);

    root->prev = nullptr;
    root->next  = nullptr;
    root->first = nullptr;
    root->last  = nullptr;
    last_node = root;
    free_points = nullptr;
}

Pointer Simple::_alloc_root(size_t N)
{
        root->first = buff.first;
        root->last = static_cast<void *>(static_cast<char*>(buff.first) + N);
        return Pointer(root); }

Pointer Simple::_alloc_last_node(size_t N)
{
    if (free_points != nullptr) 
    {
        point *new_free_point = free_points->prev;
        free_points->prev = root;
        free_points->first = root->last; 
        root->next = free_points;
        free_points->last = static_cast<void *>(static_cast<char *>(root->last) + N);
        root = free_points;
        free_points = new_free_point;
        root->next = nullptr;
    }
    else
    {
        last_node--;
        root->next = last_node;
        last_node->prev = root;
        last_node->first = root->last; 
        last_node->last = static_cast<void*>(static_cast<char*>(root->last) + N);
        last_node->next = nullptr;
        root = last_node; 
    }
    return Pointer(root);
}

    Pointer Simple::_alloc_find_free_place(size_t N)
    {

        for(point *cur = static_cast<point *>(buff.last); cur->next != nullptr; cur = cur->next)
        {
            if((cur->next)->first != nullptr)
            {
                size_t sz = static_cast<char*>((cur->next)->first) - static_cast<char*>(cur->last);

                if(sz >= N)
                {
                    if (free_points != nullptr)
                    {
                        point *new_free_points = free_points->prev;
                        free_points->next = cur->next;
                        free_points->prev = cur; 
                        free_points->first = cur->last; 
                        free_points->last = static_cast<void*>(static_cast<char*>(cur->last) + N);

                        (cur->next)->prev = free_points;
                        point *new_one = free_points;
                        free_points = new_free_points;
                        return Pointer(new_one);
                    }
                    else
                    {
                        last_node--;
                        last_node->prev = cur;
                        last_node->next = cur->next;
                        last_node->first = cur->last;
                        last_node->last = static_cast<void*>(static_cast<char*>(cur->last) + N);
                        (cur->next)->prev = last_node;
                        cur->next = last_node;
                        return Pointer(last_node);
                    }
                }
            }
        }
        throw AllocError(AllocErrorType::NoMemory, "Try defraq");
    }

    Pointer Simple::alloc(size_t N)
    {

        if ((root->prev  == nullptr) && (root->next  == nullptr) && (root->first == nullptr) && (root->last  == nullptr) &&
            (N + sizeof(point) < static_cast<char*>(buff.last) - static_cast<char*>(buff.first)))

            return _alloc_root(N);

        if (last_node > static_cast<void*>(static_cast<char*>(last_node->last) + N + sizeof(point)))

            return _alloc_last_node(N);

        return _alloc_find_free_place(N);
    }

    void Simple::defrag()
    {
        int i = 0;
        for(point *cur = static_cast<point*>(buff.last)->next; cur != nullptr; cur = cur->next)
        {
            if(cur->first != nullptr)
            {
                if (cur->first > cur->prev->last)
                {
                    size_t sz = static_cast<char*>(cur->last) - static_cast<char*>(cur->first);
                    memcpy(cur->prev->last, cur->first, sz);
                    cur->first = cur->prev->last;
                    cur->last = static_cast<char*>(cur->first) + sz;
                }
            }
            else
            {
                cur->last = cur->prev->last;
            }
        }
    }

void Simple::realloc(Pointer &p, size_t N)
{

        void *info_link = p.get();
        size_t size = p.get_size();
        free(p);
        p = alloc(N);
        memcpy(p.get(), info_link, size);
}
        void Simple::free(Pointer& p)
        {
            point *prev = nullptr;
            point *get = p.get_ptr();

            if (get != nullptr)
            {

                if (free_points == nullptr)
                {
                    free_points = get;
                    prev = nullptr;
                }
                else
                {

                    free_points->next = get;
                    prev = free_points;
                    free_points = free_points->next;
                }

                if (free_points->next != nullptr)
                {
                    if (free_points->prev != nullptr)
                    {
                        point *next = free_points->next;
                        point *last = free_points->prev;
                        next->prev = last;
                        last->next = next;
                    }
                    else
                    {
                        (free_points->next)->prev = nullptr;
                    }
                }
                else
                {
                    if (free_points->prev != nullptr)
                    {
                        root = root->prev;
                        root->next = nullptr;
                    }
                    else
                    {
                        root->next = nullptr;
                        root->last = nullptr;
                        root->first = nullptr;
                        root->last = nullptr;
                    }
                }


                if (root != free_points)
                {
                    free_points->first = nullptr;
                    free_points->last = nullptr;
                    free_points->prev = prev;
                    free_points->next = nullptr;
                }
                else
                {
                    free_points = prev;
                    if (free_points != nullptr)
                        free_points->next = nullptr;
                }
            }

            p = nullptr;
}


//std::string Simple::dump() const { return ""; }

} // namespace Allocator
} // namespace Afina
