// copied/derived from https://stackoverflow.com/questions/22487267/unable-to-use-custom-allocator-with-allocate-shared-make-shared
#include <memory>
#include <stack>
#include <mutex>
#include <vector>
#include <type_traits>

template<typename T>
struct CachingAllocator
{
   typedef std::size_t size_type;
   typedef std::ptrdiff_t difference_type;
   typedef T* pointer;
   typedef const T* const_pointer;
   typedef T& reference;
   typedef const T& const_reference;
   typedef T value_type;
   
   typedef T *value_type_ptr;
   
   int cntallocs = 0;
   int cntdeallocs = 0;

   template<typename U>
     struct rebind 
     {
	typedef CachingAllocator<U> other;
     }
   ;
   
//   CachingAllocator() throw() 
//     {
//     }
   ;
   CachingAllocator(const CachingAllocator& other) throw() 
     {
	printf("CA ctor 1 this @ %p\n", this);
     }
   ;
   
   template<typename U>
     CachingAllocator(const CachingAllocator<U>& other) throw() 
       {
	printf("CA ctor 2 this @ %p\n", this);
       }
   ;
   
   template<typename U>
     CachingAllocator& operator = (const CachingAllocator<U>& other)
       {
	printf("CA oper= 1 this @ %P\n", this);
	  return *this; 
       }
   
   CachingAllocator<T>& operator = (const CachingAllocator& other) 
     {
	printf("CA op= 2 this @ %p\n", this);
	return *this; 
     }
   
   ~CachingAllocator() 
     {
	printf("CA Dtor this @ %p\n", this);
     }
   
   
   

    std::mutex mtx_;
    std::stack<T,std::vector<value_type_ptr>> cache;

    CachingAllocator() noexcept 
    {	printf("CA ctor 3 this @ %p\n", this);
    };

//    template<typename U>
//    CachingAllocator(const CachingAllocator<U>& other) throw() {};
   
    size_t item_sz_ = 0;

    T* allocate(std::size_t n, const void* hint = 0)
    {
        //training wheels...
        //this implementation will only work for one specific fixed sz of item
        if(item_sz_)
	{
	   if(item_sz_ != n * sizeof(T))
	      return nullptr;
	}
        else
	{
	   item_sz_ = n * sizeof(T);
	}
       
        T* retval;

        std::lock_guard<decltype(mtx_)> lg(mtx_);
        if(!cache.empty())
	{
	   retval = cache.top();
	   cache.pop();
	}
        else
	{
	   //retval = static_cast<value_type_ptr>(::new char[n * sizeof(T)]);
	   //retval = static_cast<value_type_ptr>(::new (n * sizeof(T)));
	   retval = static_cast<value_type_ptr>(std::malloc (n * sizeof(T)));
	   //retval = ::new T(n * sizeof(T));
	}
       printf("alloc'd @%p, cntallocs %d\n", retval, ++cntallocs);
       return retval;
    }

    void deallocate(T* ptr, size_t n)
    {
       printf("'delete' @%p, cntdeallocs %d (cntallocs %d)\n", ptr, ++cntdeallocs, cntallocs);
        //::operator delete(ptr);
        std::lock_guard<decltype(mtx_)> lg(mtx_);
        cache.push(ptr);
       printf("stack sz %lu\n",cache.size());
    }
};

#if 0
template <typename T, typename U>
inline bool operator == (const CachingAllocator<T>&, const CachingAllocator<U>&)
{
    return true;
}

template <typename T, typename U>
inline bool operator != (const CachingAllocator<T>& a, const CachingAllocator<U>& b)
{
    return !(a == b);
}
#endif