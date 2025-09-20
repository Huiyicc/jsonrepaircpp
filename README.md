# jsonrepair

A c++ json document repair library.

A robust JSON repair library for c++ — fixes malformed, truncated, or otherwise invalid JSON strings and converts them into valid JSON.

This is a faithful port of the original [josdejong/jsonrepair](https://github.com/josdejong/jsonrepair) library (written in JavaScript/Python/...) to <目标语言>. All core functionality is preserved.

The library can automatically fix common JSON issues, including:

- ✅ Add missing quotes around keys
- ✅ Add missing escape characters
- ✅ Add missing commas and closing brackets
- ✅ Repair truncated JSON
- ✅ Replace single quotes with double quotes
- ✅ Replace “smart quotes” and special whitespace with standard characters
- ✅ Convert Python constants (None, True, False) → (null, true, false)
- ✅ Strip trailing commas, comments (//, /* */), and JSONP wrappers
- ✅ Strip code fences (json ... ) and ellipsis ([1, 2, ...])
- ✅ Unescape doubly-escaped strings ({\"key\": \"value\"} → {"key": "value"})
- ✅ Remove MongoDB types like NumberLong(2) or ISODate(...)
- ✅ Concatenate split strings ("part1" + "part2" → "part1part2")
- ✅ Convert newline-delimited JSON (NDJSON) into a valid JSON array

## Build & Install

```bash
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build .
sudo cmake --install .
```

## use

```c++
#include "jsonrepair/jsonrepair.hpp"
int main() {
  try {
    auto broken = "{name: 'John', age: 30,}";
    std::string fixed = jsonrepair(broken);
    std::cout << fixed << "\n====\n";
  } catch (const JSONRepairError &e) {
    std::cerr << e.what() << "\n====\n";
  }
  return 0;
}
```

## using lib
[nemtrif/utfcpp](https://github.com/nemtrif/utfcpp) support utf8/utf16

