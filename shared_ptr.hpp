
#pragma once

#include <cstddef>
#include <memory>
#include <utility>

namespace details {

template <typename T>
struct BaseControlBlock {
  BaseControlBlock() = default;

  explicit BaseControlBlock(T* ptr) : pointer(ptr) {}

  size_t increase_shared_count() { return ++shared_count; }
  size_t increase_weak_count() { return ++weak_count; }

  void decrease_shared_counter();
  void decrease_weak_counter();

  virtual void release_shared() {}
  virtual void release_weak() {}

  virtual ~BaseControlBlock() = default;

  T* pointer{nullptr};
  size_t shared_count{1};
  size_t weak_count{0};
};

template <typename T>
void BaseControlBlock<T>::decrease_shared_counter() {
  --shared_count;
  if (shared_count == 0) {
    release_shared();
    if (weak_count == 0) {
      release_weak();
    }
  }
}

template <typename T>
void BaseControlBlock<T>::decrease_weak_counter() {
  --weak_count;
  if (shared_count == 0 && weak_count == 0) {
    release_weak();
  }
}

template <typename T, typename Deleter = std::default_delete<T>,
          typename Allocator = std::allocator<T>>
struct ControlBlock : BaseControlBlock<T> {
  explicit ControlBlock(T* ptr, Deleter deleter = Deleter(),
                        Allocator alloc = Allocator())
      : BaseControlBlock<T>(ptr), deleter(deleter), alloc(alloc) {}

  void release_shared() override;
  void release_weak() override;

  [[no_unique_address]] Deleter deleter;
  [[no_unique_address]] Allocator alloc;
};

template <typename T, typename Deleter, typename Allocator>
void ControlBlock<T, Deleter, Allocator>::release_shared() {
  deleter(this->pointer);
  this->pointer = nullptr;
}

template <typename T, typename Deleter, typename Allocator>
void ControlBlock<T, Deleter, Allocator>::release_weak() {
  using ControlBlockAlloc = typename std::allocator_traits<
      Allocator>::template rebind_alloc<ControlBlock>;
  using ControlBlockAllocTraits = typename std::allocator_traits<
      Allocator>::template rebind_traits<ControlBlock>;

  ControlBlockAlloc control_block_alloc(alloc);
  ControlBlockAllocTraits::destroy(control_block_alloc, this);
  ControlBlockAllocTraits::deallocate(control_block_alloc, this, 1);
}

template <typename T, typename Allocator>
struct BlockVault : ControlBlock<T, std::default_delete<T>, Allocator> {
  using ControlBlockType = ControlBlock<T, std::default_delete<T>, Allocator>;

  template <typename... Args>
  BlockVault(Allocator alloc = std::allocator<T>(), Args&&... args);

  void release_shared() override;
  void release_weak() override;

  T value;
};

template <typename T, typename Allocator>
template <typename... Args>
BlockVault<T, Allocator>::BlockVault(Allocator alloc, Args&&... args)
    : value(std::forward<Args>(args)...),
      ControlBlockType(std::addressof(value), std::default_delete<T>(), alloc) {
}

template <typename T, typename Allocator>
void BlockVault<T, Allocator>::release_shared() {
  std::allocator_traits<Allocator>::destroy(ControlBlockType::alloc,
                                            std::addressof(value));
  BaseControlBlock<T>::pointer = nullptr;
}

template <typename T, typename Allocator>
void BlockVault<T, Allocator>::release_weak() {
  using ControlBlockAlloc = typename std::allocator_traits<
      Allocator>::template rebind_alloc<BlockVault>;
  using ControlBlockTraits = typename std::allocator_traits<
      Allocator>::template rebind_traits<BlockVault>;
  ControlBlockAlloc control_block_alloc(ControlBlockType::alloc);
  ControlBlockTraits::deallocate(control_block_alloc, this, 1);
}

}  // namespace details

template <typename T>
class SharedPtr {
 public:
  SharedPtr() = default;

  [[maybe_unused]] explicit SharedPtr(std::nullptr_t) {}

  template <typename Y>
    requires std::is_convertible_v<Y*, T*>
  SharedPtr(const SharedPtr<Y>& other);
  SharedPtr(const SharedPtr& other);

  template <typename Y>
    requires std::is_convertible_v<Y*, T*>
  SharedPtr(SharedPtr<Y>&& other);
  SharedPtr(SharedPtr&& other) noexcept;

  template <typename Y>
    requires std::is_convertible_v<Y*, T*>
  SharedPtr(Y* ptr);

  template <typename Y, typename Deleter>
    requires std::is_convertible_v<Y*, T*>
  explicit SharedPtr(Y* ptr, Deleter deleter);

  template <typename Y, typename Deleter, typename Allocator>
    requires std::is_convertible_v<Y*, T*>
  explicit SharedPtr(Y* ptr, Deleter deleter, Allocator alloc);

  template <typename Y>
    requires std::is_convertible_v<Y*, T*>
  SharedPtr& operator=(const SharedPtr<Y>& other);
  SharedPtr& operator=(const SharedPtr& other);

  template <typename Y>
    requires std::is_convertible_v<Y*, T*>
  SharedPtr& operator=(SharedPtr<Y>&& other);
  SharedPtr& operator=(SharedPtr&& other) noexcept;

  [[nodiscard]] int use_count() const;
  [[nodiscard]] T* get() const {
    return control_block_ == nullptr ? nullptr : control_block_->pointer;
  }

  T& operator*() { return *control_block_->pointer; }
  const T& operator*() const { return *control_block_->pointer; }

  T* operator->() { return control_block_->pointer; }
  const T* operator->() const { return control_block_->pointer; }

  void reset();

  ~SharedPtr();

 private:
  explicit SharedPtr(details::BaseControlBlock<T>* control_block)
      : control_block_(control_block) {}

  template <typename Y, typename Allocator, typename... Args>
  friend SharedPtr<Y> AllocateShared(const Allocator& alloc, Args&&... args);

  template <typename Y>
  friend class SharedPtr;

  template <typename Y>
  friend class WeakPtr;

  details::BaseControlBlock<T>* control_block_{nullptr};
};

template <typename T>
template <typename Y>
  requires std::is_convertible_v<Y*, T*>
SharedPtr<T>::SharedPtr(Y* ptr) {
  control_block_ = new details::ControlBlock<T>(ptr);
}

template <typename T>
template <typename Y>
  requires std::is_convertible_v<Y*, T*>
SharedPtr<T>::SharedPtr(const SharedPtr<Y>& other)
    : control_block_(
          dynamic_cast<details::BaseControlBlock<T>*>(other.control_block_)) {
  if (control_block_ != nullptr && control_block_->pointer != nullptr) {
    control_block_->increase_shared_count();
  }
}

template <typename T>
SharedPtr<T>::SharedPtr(const SharedPtr& other)
    : control_block_(other.control_block_) {
  if (control_block_ != nullptr && control_block_->pointer != nullptr) {
    control_block_->increase_shared_count();
  }
}

template <typename T>
template <typename Y>
  requires std::is_convertible_v<Y*, T*>
SharedPtr<T>::SharedPtr(SharedPtr<Y>&& other)
    : control_block_(
          dynamic_cast<details::BaseControlBlock<T>*>(other.control_block_)) {
  other.control_block_ = nullptr;
}

template <typename T>
SharedPtr<T>::SharedPtr(SharedPtr&& other) noexcept
    : control_block_(other.control_block_) {
  other.control_block_ = nullptr;
}

template <typename T>
template <typename Y, typename Deleter>
  requires std::is_convertible_v<Y*, T*>
SharedPtr<T>::SharedPtr(Y* ptr, Deleter deleter) {
  control_block_ =
      new details::ControlBlock<T, Deleter>(ptr, std::move(deleter));
}

template <typename T>
template <typename Y, typename Deleter, typename Allocator>
  requires std::is_convertible_v<Y*, T*>
SharedPtr<T>::SharedPtr(Y* ptr, Deleter deleter, Allocator alloc) {
  using ControlBlockType = details::ControlBlock<Y, Deleter, Allocator>;
  using ControlBlockAlloc = typename std::allocator_traits<
      Allocator>::template rebind_alloc<ControlBlockType>;
  using ControlBlockTraits = typename std::allocator_traits<
      Allocator>::template rebind_traits<ControlBlockType>;

  ControlBlockAlloc control_block_alloc(alloc);
  ControlBlockType* control_block =
      ControlBlockTraits::allocate(control_block_alloc, 1);
  try {
    ControlBlockTraits::construct(control_block_alloc, control_block, ptr,
                                  deleter, alloc);
    control_block_ = control_block;
  } catch (...) {
    deleter(ptr);
    throw;
  }
}

template <typename T>
template <typename Y>
  requires std::is_convertible_v<Y*, T*>
SharedPtr<T>& SharedPtr<T>::operator=(const SharedPtr<Y>& other) {
  if (control_block_ != nullptr) {
    control_block_->decrease_shared_counter();
  }
  control_block_ =
      dynamic_cast<details::BaseControlBlock<T>*>(other.control_block_);
  if (control_block_ != nullptr) {
    control_block_->increase_shared_count();
  }
  return *this;
}

template <typename T>
SharedPtr<T>& SharedPtr<T>::operator=(const SharedPtr& other) {
  if (this != &other) {
    if (control_block_ != nullptr) {
      control_block_->decrease_shared_counter();
    }
    control_block_ = other.control_block_;
    if (control_block_ != nullptr) {
      control_block_->increase_shared_count();
    }
  }
  return *this;
}

template <typename T>
template <typename Y>
  requires std::is_convertible_v<Y*, T*>
SharedPtr<T>& SharedPtr<T>::operator=(SharedPtr<Y>&& other) {
  if (control_block_ != nullptr) {
    control_block_->decrease_shared_counter();
  }
  control_block_ =
      dynamic_cast<details::BaseControlBlock<T>*>(other.control_block_);
  other.control_block_ = nullptr;
  return *this;
}

template <typename T>
SharedPtr<T>& SharedPtr<T>::operator=(SharedPtr&& other) noexcept {
  if (this != &other) {
    if (control_block_ != nullptr) {
      control_block_->decrease_shared_counter();
    }
    control_block_ = other.control_block_;
    other.control_block_ = nullptr;
  }
  return *this;
}

template <typename T>
SharedPtr<T>::~SharedPtr() {
  if (control_block_ != nullptr) {
    control_block_->decrease_shared_counter();
  }
}

template <typename T>
int SharedPtr<T>::use_count() const {
  if (control_block_ == nullptr) {
    return 0;
  }
  return control_block_->shared_count;
}

template <typename T>
void SharedPtr<T>::reset() {
  if (control_block_ != nullptr) {
    control_block_->decrease_shared_counter();
    control_block_ = nullptr;
  }
}

template <typename T, typename Allocator, typename... Args>
SharedPtr<T> AllocateShared(const Allocator& alloc, Args&&... args) {
  using Vault = details::BlockVault<T, Allocator>;
  using VaultAlloc =
      typename std::allocator_traits<Allocator>::template rebind_alloc<Vault>;
  using VaultAllocTraits =
      typename std::allocator_traits<Allocator>::template rebind_traits<Vault>;

  VaultAlloc vault_alloc(alloc);
  Vault* ptr = VaultAllocTraits::allocate(vault_alloc, 1);
  VaultAllocTraits::construct(vault_alloc, ptr, alloc,
                              std::forward<Args>(args)...);
  return SharedPtr<T>(dynamic_cast<details::BaseControlBlock<T>*>(ptr));
}

template <typename T, typename... Args>
SharedPtr<T> MakeShared(Args&&... args) {
  return AllocateShared<T, std::allocator<T>, Args...>(
      std::allocator<T>(), std::forward<Args>(args)...);
}

template <typename T>
class WeakPtr {
 public:
  WeakPtr() = default;

  WeakPtr(const WeakPtr& other);

  explicit WeakPtr(const SharedPtr<T>& other);

  WeakPtr(WeakPtr&& other) noexcept;

  WeakPtr& operator=(const WeakPtr& other);

  WeakPtr& operator=(WeakPtr&& other) noexcept;

  [[nodiscard]] bool expired() const {
    return control_block_->shared_count == 0;
  }

  SharedPtr<T> lock() const;

  ~WeakPtr();

 private:
  details::BaseControlBlock<T>* control_block_{nullptr};
};

template <typename T>
WeakPtr<T>::WeakPtr(const WeakPtr<T>& other)
    : control_block_(other.control_block_) {
  control_block_->increase_weak_count();
}

template <typename T>
WeakPtr<T>::WeakPtr(const SharedPtr<T>& other)
    : control_block_(other.control_block_) {
  if (control_block_ != nullptr) {
    control_block_->increase_weak_count();
  }
}

template <typename T>
WeakPtr<T>::WeakPtr(WeakPtr<T>&& other) noexcept : control_block_(other.control_block_) {
  other.control_block_ = nullptr;
}

template <typename T>
WeakPtr<T>& WeakPtr<T>::operator=(const WeakPtr<T>& other) = default;

template <typename T>
WeakPtr<T>& WeakPtr<T>::operator=(WeakPtr<T>&& other) noexcept {
  control_block_ = std::move(other.control_block_);
  return *this;
}

template <typename T>
SharedPtr<T> WeakPtr<T>::lock() const {
  if (control_block_ == nullptr || control_block_->shared_count == 0) {
    return SharedPtr<T>();
  }
  return SharedPtr<T>(control_block_);
}

template <typename T>
WeakPtr<T>::~WeakPtr() {
  if (control_block_ != nullptr) {
    control_block_->decrease_weak_counter();
  }
}
