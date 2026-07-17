#pragma once
#include "tool.h"
#include <iostream>
#include <fstream>
#include <map>
#include <string>

class FileWriteTool : public Tool {
private:
    std::string base_directory; 
public:
    FileWriteTool(const std::string& n, const std::string& d, const std::string& base_dir = "./");
    std::string execute(const std::map<std::string, std::string>& args) override;
};
