#ifndef __SIMPLE_OBJECTS_H_
#define __SIMPLE_OBJECTS_H_

#include <string.h>
#include <mutex>

struct exception : std::exception
{
  const char*                     e_str;
public:
  exception(const char* str) : e_str(str) { };
  exception(std::string str) {
    char *cstr = new char[str.length() + 1];
    strcpy(cstr, str.c_str());
    e_str = cstr;
  };
  inline const char* what() const noexcept { return e_str; };
};

template <typename T>
class mem_cluster_t
{
private:
  unsigned int  size;
  unsigned int  free_count;
  std::mutex    stack_mtx;
  unsigned int* free_item_nums;
  T*            items;

public:
  mem_cluster_t(unsigned int size) : size(size), free_count(size), free_item_nums(new unsigned int[size]), items(new T[size]) {
    for (unsigned int i = 0; i < size; i++) {
      free_item_nums[i] = size - i - 1;
    }
  };
  ~mem_cluster_t() = default;

  void init(void func(unsigned int, T*)) {
    func(size, items);
  }

  T* get() {
    stack_mtx.lock();
    if (free_count == 0) {
      stack_mtx.unlock();
      return NULL;
    }
    free_count--;
    unsigned int num = free_item_nums[free_count];
    T* item = &items[num];
    stack_mtx.unlock();
    return item;
  }

  void put(T* item) {
    unsigned int num = (((long)item) - ((long)&items[0])) / sizeof(T);
    // так повезло, что throw exception тут не нужен

    stack_mtx.lock();
    for(unsigned int i = free_count; i < size; i++) {
      if (free_item_nums[i] == num) {
        free_item_nums[i] = free_item_nums[free_count];
        free_item_nums[free_count] = num;
        break;
      }
    }
    free_count++;
    stack_mtx.unlock();
  }
};

// NOTICE: конструкция с realloc оптимальна для случая, когда add выполняется при инициализации приложения (а нее по ходу)
template <typename T>
class pointer_map {
public:
  pointer_map() : size(0) {
    items = (T**) valloc(0);
    keys = (const char**) valloc(0);
  }

  inline void add(const char* key, T* item) {
    items = (T**) realloc(items, sizeof(T*)*(size + 1));
    items[size] = item;

    keys = (const char**) realloc(keys, sizeof(char*)*(size + 1));
    keys[size] = key;

    size++;
  }

  T* operator[](char* name) {
    for (unsigned int i = 0; i < size; i++) {
      if (strcmp(name, keys[i]) == 0) {
        return items[i];
      }
    }
    return NULL;
  }

private:
  unsigned int          size;
  T**                   items;
  const char**          keys;
};

#endif //__SIMPLE_OBJECTS_H_