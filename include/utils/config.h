#ifndef __CONFIG_H_
#define __CONFIG_H_

#include <map>
#include <fstream>

#include <utils/simple_objects.h>

class config {
  std::map<std::string,std::string> content_;

public:
  config(std::string const& configFile);

  std::string const& value(std::string const& section, std::string const& entry) const;
  bool exist(std::string const& section, std::string const& entry) const;
};

#endif //__CONFIG_H_
