#include "jsonrepair/jsonrepair.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

extern std::vector<std::string> testdad;

int main() {
  for (auto &v : testdad) {
    try {
      std::string fixed = jsonrepair(v, 10);
      std::cout << fixed << "\n====\n";
    } catch (const JSONRepairError &e) {
      std::cerr << e.what() << "\n====\n";
    }
  }
  return 0;
}

std::vector<std::string> testdad = {
    R"({
    "translated_contents": [
        {
            "source": "TABLE 5. Finish time and "throughput increase for HR and RR.",
            "translation": "表 5. HR 和 RR 的完成时间及吞吐量提升"]
        }
    ]
}
)",
  R"({ “id”: 1 })",
    "{ homepage: https://example.com/path }",
    "\"hello\\world",
    R"({ a: "foo" + "bar", b: "baz" })",
    R"({ "id": 1 }
{ "id": 2 }
{ "id": 3 })",
    R"([[[[[[[[[1]]]]])",
    "+",
};