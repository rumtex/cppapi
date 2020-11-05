#include <utils/config.h>

std::string trim(std::string const& source, char const* delims = " \t\r\n") {
  std::string result(source);
  std::string::size_type index = result.find_last_not_of(delims);
  if(index != std::string::npos)
    result.erase(++index);

  index = result.find_first_not_of(delims);
  if (index != std::string::npos)
    result.erase(0, index);
  else
    result.erase();
  return result;
}

config::config(std::string const& configFile) {
  std::ifstream file(configFile.c_str());

  if (!file) throw exception("missing config file (./"+ configFile + ")");

  std::string line;
  std::string name;
  std::string value;
  std::string inSection;
  int posEqual;
  while (std::getline(file,line)) {

    if (! line.length()) continue;

    if (line[0] == '#') continue;
    if (line[0] == ';') continue;

    if (line[0] == '[') {
      inSection=trim(line.substr(1,line.find(']')-1));
      continue;
    }

    posEqual=line.find('=');
    name  = trim(line.substr(0,posEqual));
    value = trim(line.substr(posEqual+1));

    content_[inSection+'/'+name]=std::string(value);
  }
}

std::string const& config::value(std::string const& section, std::string const& entry) const {

  std::map<std::string,std::string>::const_iterator ci = content_.find(section + '/' + entry);

  if (ci == content_.end()) throw exception("[" + section + "] " + entry + ": does not exist");

  return ci->second;
}

bool config::exist(std::string const& section, std::string const& entry) const {

  return (content_.find(section + '/' + entry) != content_.end());
}

config*                 s_config = NULL;