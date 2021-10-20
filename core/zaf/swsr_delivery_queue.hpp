#pragma once

#include <atomic>
#include <iostream>
#include <thread>
#include <type_traits>
#include <vector>

#include "zaf/zaf_exception.hpp"

namespace zaf {
enum SWSRDeliveryQueueFullStrategy {
  Blocking,
  // Resize is only available for sending message to self
  // unless the buffer can be resized with thread-safety guarantee
  Resize,
  // give up the message if the queue is full
  Giveup,
};

/**
 * Single writer single reader shared queue for message delivery.
 * 1. need to ensure push happen-before inc_write_progress
 * 2. need to ensure one read happen-after inc_read_progress; basically inc_read_progress happen-before read
 * 3. writer needs not to see the latest read_idx, i.e., if it sees an old read_idx and the queue is full, the writer will block-and-retry or resize
 * 4. reader needs to see the latest write_idx, otherwise reader will read nothing from the queue
 *    while the writer may not notify reader in future if the writer is blocked for writing.
 * 5. write_progress and read_progress (i.e., may_have_message flag) needs to be up-to-date to both writer and reader
 */
template<typename Item>
class SWSRDeliveryQueue {
private:
  // atomic should be used because `write_idx`, `read_idx` and `may_have_message`
  // are accessed by two threads and the writes on them by one thread must be
  // visible to the other thread. Note: volatile does not help.
  // items[write_idx & bit_mask] is the one that can be used to write the next item in case the queue is not full
  // items[read_idx & bit_mask] is the one that can be read in case the queue is not empty
  std::atomic<unsigned> write_idx{0};
  std::atomic<unsigned> read_idx{0};
  std::atomic<bool> may_have_message{false};
  // `cap` must be 2 to a power of x
  unsigned cap = 0;
  // `bit_mask` must be `cap - 1`
  unsigned bit_mask = 0;
  // the buffer with len `cap`
  std::vector<Item> items;

public:
  // only accessed by the reader
  // when `num_empty_read` equals to `max_empty_read`, reader changes `may_have_message` to false
  unsigned num_empty_read = 0;
  bool is_writing_by_sender = true;
  bool is_reading_by_reader = false;

  // configurations
  unsigned max_messages_read = 100;
  unsigned max_empty_read = 100;

  SWSRDeliveryQueue() = default;

  SWSRDeliveryQueue(SWSRDeliveryQueue<Item>&& other) {
    this->write_idx.store(other.write_index());
    this->read_idx.store(other.read_index());
    this->may_have_message.store(other.may_have_message.load());
    this->cap = other.cap;
    this->bit_mask = other.bit_mask;
    this->items = std::move(other.items);
    other.clear();
  }

  SWSRDeliveryQueue& operator=(SWSRDeliveryQueue<Item>&& other) {
    this->write_idx.store(other.write_index());
    this->read_idx.store(other.read_index());
    this->may_have_message.store(other.may_have_message.load());
    this->cap = other.cap;
    this->bit_mask = other.bit_mask;
    this->items = other.items;
    other.clear();
    return *this;
  }

  void clear() {
    this->write_idx.store(0, std::memory_order_release);
    this->read_idx.store(0, std::memory_order_release);
    this->may_have_message.store(false, std::memory_order_release);
    this->cap = 0;
    this->bit_mask = 0;
    this->items.clear();
  }

  constexpr static unsigned max_empty_read_timespan_ms() {
    return 10u;
  }

  // increase the writer progress if the writer is not ahead of the reader
  // mark that the writer has somehow notified the reader one more time
  // only writer can call
  inline bool inc_write_progress() {
    return !may_have_message.exchange(true, std::memory_order_acq_rel);
  }

  // mark that the reader get notified one more time
  // only reader can call
  inline void inc_read_progress() {
    may_have_message.store(false, std::memory_order_acq_rel);
  }

  inline bool can_read() const {
    return may_have_message.load(std::memory_order_acq_rel);
  }

  // whether the queue is too full that writer cannot push anything
  inline bool full() const {
    return write_index() - read_index() == cap;
  }

  // whether the reader can get anything from the queue
  inline bool empty() const {
    return write_index() == read_index();
  }

  // the number of items
  inline unsigned size() const {
    return write_index() - read_index();
  }

  inline unsigned write_index(std::memory_order o = std::memory_order_acquire) const {
    return write_idx.load(o);
  }

  inline unsigned read_index(std::memory_order o = std::memory_order_acquire) const {
    return read_idx.load(o);
  }

  // set the capacity of the queue to 2^scale
  inline void resize(unsigned scale) {
    this->cap = 1u << scale;
    items.resize(this->cap);
    bit_mask = cap - 1;
  }

  inline unsigned capacity() const {
    return cap;
  }

  // writer push an item
  // only writer can call
  template<typename U>
  inline bool push(U&& elem, SWSRDeliveryQueueFullStrategy s = Blocking) {
    auto w = write_index();
    if (w - cap == read_index()) {
      // the queue is full
      switch (s) {
        case Blocking: {
          // for sending messages to others
          while (w - cap == read_index()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
          break;
        }
        // Note(zzxx):
        // To use Resize in two-thread-context:
        // 1. change std::vector to std::deque, such that reading std::deque remains valid while resizing
        // 2. change std::move to std::copy such that reader either reads old or new data, depending on whether the reader gets the old bit_mask or the new one
        // 3. the content stored in the queue should be trivally copyable, e.g., pointer
        case Resize: {
          // for sending messages to self
          items.resize(this->cap << 1);
          std::move(items.begin(), items.begin() + (w & bit_mask), items.begin() + this->cap);
          this->cap <<= 1;
          this->bit_mask = this->cap - 1;
          break;
        }
        case Giveup: {
          return false;
        }
        default: {
          throw ZAFException("Unknown SWSRDeliveryQueueFullStrategy: ", s);
        }
      }
    }
    items[w & bit_mask] = std::forward<U>(elem);
    write_idx.fetch_add(1, std::memory_order_release);
    return true;
  }

  // Note: ensure the queue is NOT empty()
  // only reader can call
  inline Item& top() {
    return items[read_index(std::memory_order_relaxed) & bit_mask];
  }

  // Note: ensure the queue is NOT empty()
  // only reader can call
  template<typename PopHandler>
  inline void pop_one(PopHandler&& handler) {
    auto& item = top();
    handler(item);
    read_idx.fetch_add(1, std::memory_order_release);
  }

  // Note: ensure the queue is NOT empty()
  // only reader can call
  inline void pop_one() {
    pop_one([](auto&&){});
  }

  // Notifier is a customized method for telling the reader that there is a new message to read
  template<typename U, typename Notifier>
  inline bool push(U&& elem, Notifier notify_reader, SWSRDeliveryQueueFullStrategy s = Blocking) {
    if (push(std::forward<U>(elem), s)) {
      if (inc_write_progress()) {
        // the `may_have_message` changes from false to true, it means the reader is not reading
        notify_reader();
      }
      return true;
    }
    return false;
  }

  // Read some messages that are immediately available in the queue.
  // The read loops until the number of messages that are read reaches the maximum,
  // or there is no available message in the queue.
  // The number of messages to be read will be no more than `max_messages_read`.
  // Return: the number of messages that have been read
  template<typename PopHandler>
  inline unsigned pop_some(PopHandler&& handler, unsigned max_messages_read) {
    unsigned num_read = 0;
    while (num_read < max_messages_read) {
      auto w = write_index();
      auto r = read_index(std::memory_order_relaxed);
      if (r == w) {
        // empty queue at the moment
        break;
      }
      for (auto i = r; i < w && num_read < max_messages_read; num_read++, i++) {
        // update `read_idx` for each message so that the writer can find available slot when the slot becomes empty
        pop_one(handler);
      }
    }
    return num_read;
  }

  template<typename PopHandler>
  inline unsigned pop_some(PopHandler&& handler) {
    return this->pop_some(std::forward<PopHandler>(handler), this->max_messages_read);
  }

  // OnReachMaxEmptyRead: action to take for stopping the reader from reading the queue when the queue is empty
  template<typename PopHandler, typename OnReachMaxEmptyRead,
    typename std::enable_if_t<std::is_invocable_v<OnReachMaxEmptyRead>>* = nullptr>
  inline void pop_some(PopHandler&& handler, OnReachMaxEmptyRead&& on_reach_max_empty_read, unsigned max_messages_read) {
    if (0 == pop_some(handler, max_messages_read)) {
      if (++num_empty_read == max_empty_read) {
        // the reader stops reading this queue until the writer notifies it again
        inc_read_progress();
        // read once more in case the writer writes any when `inc_read_progress`
        pop_some(handler, max_messages_read);
        num_empty_read = 0;
        on_reach_max_empty_read();
      }
    } else {
      num_empty_read = 0;
    }
  }

  template<typename PopHandler, typename OnReachMaxEmptyRead>
  inline void pop_some(PopHandler&& handler, OnReachMaxEmptyRead&& on_reach_max_empty_read) {
    this->pop_some(std::forward<PopHandler>(handler), std::forward<OnReachMaxEmptyRead>(on_reach_max_empty_read), this->max_empty_read);
  }

  friend std::ostream& operator<<(std::ostream& out, const SWSRDeliveryQueue<Item>& item) {
    out << "SWSRDeliveryQueue{"
        << &item
        << ", size: " << item.size()
        << ", write idx: " << (item.write_index() & item.bit_mask)
        << ", read idx: " << (item.read_index() & item.bit_mask)
        << ", empty read: " << item.num_empty_read
        << ", can read: " << item.can_read()
        << "}";
    return out;
  }
};
} // namespace zaf
