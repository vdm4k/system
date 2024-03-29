#include <system/thread/thread.h>

#ifdef __linux__
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)

#endif

namespace bro::system::thread {

result thread::set_name(std::string const &name) {
  result res{is_thread_running()};
  if (!res)
    return res;

  _name = name;
#ifdef __linux__
  int rc = pthread_setname_np(_thread.native_handle(), name.c_str());
  bool ok = rc == 0;
  if (!ok) {
    switch (rc) {
    case ERANGE: {
      return result{"the length of the string specified pointed to by name exceeds the "
                    "allowed limit"};
      break;
    }
    default: {
      return result{"error code not defined"};
      break;
    }
    }
  }
#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)

#endif
  return {};
}

result thread::set_affinity(std::vector<size_t> const &core_ids) {
  result res{is_thread_running()};
  if (!res)
    return res;

#ifdef __linux__
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  for (auto core_id : core_ids)
    CPU_SET(core_id, &cpuset);
  int rc = pthread_setaffinity_np(_thread.native_handle(), sizeof(cpu_set_t), &cpuset);
  bool ok = rc == 0;
  if (!ok) {
    switch (rc) {
    case EFAULT: {
      return result{"a supplied memory address was invalid"};
      break;
    }
    case EINVAL: {
      return result{"the affinity bit mask mask contains no processors "
                    "that are currently physically on the system and permitted to the "
                    "thread"};
      break;
    }
    case ESRCH: {
      return result{"no thread with the id thread could be found"};
      break;
    }
    default: {
      return result{"error code not defined"};
      break;
    }
    }
  }

#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
  return result{"unsupported functionality"};
#else
  return result{"unsupported functionality"};
#endif

  return res;
}

expected<std::vector<size_t>> thread::get_affinity() {
  expected<std::vector<size_t>> expect{is_thread_running()};
  if (!expect)
    return expect;

#ifdef __linux__
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);

  int rc = pthread_getaffinity_np(_thread.native_handle(), sizeof(cpu_set_t), &cpuset);
  bool ok = rc == 0;
  if (ok) {
    for (size_t core_id = 0; core_id < CPU_SETSIZE; ++core_id) {
      if (CPU_ISSET(core_id, &cpuset))
        expect._value.push_back(core_id);
    }
  } else {
    switch (rc) {
    case EFAULT: {
      return result{"a supplied memory address was invalid"};
      break;
    }
    case EINVAL: {
      return result{"cpusetsize is smaller than the size of the affinity mask used by "
                    "the kernel."};
      break;
    }
    case ESRCH: {
      return result{"no thread with the id thread could be found"};
      break;
    }
    default: {
      return result{"error code not defined"};
      break;
    }
    }
  }

#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
  return result{"unsupported functionality"};
#else
  return result{"unsupported functionality"};
#endif

  return expect;
}

std::thread const &thread::get_std_thread() const {
  return _thread;
}

result thread::is_thread_running() const noexcept {
  if (!is_running()) {
    return result{"thread is not in running"};
  }
  return {};
}

bool thread::set_config(config const *config) {
  if (config) {
    _config = *config;
    return true;
  }
  return false;
}

bool thread::has_config() const noexcept {
  return _config._flush_statistic || _config._call_logic_fun || _config._call_logic_on_n_loop || _config._call_sleep_on_n_loop
         || _config._call_sleep || _config._call_sleep_on_n_empty_loop_in_a_row;
}

void thread::flush_statistic() {
  copy_statistic(&_prev_statistic, &_actual_statistic);
  _actual_statistic = {};
}

void thread::copy_statistic(statistic *to, statistic *from) noexcept {
  bool expect{false};
  while (!_write_stat.compare_exchange_strong(expect, true))
    ;
  memcpy(to, from, sizeof(statistic));
  _write_stat.store(false, std::memory_order_release);
}

statistic thread::get_statistic() noexcept {
  statistic stat;
  copy_statistic(&stat, &_prev_statistic);
  return stat;
}

std::pair<uint64_t, std::chrono::microseconds> thread::get_need_sleep(std::chrono::microseconds const start) const noexcept {
  uint64_t call_sleep_on_n_loop{0};
  std::chrono::microseconds call_sleep_on_time{0};
  if (_config._sleep) {
    call_sleep_on_n_loop = _config._call_sleep_on_n_loop ? *_config._call_sleep_on_n_loop : 0;
    call_sleep_on_time = _config._call_sleep ? start + *_config._call_sleep : std::chrono::microseconds{0};
  }
  return {call_sleep_on_n_loop, call_sleep_on_time};
}

std::pair<uint64_t, std::chrono::microseconds> thread::get_need_call_logic(std::chrono::microseconds const start) const noexcept {
  uint64_t call_logic_on_n_loop{0};
  std::chrono::microseconds call_logic_fun{0};
  if (_config._call_logic_on_n_loop || _config._call_logic_fun) {
    call_logic_on_n_loop = _config._call_logic_on_n_loop ? *_config._call_logic_on_n_loop : 0;
    call_logic_fun = _config._call_logic_fun ? start + *_config._call_logic_fun : std::chrono::microseconds{0};
  }
  return {call_logic_on_n_loop, call_logic_fun};
}

std::chrono::microseconds thread::get_flush_stat(std::chrono::microseconds start_tsc) {
  return _config._flush_statistic ? start_tsc + *_config._flush_statistic : std::chrono::microseconds{0};
}

} // namespace bro::system::thread
