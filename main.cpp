//
// Created by sheyme on 13/05/25.
//

#include <iostream>

#include "utils/memory_utils.hpp"
#include "utils/utils.hpp"

size_t MemoryManager::type_new_allocated = 0;
size_t MemoryManager::type_new_deleted = 0;
size_t MemoryManager::allocator_allocated = 0;
size_t MemoryManager::allocator_deallocated = 0;
size_t MemoryManager::allocator_constructed = 0;
size_t MemoryManager::allocator_destroyed = 0;

template <typename T, bool PropagateOnConstruct, bool PropagateOnAssign>
size_t
    WhimsicalAllocator<T, PropagateOnConstruct, PropagateOnAssign>::counter = 0;

size_t Accountant::ctor_calls = 0;
size_t Accountant::dtor_calls = 0;

size_t Base::base_created = 0;
size_t Base::base_destroyed = 0;
size_t Derived::derived_created = 0;
size_t Derived::derived_destroyed = 0;

bool ThrowingAccountant::need_throw = false;

void SetupTest() {
  MemoryManager::type_new_allocated = 0;
  MemoryManager::type_new_deleted = 0;
  MemoryManager::allocator_allocated = 0;
  MemoryManager::allocator_deallocated = 0;
  MemoryManager::allocator_constructed = 0;
  MemoryManager::allocator_destroyed = 0;
  Accountant::reset();
  Base::reset();
  Derived::reset();
}

//#include "smart_pointers.hpp"
#include "shared_ptr.hpp"

int main() {
  SetupTest();

  size_t deleter_calls = 0;
  auto custom_deleter = [&deleter_calls]<typename T>(T*) {++deleter_calls; };

  Accountant acc;
  {
    AllocatorWithCount<Accountant> alloc;
    SharedPtr<Accountant> ptr(&acc, custom_deleter, alloc);
    auto moved_ptr = std::move(ptr);
    auto copy_ptr = moved_ptr;
    moved_ptr = MakeShared<Accountant>();
  }

  std::cout << (MemoryManager::allocator_allocated > 0);
  std::cout << (MemoryManager::allocator_allocated == MemoryManager::allocator_deallocated);
  std::cout << (Accountant::ctor_calls == 2);
  std::cout << (Accountant::dtor_calls == 1);
  std::cout << (deleter_calls == 1);


}