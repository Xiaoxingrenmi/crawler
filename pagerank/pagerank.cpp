
// Functional PageRank implementation
//   by BOT Man & ZhangHan, 2018
//
// References:
// - http://ilpubs.stanford.edu:8090/422/1/1999-66.pdf
// - https://en.wikipedia.org/wiki/PageRank#Iterative
//
// Input:
//
// 1 http://localhost/
// 2 http://localhost/page1/
// 3 http://localhost/page2/
// 4 http://localhost/page2/page2-1/
//
// 1 1
// 1 2
// 2 1
// 2 3
// 2 4
// 3 1
// 3 3
// 3 4
// 4 1
// 4 3
//
// Output:
//
// 0.161595 http://localhost/page2/page2-1/
// 0.207705 http://localhost/page1/
// 0.230248 http://localhost/page2/
// 0.400453 http://localhost/

#include <assert.h>
#include <math.h>
#include <stddef.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using Index = size_t;
using AdjacencyMatrix = std::multimap<Index, Index>;

using Rank = double;
using Ranks = std::vector<Rank>;

using Url = std::string;
using UrlIndexMap = std::map<Index, Url>;
using UrlRankMap = std::map<Rank, Url>;

constexpr auto kDampingFactor = .85;
constexpr auto kConvergenceEpsilon = 1e-6;
constexpr auto kDeviationLimit = 1e-6;

AdjacencyMatrix Transpose(const AdjacencyMatrix& matrix) {
  return std::accumulate(
      std::begin(matrix), std::end(matrix), AdjacencyMatrix{},
      [](AdjacencyMatrix& transposed,
         const AdjacencyMatrix::value_type& pair) -> AdjacencyMatrix& {
        transposed.emplace(pair.second, pair.first);
        return transposed;
      });
}

size_t NumberOfPages(const AdjacencyMatrix& matrix) {
  auto key_set = [&matrix]() -> std::set<Index> {
    return std::accumulate(
        std::begin(matrix), std::end(matrix), std::set<Index>{},
        [](std::set<Index>& key_set,
           const AdjacencyMatrix::value_type& pair) -> std::set<Index>& {
          key_set.emplace(pair.second);
          return key_set;
        });
  };
  return key_set().size();
}

Ranks InitRanks(size_t N) {
  Ranks ranks(N);
  std::fill(std::begin(ranks), std::end(ranks),
            static_cast<Rank>(1) / static_cast<Rank>(N));
  return ranks;
}

void StepPageRank(const AdjacencyMatrix& matrix,
                  const AdjacencyMatrix& transposed,
                  const size_t number_of_pages,
                  const Ranks& ranks,
                  Ranks& next_ranks) {
  assert(number_of_pages == ranks.size());
  assert(number_of_pages == next_ranks.size());

  auto rank = [&ranks](Index index) -> Rank { return ranks[index]; };

  auto out_degree = [&matrix](Index index) -> size_t {
    return matrix.count(index);
  };

  // incoming_rank = sigma {from \in M(to)} (rank(from) / out_degree(from))
  auto incoming_rank = [&transposed, &rank, &out_degree](Index index) -> Rank {
    return std::accumulate(
        transposed.lower_bound(index), transposed.upper_bound(index),
        static_cast<Rank>(0),
        [&rank, &out_degree](Rank new_rank,
                             const AdjacencyMatrix::value_type& pair) -> Rank {
          return new_rank +
                 rank(pair.second) / static_cast<Rank>(out_degree(pair.second));
        });
  };

  // new_rank = ((1 - d) / number_of_pages + d * incoming_rank(index))
  auto new_rank = [&incoming_rank, number_of_pages](Index index) -> Rank {
    return (1 - kDampingFactor) / static_cast<Rank>(number_of_pages) +
           kDampingFactor * incoming_rank(index);
  };

  // fill |new_ranks| with new rank values
  std::generate(
      std::begin(next_ranks), std::end(next_ranks),
      [new_rank, index = 0]() mutable -> Rank { return new_rank(index++); });
}

bool IsConvergent(const Ranks& ranks, const Ranks& next_ranks) {
  assert(ranks.size() == next_ranks.size());

  auto square = [](Rank rank) -> Rank { return rank * rank; };

  return std::accumulate(
             std::begin(ranks), std::end(ranks), static_cast<Rank>(0),
             [&square, &next_ranks, index = 0](Rank vector_norm,
                                               Rank rank) mutable -> Rank {
               return vector_norm + square(rank - next_ranks[index++]);
             }) < kConvergenceEpsilon;
}

// https://stackoverflow.com/questions/1729772/getline-vs-istream-iterator
struct Line {
  std::string line;
  operator std::string() const { return line; }
};

std::istream& operator>>(std::istream& istr, Line& data) {
  return std::getline(istr, data.line);
}

using InputRet = std::pair<UrlIndexMap, AdjacencyMatrix>;
InputRet Input(std::istream& istr) {
  return std::accumulate(
      std::istream_iterator<Line>(istr), std::istream_iterator<Line>(),
      InputRet{},
      [meet_empty_line = false](InputRet& ret,
                                const Line& line) mutable -> InputRet& {
        if (line.line.empty()) {
          meet_empty_line = true;
          return ret;
        }
        if (!meet_empty_line) {
          Index index;
          Url url;
          std::istringstream(line) >> index >> url;
          assert(index && !url.empty());  // input validation
          ret.first.emplace(index - 1, url);
        } else {
          Index index1;
          Index index2;
          std::istringstream(line) >> index1 >> index2;
          assert(index1 && index2);  // input validation
          ret.second.emplace(index1 - 1, index2 - 1);
        }
        return ret;
      });
}

UrlRankMap NormalizeOutput(const UrlIndexMap& url_index_map,
                           const Ranks& ranks) {
  return std::accumulate(
      std::begin(url_index_map), std::end(url_index_map), UrlRankMap{},
      [&ranks](UrlRankMap& ret,
               const UrlIndexMap::value_type& url_index) -> UrlRankMap& {
        ret.emplace(ranks[url_index.first], url_index.second);
        return ret;
      });
}

namespace std {
std::ostream& operator<<(std::ostream& ostr,
                         const UrlRankMap::value_type& data) {
  return ostr << data.first << " " << data.second;
}
}  // namespace std

void Output(std::ostream& ostr, const UrlRankMap& url_rank_map) {
  std::copy(std::begin(url_rank_map), std::end(url_rank_map),
            std::ostream_iterator<UrlRankMap::value_type>(ostr, "\n"));
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "usage: ./pagerank CRAWLER_OUTPUT [PAGERANK_OUTPUT]\n";
    return 1;
  }

  auto ifs = std::ifstream(argv[1]);
  auto ofs = argc >= 3 ? std::ofstream(argv[2]) : std::ofstream();
  if (!ifs.is_open()) {
    std::cerr << "failed to open " << argv[1] << "\n";
    return 1;
  }

  const InputRet input = Input(ifs);
  const UrlIndexMap& url_index_map = input.first;

  const AdjacencyMatrix& matrix = input.second;
  const AdjacencyMatrix transposed = Transpose(matrix);

  const size_t number_of_pages = NumberOfPages(matrix);
  Ranks ranks = InitRanks(number_of_pages);
  Ranks next_ranks = InitRanks(number_of_pages);

  do {
    StepPageRank(matrix, transposed, number_of_pages, ranks, next_ranks);

    assert(fabs(static_cast<Rank>(1) -
                std::accumulate(std::begin(ranks), std::end(ranks),
                                static_cast<Rank>(0))) < kDeviationLimit);
#ifdef DEBUG
    std::cout << "cur: ";
    std::copy(std::begin(ranks), std::end(ranks),
              std::ostream_iterator<Rank>(std::cout, " "));
    std::cout << std::endl;
    std::cout << "nxt: ";
    std::copy(std::begin(next_ranks), std::end(next_ranks),
              std::ostream_iterator<Rank>(std::cout, " "));
    std::cout << std::endl << std::endl;
#endif  // DEBUG

    std::swap(ranks, next_ranks);
  } while (!IsConvergent(ranks, next_ranks));

  Output(ofs.is_open() ? ofs : std::cout,
         NormalizeOutput(url_index_map, ranks));
  return 0;
}
