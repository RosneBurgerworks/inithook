/*
 * textfile.cpp
 *
 *  Created on: Jan 24, 2017
 *      Author: nullifiedcat
 */

#include "common.hpp"

#include <stdio.h>

TextFile::TextFile() : lines{}
{
}

bool TextFile::TryLoad(const std::string &name)
{
    if (name.length() == 0)
        return false;
    std::string filename = format(DATA_PATH "/", name);
    std::ifstream file(filename, std::ios::in);
    if (!file)
    {
        return false;
    }
    lines.clear();
    for (std::string line; std::getline(file, line);)
    {
        if (*line.rbegin() == '\r')
            line.erase(line.length() - 1, 1);
        lines.push_back(line);
    }
    if (lines.size() > 0 && *lines.rbegin() == "\n")
        lines.pop_back();

    return true;
}

void TextFile::Load(const std::string &name)
{
    std::string filename = DATA_PATH "/" + name;
    std::ifstream file(filename, std::ios::in);
    if (file.bad())
    {
        logging::Info("Could not open the file: %s", filename.c_str());
        return;
    }
    lines.clear();
    for (std::string line; std::getline(file, line);)
    {
        if (*line.rbegin() == '\r')
            line.erase(line.length() - 1, 1);
        lines.push_back(line);
    }
}

size_t TextFile::LineCount() const
{
    return lines.size();
}

const std::string &TextFile::Line(size_t id) const
{
    return lines.at(id);
}
