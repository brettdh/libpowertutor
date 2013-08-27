#ifndef pthread_util_incl
#define pthread_util_incl

#include <pthread.h>
#include <boost/shared_ptr.hpp>
#include <map>
#include <vector>
#include <deque>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define PTHREAD_ASSERT_SUCCESS(rc)                      \
    do {                                                \
        if (rc != 0) {                                  \
            fprintf(stderr, "PTHREAD ERROR: %s\n",      \
                    strerror(rc));                      \
            assert(0);                                  \
        }                                               \
    } while (0)


class PthreadScopedLock {
  public:
    PthreadScopedLock() : mutex(NULL) {}

    explicit PthreadScopedLock(pthread_mutex_t *mutex_) {
        mutex = NULL;
        acquire(mutex_);
    }

    void acquire(pthread_mutex_t *mutex_) {
        assert(mutex == NULL);
        assert(mutex_);
        mutex = mutex_;
        int rc = pthread_mutex_lock(mutex);
        PTHREAD_ASSERT_SUCCESS(rc);
    }
    
    ~PthreadScopedLock() {
        if (mutex) {
            release();
        }
    }
    void release() {
        int rc = pthread_mutex_unlock(mutex);
        PTHREAD_ASSERT_SUCCESS(rc);

        mutex = NULL;
    }
  private:
    pthread_mutex_t *mutex;
};



template <typename T>
class LockWrappedQueue {
  public:
    LockWrappedQueue() {
        pthread_mutex_init(&lock, NULL);
        pthread_cond_init(&cv, NULL);
    }
    size_t size() {
        PthreadScopedLock lk(&lock);
        return q.size();
    }
    bool empty() { return size() == 0; }
    void push(const T& val) {
        PthreadScopedLock lk(&lock);
        q.push_back(val);
        pthread_cond_broadcast(&cv);
    }
    void pop(T& val) {
        PthreadScopedLock lk(&lock);
        while (q.empty()) {
            pthread_cond_wait(&cv, &lock);
        }
        val = q.front();
        q.pop_front();
    }

    // iteration is not thread-safe.
    typedef typename std::deque<T>::iterator iterator;
    iterator begin() {
        return q.begin();
    }

    iterator end() {
        return q.end();
    }
  private:
    std::deque<T> q;
    pthread_mutex_t lock;
    pthread_cond_t cv;
};

/* LockWrappedMap
 *
 * Simple wrapper around an STL map that makes
 * the basic operations (insert, find, erase)
 * thread-safe.  Useful if you want any per-object
 * locks to live inside the contained objects.
 */
template <typename KeyType, typename ValueType,
          typename ordering = std::less<KeyType> >
class LockWrappedMap {
    typedef std::map<KeyType, ValueType, ordering> MapType;

  public:
    LockWrappedMap() {
        pthread_mutex_init(&membership_lock, NULL);
    }

    ~LockWrappedMap() {
        pthread_mutex_destroy(&membership_lock);
    }

    typedef typename MapType::iterator iterator;

    bool insert(const KeyType& key, const ValueType& val) {
        PthreadScopedLock lock(&membership_lock);
        std::pair<iterator, bool> ret = 
            the_map.insert(std::make_pair(key, val));
        return ret.second;
    }

    bool find(const KeyType &key, ValueType& val) {
        PthreadScopedLock lock(&membership_lock);
        typename MapType::iterator pos = the_map.find(key);
        if (pos != the_map.end()) {
            val = pos->second;
            return true;
        } else {
            return false;
        }
    }

    bool erase(const KeyType &key) {
        PthreadScopedLock lock(&membership_lock);
        return (the_map.erase(key) == 1);
    }

    /* Not thread-safe, obviously. */
    iterator begin() {
        return the_map.begin();
    }

    /* Not thread-safe, obviously. */
    iterator end() {
        return the_map.end();
    }

  private:
    MapType the_map;
    pthread_mutex_t membership_lock;
};


#endif
