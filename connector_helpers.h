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

  void pop() {
    if (tree_.size()) {
      tree_.erase(tree_.begin());
    }
    if (tree_.size()) {
      tree_.front().erase(tree_.front().begin());
    }
  }

  std::string serialize() {
    std::stringstream ss;
    for (std::vector<std::string> &list : tree_) {
      for (std::string &str : list) {
        ss << str << ":";
      }
      ss << ";";
    }
    return ss.str();
  }
};

}

#endif //HDF5_VOLS__CONNECTOR_HELPERS_H_
