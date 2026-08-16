#pragma once
#include "tool.h"
#include <fstream>
#include <sstream>
#include <map>
#include <string>

class FileReadTool : public Tool {
private:
    std::string base_directory; 
public:
    FileReadTool(const std::string& base_dir = "./");
    std::string execute(const std::map<std::string, std::string>& args) override;
};