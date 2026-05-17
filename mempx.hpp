#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <utility>

#define MEMPX_HEADER_DEFINED 1

#ifdef _WIN32
#ifdef MPX_EXPORTS
#define MPX_API __declspec(dllexport)
#else
#define MPX_API __declspec(dllimport)
#endif
#else
#define MPX_API
#endif

namespace mpx {
	using i8 = int8_t;
	using i16 = int16_t;
	using i32 = int32_t;
	using i64 = int64_t;
	using u8 = uint8_t;
	using u16 = uint16_t;
	using u32 = uint32_t;
	using u64 = uint64_t;
	using size = size_t;
	using isize = i64;

	struct use_allocator_t{};
	inline constexpr use_allocator_t use_allocator{};

	struct memory;

	struct iallocator{
		virtual memory allocate(size bytes)    = 0;
		virtual void   deallocate(memory& mem) = 0;
		virtual ~iallocator()                  = default;
	};

	struct heap_allocator : public iallocator{
		memory allocate(size bytes) override;
		void   deallocate(memory& mem) override;
	};

	struct virtual_allocator : public iallocator{
		memory allocate(size bytes) override;
		void   deallocate(memory& mem) override;
	};

	struct memory {
	public:
		enum struct ownership{
			owner,
			user,
			invalid
		};
		enum struct type {
			virtual_memory,
			heap_memory,
			custom,
			invalid
		};

		memory()
			:m_ownership(ownership::invalid),
			 m_type(type::invalid),
			 m_size(0),
			 m_data(nullptr),
			 m_timestamp(0),
			 m_parent(nullptr),
			 m_sliced(false){}

		memory(void* data, size size, bool is_owner = false, type mem_type = memory::type::custom, memory* parent = nullptr,bool slice_created = false)
			:m_ownership(is_owner ? ownership::owner : ownership::user),
			 m_type(mem_type),
			 m_size(size),
			 m_data(static_cast<u8*>(data)),
			 m_timestamp(time(nullptr)),
			 m_parent(parent),
			 m_sliced(slice_created){
			assert(data);
			assert(size != 0);
		}

		memory(const memory& other)
			:m_ownership(ownership::user),
			 m_type(other.m_type),
			 m_size(other.m_size),
			 m_data(other.m_data),
			 m_timestamp(other.m_timestamp),
			 m_parent(other.m_parent),
			 m_sliced(other.m_sliced){
			if(!is_owner())
				assert(parent() && !parent()->is_invalid());			
		}

		memory(memory&& other)
			:m_ownership(other.m_ownership),
			 m_type(other.m_type),
			 m_size(other.m_size),
			 m_data(other.m_data),
			 m_timestamp(other.m_timestamp),
			 m_parent(other.m_parent),
			 m_sliced(other.m_sliced){
			if(!is_owner())
				assert(parent() && !parent()->is_invalid());			
			other.reset();
		}

		memory& operator=(const memory& other){
			if(!other.is_owner() && other.m_sliced)
				assert(other.parent() && !other.parent()->is_invalid());			
			if(this != &other){
				m_ownership = ownership::user;
				m_type      = other.m_type;
				m_size      = other.m_size;
				m_data      = other.m_data;
				m_timestamp = other.m_timestamp;
				m_parent    = other.m_parent;				
				m_sliced    = other.m_sliced;
			}
			return *this;
		}

		memory& operator=(memory&& other){
			if(!other.is_owner() && other.m_sliced)
				assert(other.parent() && !other.parent()->is_invalid());			
			if(this != &other){
				m_ownership = other.m_ownership;
				m_type      = other.m_type;
				m_size      = other.m_size;
				m_data      = other.m_data;
				m_timestamp = other.m_timestamp;
				m_parent    = other.m_parent;				
				m_sliced    = other.m_sliced;
				other.reset();
			}
			return *this;
		}

		bool is_owner()   const{ return m_ownership == ownership::owner; }
		bool is_virtual() const{ return m_type == type::virtual_memory; }
		bool is_heap()    const{ return m_type == type::heap_memory; }
		bool is_custom()  const{ return m_type == type::custom; }
		bool is_invalid() const{ return m_type == type::invalid || m_ownership == ownership::invalid; }

		template<typename T = u8>
		T* at(size i){
			assert(m_data);
			if(!is_owner() && m_sliced)
				assert(parent() && !parent()->is_invalid());
			return reinterpret_cast<T*>(m_data) + i;
		}

		template<typename T = u8>
		T* as(){ return at<T>(0); }

		u8& operator[](size i){ return *at(i); }

		const u8& operator[](size i) const{
			assert(m_data);
			if(!is_owner() && m_sliced)
				assert(parent() && !parent()->is_invalid());
			return *(m_data + i);
		}

		memory* parent() const { return m_parent; }
		void* raw(){
			if(!is_owner() && m_sliced)
				assert(parent() && !parent()->is_invalid());
			return m_data;
		}
		u8*   begin() const{
			if(!is_owner() && m_sliced)
				assert(parent() && !parent()->is_invalid());
			return m_data;
		}
		u8*   end()   const{
			if(!is_owner() && m_sliced)
				assert(parent() && !parent()->is_invalid());
			return m_data ? (m_data + m_size) : nullptr;
		}
		size  bytes() const{ return m_size; }

		time_t timestamp() const{ return m_timestamp; }

		void reset(){
			m_ownership = ownership::invalid;
			m_type      = type::invalid;
			m_size      = 0;
			m_data      = nullptr;
			m_timestamp = 0;
		}

		memory slice(size offset, size length){
			assert(m_data);
			assert(offset + length <= m_size);
			if(!is_owner() && m_sliced)
				assert(parent() && !parent()->is_invalid());
			auto mem = memory{m_data + offset,length,false,m_type,this,true};
			return mem;
		}

	private:
		ownership m_ownership;
		type      m_type;
		size      m_size;
		u8*       m_data;
		time_t    m_timestamp;
		bool m_sliced;

		memory* m_parent = nullptr;
	};

	struct arena_allocator : public iallocator{
		arena_allocator(iallocator& source, size bytes)
			: m_source(&source),
			  m_backing(source.allocate(bytes)),
			  m_offset(0){}

		arena_allocator(const arena_allocator&)            = delete;
		arena_allocator& operator=(const arena_allocator&) = delete;

		arena_allocator(arena_allocator&& other)
			: m_source(other.m_source),
			  m_backing(std::move(other.m_backing)),
			  m_offset(other.m_offset){
			other.m_source = nullptr;
			other.m_offset = 0;
		}

		arena_allocator& operator=(arena_allocator&& other){
			if(this != &other){
				if(m_source && !m_backing.is_invalid())
					m_source->deallocate(m_backing);
				m_source        = other.m_source;
				m_backing       = std::move(other.m_backing);
				m_offset        = other.m_offset;
				other.m_source  = nullptr;
				other.m_offset  = 0;
			}
			return *this;
		}

		~arena_allocator(){
			if(m_source && !m_backing.is_invalid())
				m_source->deallocate(m_backing);
		}

		memory allocate(size bytes) override{
			assert(m_offset + bytes <= m_backing.bytes());
			memory mem = m_backing.slice(m_offset, bytes);
			m_offset  += bytes;
			return mem;
		}

		void deallocate(memory&) override{}

		void reset()           { m_offset = 0; }
		size used()      const { return m_offset; }
		size remaining() const { return m_backing.bytes() - m_offset; }

	private:
		iallocator* m_source;
		memory      m_backing;
		size        m_offset;
	};

	extern heap_allocator    g_heap;
	extern virtual_allocator g_virtual;

	template<typename T, typename Allocator = heap_allocator>
	struct box{
	public:
		template<typename ...Args>
		explicit box(Args&&... args)
			: m_allocator(&g_heap),
			  m_mem(m_allocator->allocate(sizeof(T))){
			new (m_mem.raw()) T{std::forward<Args>(args)...};
		}

		template<typename ...Args>
		box(use_allocator_t, Allocator& alloc, Args&&... args)
			: m_allocator(&alloc),
			  m_mem(m_allocator->allocate(sizeof(T))){
			new (m_mem.raw()) T{std::forward<Args>(args)...};
		}

		box(const box&)            = delete;
		box& operator=(const box&) = delete;

		box(box&& other)
			: m_allocator(other.m_allocator),
			  m_mem(std::move(other.m_mem)){
			other.m_allocator = nullptr;
		}

		box& operator=(box&& other){
			if(this != &other){
				if(!m_mem.is_invalid()){
					m_mem.as<T>()->~T();
					m_allocator->deallocate(m_mem);
				}
				m_allocator       = other.m_allocator;
				m_mem             = std::move(other.m_mem);
				other.m_allocator = nullptr;
			}
			return *this;
		}

		~box(){
			if(!m_mem.is_invalid()){
				m_mem.as<T>()->~T();
				m_allocator->deallocate(m_mem);
			}
		}

		T& get()        { return *raw(); }
		T* raw()        { return m_mem.as<T>(); }
		T& operator*()  { return get(); }
		T* operator->() { return raw(); }

		memory&    get_memory()   { return m_mem; }
		Allocator* get_allocator(){ return m_allocator; }

	private:
		Allocator* m_allocator;
		memory     m_mem;
	};

	template<typename T>
	box(T&&) -> box<std::decay_t<T>, heap_allocator>;

	template<typename Alloc, typename T>
	box(use_allocator_t, Alloc&, T&&) -> box<std::decay_t<T>, Alloc>;

	template<typename T, typename ...Args>
	box<T> make_box(Args&&... args){
		return box<T>{std::forward<Args>(args)...};
	}

	template<typename T, typename Allocator, typename ...Args>
	box<T, Allocator> make_box_with(Allocator& alloc, Args&&... args){
		return box<T, Allocator>{use_allocator, alloc, std::forward<Args>(args)...};
	}

	memory malloc(size bytes);
	void   free(memory& mem);
	memory vmalloc(size bytes);
	void   vfree(memory& mem);
}
