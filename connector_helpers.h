//
// Created by lukemartinlogan on 12/11/23.
//

#ifndef HDF5_VOLS__CONNECTOR_HELPERS_H_
#define HDF5_VOLS__CONNECTOR_HELPERS_H_

#include <string>
#include <vector>
#include <list>
#include <sstream>

namespace h5 {

class ParseConn {
 public:
  std::vector<std::vector<std::string>> tree_;

 public:
  void parse(const std::string &str) {
    std::stringstream ss(str);

    std::string substring;
    while (std::getline(ss, substring, ';')) {
      std::stringstream ss2(substring);
      std::string substring2;
      tree_.emplace_back();
      while (std::getline(ss2, substring2, ':')) {
        tree_.back().push_back(substring2);
      }
    }
  }

  std::vector<std::string> &front() {
    return tree_.front();
  }

  std::string GetNextVolName() {
    if (tree_.size() >= 2) {
      return tree_[1][0];
    }
    return "";
  }

  std::string GetNextVolParams() {
    std::stringstream ss;
    if (tree_[1].size() >= 2) {
      ss << tree_[1][1] << ";";
    }
    for (size_t i = 2; i < tree_.size(); ++i) {
      for (std::string &name_param : tree_[i]) {
        ss << name_param << ":";
      }
      ss << ";";
    }
    return ss.str();
  }
};

inline void compress(void *input, size_t size, void **output) {
  (void) input; (void) size; (void) output;
}

inline void decompress(void *input, size_t size, void **output) {
  (void) input; (void) size; (void) output;
}

}

#endif //HDF5_VOLS__CONNECTOR_HELPERS_H_
