#ifndef FLAT_SET_HPP
#define FLAT_SET_HPP

// Taken from https://github.com/Fumbo/flat_set/tree/master

#include <vector>
#include <utility>
#include <functional>
#include <memory>
#include <initializer_list>
#include <algorithm>

namespace dawn {
template <typename Key, typename Compare = std::less<Key>,
          typename Allocator = std::allocator<Key>>
class flat_set {
 private:
  std::vector<Key> elts_;

 public:
  /* Types */
  typedef Key               key_type;
  typedef Key               value_type;
  typedef Compare           key_compare;
  typedef Compare           value_compare;
  typedef Allocator         allocator_type;
  typedef value_type&       reference;
  typedef const value_type& const_reference;

  typedef typename std::vector<Key>::iterator         iterator;
  typedef typename std::vector<Key>::const_iterator   const_iterator;
  typedef typename std::vector<Key>::reverse_iterator reverse_iterator;
  typedef
      typename std::vector<Key>::const_reverse_iterator const_reverse_iterator;

  typedef size_t                                             size_type;
  typedef ptrdiff_t                                          difference_type;
  typedef typename std::allocator_traits<Allocator>::pointer pointer;
  typedef
      typename std::allocator_traits<Allocator>::const_pointer const_pointer;

  /* CONSTRUCTORS */
  explicit flat_set(const Compare&   comp  = Compare(),
                    const Allocator& alloc = Allocator());
  explicit flat_set(const Allocator& alloc);
  template <class InputIt>
  flat_set(InputIt first, InputIt last, const Compare& comp = Compare(),
           const Allocator& alloc = Allocator());
  flat_set(const flat_set& other);
  flat_set(const flat_set& other, const Allocator& alloc);
  flat_set(flat_set&& other);
  flat_set(flat_set&& other, const Allocator& alloc);
  flat_set(std::initializer_list<value_type> init,
           const Compare&                    comp  = Compare(),
           const Allocator&                  alloc = Allocator());

  /* DESTRUCTOR */
  ~flat_set();

  /* OPERATOR EQUALS */
  flat_set& operator=(const flat_set& other);
  flat_set& operator=(flat_set&& other);
  flat_set& operator=(std::initializer_list<value_type> ilist);

  /* ITERATORS */
  iterator       begin();
  const_iterator begin() const;
  const_iterator cbegin() const;

  iterator       end();
  const_iterator end() const;
  const_iterator cend() const;

  reverse_iterator       rbegin();
  const_reverse_iterator rbegin() const;
  const_reverse_iterator crbegin() const;

  reverse_iterator       rend();
  const_reverse_iterator rend() const;
  const_reverse_iterator crend() const;

  /* CAPACITY */
  bool      empty() const;
  size_type size() const;
  size_type max_size() const;

  /* MODIFIERS */
  void clear();

  std::pair<iterator, bool> insert(const value_type& value);
  std::pair<iterator, bool> insert(value_type&& value);
  iterator                  insert(iterator hint, const value_type& value);
  iterator insert(const_iterator hint, const value_type& value);
  iterator insert(const_iterator hint, value_type&& value);
  template <class InputIt>
  void insert(InputIt first, InputIt last);
  void insert(std::initializer_list<value_type> ilist);

  template <class... Args>
  std::pair<iterator, bool> emplace(Args&&... args);
  template <class... Args>
  iterator emplace_hint(const_iterator hint, Args&&... args);

  void      erase(iterator position);
  iterator  erase(const_iterator position);
  void      erase(iterator first, iterator last);
  iterator  erase(const_iterator first, const_iterator last);
  size_type erase(const key_type& key);

  void swap(flat_set& other);

  /* LOOKUP */
  size_type count(const Key& key) const;

  iterator       find(const Key& key);
  const_iterator find(const Key& key) const;

  std::pair<iterator, iterator>             equal_range(const Key& key);
  std::pair<const_iterator, const_iterator> equal_range(const Key& key) const;

  iterator       lower_bound(const Key& key);
  const_iterator lower_bound(const Key& key) const;

  iterator       upper_bound(const Key& key);
  const_iterator upper_bound(const Key& key) const;

  /* OBSERVATORS */
  key_compare   key_comp() const;
  value_compare value_comp() const;

  template <class K, class C, class A>
  friend bool operator==(const flat_set<K, C, A>& lhs,
                         const flat_set<K, C, A>& rhs);
  template <class K, class C, class A>
  friend bool operator!=(const flat_set<K, C, A>& lhs,
                         const flat_set<K, C, A>& rhs);
  template <class K, class C, class A>
  friend bool operator<(const flat_set<K, C, A>& lhs,
                        const flat_set<K, C, A>& rhs);
  template <class K, class C, class A>
  friend bool operator<=(const flat_set<K, C, A>& lhs,
                         const flat_set<K, C, A>& rhs);
  template <class K, class C, class A>
  friend bool operator>(const flat_set<K, C, A>& lhs,
                        const flat_set<K, C, A>& rhs);
};

template <class Key, class Compare, class Alloc>
bool operator==(const flat_set<Key, Compare, Alloc>& lhs,
                const flat_set<Key, Compare, Alloc>& rhs);
template <class Key, class Compare, class Alloc>
bool operator!=(const flat_set<Key, Compare, Alloc>& lhs,
                const flat_set<Key, Compare, Alloc>& rhs);
template <class Key, class Compare, class Alloc>
bool operator<(const flat_set<Key, Compare, Alloc>& lhs,
               const flat_set<Key, Compare, Alloc>& rhs);
template <class Key, class Compare, class Alloc>
bool operator<=(const flat_set<Key, Compare, Alloc>& lhs,
                const flat_set<Key, Compare, Alloc>& rhs);
template <class Key, class Compare, class Alloc>
bool operator>(const flat_set<Key, Compare, Alloc>& lhs,
               const flat_set<Key, Compare, Alloc>& rhs);
template <class Key, class Compare, class Alloc>
bool operator>=(const flat_set<Key, Compare, Alloc>& lhs,
                const flat_set<Key, Compare, Alloc>& rhs);

template <class Key, class Compare, class Alloc>
void swap(flat_set<Key, Compare, Alloc>& lhs,
          flat_set<Key, Compare, Alloc>& rhs);

// CONSTRUCTORS
template <typename K, typename C, typename A>
inline flat_set<K, C, A>::flat_set(const C&, const A&)
    : elts_(std::vector<K, A>()) {}

template <typename K, typename C, typename A>
inline flat_set<K, C, A>::flat_set(const A& alloc)
    : elts_(std::vector<K, A>(alloc)) {}

template <typename K, typename C, typename A>
template <class InputIt>
inline flat_set<K, C, A>::flat_set(InputIt first, InputIt last, const C&,
                                   const A&)
    : elts_(std::vector<K, A>(first, last)) {}

template <typename K, typename C, typename A>
inline flat_set<K, C, A>::flat_set(const flat_set& other)
    : elts_(std::vector<K, A>(other.elts_, allocator_type())) {}

template <typename K, typename C, typename A>
inline flat_set<K, C, A>::flat_set(const flat_set& other, const A& alloc)
    : elts_(std::vector<K, A>(other.elts_, alloc)) {}

template <typename K, typename C, typename A>
inline flat_set<K, C, A>::flat_set(flat_set&& other)
    : elts_(move(other.elts_)) {}

template <typename K, typename C, typename A>
inline flat_set<K, C, A>::flat_set(flat_set&& other, const A& alloc)
    : elts_(std::vector<K, A>(other.elts_, alloc)) {}

template <typename K, typename C, typename A>
inline flat_set<K, C, A>::flat_set(std::initializer_list<value_type> init,
                                   const C&, const A&) {
  insert(init);
}

/* DESTRUCTOR */
template <typename K, typename C, typename A>
inline flat_set<K, C, A>::~flat_set() {}

/* OPERATOR EQUALS */
template <typename K, typename C, typename A>
flat_set<K, C, A>& flat_set<K, C, A>::operator=(const flat_set& other) {
  elts_ = other.elts_;
  return *this;
}

template <typename K, typename C, typename A>
flat_set<K, C, A>& flat_set<K, C, A>::operator=(flat_set&& other) {
  elts_ = other.elts_;
  return *this;
}

template <typename K, typename C, typename A>
flat_set<K, C, A>& flat_set<K, C, A>::operator=(
    std::initializer_list<value_type> ilist) {
  // TODO
  elts_ = vector<K, A>(ilist);
  return *this;
}

// ITERATORS
template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::begin() -> iterator {
  return elts_.begin();
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::begin() const -> const_iterator {
  return elts_.begin();
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::cbegin() const -> const_iterator {
  return elts_.cbegin();
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::end() -> iterator {
  return elts_.end();
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::end() const -> const_iterator {
  return elts_.end();
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::cend() const -> const_iterator {
  return elts_.cend();
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::rbegin() -> reverse_iterator {
  return elts_.rbegin();
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::rbegin() const -> const_reverse_iterator {
  return elts_.rbegin();
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::crbegin() const -> const_reverse_iterator {
  return elts_.crbegin();
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::rend() -> reverse_iterator {
  return elts_.rend();
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::rend() const -> const_reverse_iterator {
  return elts_.rend();
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::crend() const -> const_reverse_iterator {
  return elts_.crend();
}

template <typename K, typename C, typename A>
inline bool flat_set<K, C, A>::empty() const {
  return elts_.empty();
}

template <typename K, typename C, typename A>
inline size_t flat_set<K, C, A>::size() const {
  return elts_.size();
}

template <typename K, typename C, typename A>
inline size_t flat_set<K, C, A>::max_size() const {
  return elts_.max_size();
}

/* MODIFIERS */
template <typename K, typename C, typename A>
inline void flat_set<K, C, A>::clear() {
  elts_.clear();
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::insert(const K& value)
    -> std::pair<iterator, bool> {
  if (elts_.begin() == elts_.end()) {
    elts_.push_back(value);
    return std::pair<iterator, bool>(elts_.begin(), true);
  }

  iterator low;
  low = std::lower_bound(elts_.begin(), elts_.end(), value);

  if (*low != value) {
    elts_.insert(low, value);
    return std::pair<iterator, bool>(low, true);
  }
  return std::pair<iterator, bool>(low, false);
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::insert(K&& value) -> std::pair<iterator, bool> {
  if (elts_.begin() == elts_.end()) {
    elts_.push_back(value);
    return std::pair<iterator, bool>(elts_.begin(), true);
  }

  iterator low;
  low = std::lower_bound(elts_.begin(), elts_.end(), value, key_compare());
  if (*low != value) {
    elts_.insert(low, value);
    return std::pair<iterator, bool>(low, true);
  }
  return std::pair<iterator, bool>(low, false);
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::insert(iterator hint, const K& value)
    -> iterator {
  if (elts_.begin() == elts_.end()) {
    elts_.push_back(value);
    return elts_.begin();
  }

  if (find(value) != elts_.end()) return elts_.end();
  elts_.insert(hint, value);
  if (is_sorted(elts_.begin(), elts_.end(), key_compare()))
    return hint;
  else {
    sort(elts_.begin(), elts_.end(), key_compare());
    return find(value);
  }
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::insert(const_iterator hint, const K& value)
    -> iterator {
  if (elts_.begin() == elts_.end()) {
    elts_.push_back(value);
    return elts_.begin();
  }

  if (find(value) != elts_.end()) return elts_.end();
  elts_.insert(hint, value);
  if (is_sorted(elts_.begin(), elts_.end(), key_compare()))
    return hint;
  else {
    sort(elts_.begin(), elts_.end(), key_compare());
    return find(value);
  }
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::insert(const_iterator hint, K&& value)
    -> iterator {
  if (elts_.begin() == elts_.end()) {
    elts_.push_back(value);
    return elts_.begin();
  }
  if (find(value) != elts_.end()) return elts_.end();
  elts_.insert(hint, value);
  if (is_sorted(elts_.begin(), elts_.end(), key_compare()))
    return hint;
  else {
    sort(elts_.begin(), elts_.end(), key_compare());
    return find(value);
  }
}

template <typename K, typename C, typename A>
template <class InputIt>
inline void flat_set<K, C, A>::insert(InputIt first, InputIt last) {
  InputIt it = first;
  while (it != last) {
    insert(*it);
    it++;
  }
}

template <typename K, typename C, typename A>
inline void flat_set<K, C, A>::insert(std::initializer_list<value_type> ilist) {
  for (auto it : ilist) insert(it);
}

template <typename K, typename C, typename A>
template <class... Args>
inline auto flat_set<K, C, A>::emplace(Args&&... args)
    -> std::pair<iterator, bool> {
  return insert(K(forward<Args>(args)...));
}

template <typename K, typename C, typename A>
template <class... Args>
inline auto flat_set<K, C, A>::emplace_hint(const_iterator hint, Args&&... args)
    -> iterator {
  return insert(K(forward<Args>(args)...));
}

template <typename K, typename C, typename A>
void flat_set<K, C, A>::erase(iterator position) {
  elts_.erase(position);
}

template <typename K, typename C, typename A>
auto flat_set<K, C, A>::erase(const_iterator position) -> iterator {
  return elts_.erase(position);
}

template <typename K, typename C, typename A>
void flat_set<K, C, A>::erase(iterator first, iterator last) {
  elts_.erase(first, last);
}

template <typename K, typename C, typename A>
auto flat_set<K, C, A>::erase(const_iterator first, const_iterator last)
    -> iterator {
  elts_.erase(first, last);
}

template <typename K, typename C, typename A>
auto flat_set<K, C, A>::erase(const key_type& key) -> size_type {
  auto it = find(key);
  if (it != elts_.end()) elts_.erase(it);
  return elts_.size();
}

template <typename K, typename C, typename A>
void flat_set<K, C, A>::swap(flat_set& other) {
  flat_set tmp = flat_set(*this);
  *this        = other;
  other        = tmp;
}

/* LOOKUP */
template <typename K, typename C, typename A>
inline size_t flat_set<K, C, A>::count(const K& key) const {
  if (find(key) != elts_.end())
    return 1;
  else
    return 0;
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::find(const K& key) -> iterator {
  auto it = std::lower_bound(elts_.begin(), elts_.end(), key, key_compare());
  if (it != elts_.end() && *it == key)
    return it;
  else
    return elts_.end();
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::find(const K& key) const -> const_iterator {
  auto it = std::lower_bound(elts_.cbegin(), elts_.cend(), key, key_compare());
  if (it != elts_.end() && *it == key)
    return it;
  else
    return elts_.end();
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::equal_range(const K& key)
    -> std::pair<iterator, iterator> {
  auto it = find(key);
  if (it != elts_.begin())
    return pair<iterator, iterator>(it, it);
  else
    return pair<iterator, iterator>(elts_.end(), elts_.begin());
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::equal_range(const K& key) const
    -> std::pair<const_iterator, const_iterator> {
  auto it = find(key);
  if (it != elts_.begin())
    return pair<const_iterator, const_iterator>(it, it);
  else
    return pair<const_iterator, const_iterator>(elts_.end(), elts_.begin());
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::lower_bound(const K& key) -> iterator {
  return std::lower_bound(elts_.begin(), elts_.end(), key, key_compare());
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::lower_bound(const K& key) const
    -> const_iterator {
  return std::lower_bound(elts_.cbegin(), elts_.cend(), key, key_compare());
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::upper_bound(const K& key) -> iterator {
  return std::upper_bound(elts_.begin(), elts_.end(), key, key_compare());
}

template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::upper_bound(const K& key) const
    -> const_iterator {
  return std::upper_bound(elts_.cbegin(), elts_.cend(), key, key_compare());
}

/* OBSERVATORS */
template <typename K, typename C, typename A>
inline auto flat_set<K, C, A>::key_comp() const -> key_compare {
  return key_compare();
}

template <class K, class C, class A>
bool operator==(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs) {
  return lhs.elts_ == rhs.elts_;
}

template <class K, class C, class A>
bool operator!=(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs) {
  return lhs.elts_ != rhs.elts_;
}

template <class K, class C, class A>
bool operator<(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs) {
  return lhs.elts_ < rhs.elts_;
}

template <class K, class C, class A>
bool operator<=(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs) {
  return lhs.elts_ <= rhs.elts_;
}

template <class K, class C, class A>
bool operator>(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs) {
  return lhs.elts_ > rhs.elts_;
}

template <class K, class C, class A>
bool operator>=(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs) {
  return lhs.elts_ >= rhs.elts_;
}

template <class K, class C, class A>
void swap(flat_set<K, C, A>& lhs, flat_set<K, C, A>& rhs) {
  return lhs.swap(rhs);
}

}  // namespace dawn

#endif /* FLAT_flat_set_HH */
