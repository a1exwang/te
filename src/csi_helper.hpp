#pragma once
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace te {


// parse format 123;444;;22
std::vector<int> parse_csi_colon_ints(const std::vector<uint8_t> &seq, int start, int end, std::optional<int> default_value = 0) {
  if (start >= end) {
    if (default_value.has_value()) {
      return std::vector<int>(1, default_value.value());
    } else {
      return {};
    }
  }
  std::string s;
  std::vector<int> result;
  for (int i = start; i < end; i++) {
    if (std::isdigit(seq[i])) {
      s.push_back(seq[i]);
    } else if (seq[i] == ';') {
      if (s.empty()) {
        if (default_value.has_value()) {
          result.push_back(default_value.value());
        } else {
          result.push_back(0);
        }
      } else {
        result.push_back(std::stoi(s));
      }
      s.clear();
    }
  }
  if (s.empty()) {
    result.push_back(0);
  } else {
    result.push_back(std::stoi(s));
  }
  return result;
}

template <typename>
struct TupleAddOne;

template <typename... First>
struct TupleAddOne<std::tuple<First...>> {
using type = std::tuple<First..., int>;
};

template <int N>
struct NTuple {
  using type = typename TupleAddOne<typename NTuple<N-1>::type>::type;
};

template <>
struct NTuple<0> {
  using type = std::tuple<>;
};

struct InvalidCSISeq :public std::exception {};


template <typename F, size_t... Is>
auto gen_tuple_impl(F func, std::index_sequence<Is...> ) {
  return std::make_tuple(func(Is)...);
}

template <size_t N, typename F>
auto gen_tuple(F func) {
  return gen_tuple_impl(func, std::make_index_sequence<N>{} );
}

// Tp must be tuple<int, int ...>
template <typename Tp>
void copy_vector_to_tuple(const std::vector<int> &vec, Tp &tp) {
  constexpr int N = std::tuple_size_v<Tp>;
  tp = gen_tuple<N>([&vec](size_t i) { return vec[i]; });
}


template <int N>
typename NTuple<N>::type parse_csi_n(const std::vector<uint8_t> &seq, int start, int end, int default_value) {
  auto ints = parse_csi_colon_ints(seq, start, end, default_value);
  if (ints.size() != N) {
    throw InvalidCSISeq();
  }

  typename NTuple<N>::type tp;
  copy_vector_to_tuple(ints, tp);
  return tp;
}

template <int N>
typename NTuple<N>::type csi_n(const std::vector<uint8_t> &seq, int default_value) {
  return parse_csi_n<N>(seq, 0, seq.size() - 1, default_value);
}
};
