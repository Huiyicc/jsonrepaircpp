//
// Created by ShiYang Jia on 25-9-20.
//
#ifndef JSONREPAIR_HPP_
#define JSONREPAIR_HPP_
#include <string>
#include <stdexcept>


class JSONRepairError : public std::runtime_error {
public:
    size_t position;
    JSONRepairError(const std::string& message, size_t pos);
};

std::string jsonrepair(const std::string& text, int maxDepth = 100) ;
std::u16string jsonrepair(const std::u16string& text, int maxDepth = 100);

#endif