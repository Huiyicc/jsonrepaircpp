//
// Created by ShiYang Jia on 25-9-20.
//
#include "./jsonrepair.hpp"
#include "./utf8.h"
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <vector>

using CharT = char16_t;
using StringT = std::u16string;

static bool isHex(CharT c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
         (c >= 'a' && c <= 'f');
}

static bool isDigit(CharT c) { return c >= '0' && c <= '9'; }

static bool isValidStringCharacter(CharT c) { return c >= 0x20; }

static bool isDelimiter(CharT c) {
  return c == ',' || c == ':' || c == '[' || c == ']' || c == '/' || c == '{' ||
         c == '}' || c == '(' || c == ')' || c == '\n' || c == '+';
}

static bool isFunctionNameCharStart(CharT c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
         c == '$';
}

static bool isFunctionNameChar(CharT c) {
  return isFunctionNameCharStart(c) || (c >= '0' && c <= '9');
}

static bool isUrlStart(const StringT &s) {
  if (s.length() < 3)
    return false;
  static const std::vector<StringT> prefixes = {
      u"http://", u"https://", u"ftp://", u"mailto:",
      u"file://", u"data:",    u"irc://"};
  for (const auto &prefix : prefixes) {
    if (s.length() >= prefix.length() &&
        s.substr(0, prefix.length()) == prefix) {
      return true;
    }
  }
  return false;
}

static bool isUrlChar(CharT c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
         c == '~' || c == ':' || c == '/' || c == '?' || c == '#' || c == '@' ||
         c == '!' || c == '$' || c == '&' || c == '\'' || c == '(' ||
         c == ')' || c == '*' || c == '+' || c == ',' || c == ';' || c == '=';
}

static bool isUnquotedStringDelimiter(CharT c) {
  return c == ',' || c == '[' || c == ']' || c == '/' || c == '{' || c == '}' ||
         c == '\n' || c == '+';
}

static bool isStartOfValue(CharT c) {
  return c == '"' || c == '\'' || c == '{' || c == '[' || c == '-' ||
         (c >= '0' && c <= '9') || isFunctionNameCharStart(c);
}

static bool isControlCharacter(CharT c) {
  return c == '\n' || c == '\r' || c == '\t' || c == '\b' || c == '\f';
}

static bool isWhitespace(CharT c) {
  return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

static bool isWhitespaceExceptNewline(CharT c) {
  return c == ' ' || c == '\t' || c == '\r';
}

static bool isSpecialWhitespace(CharT c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return uc == 0xA0 || (uc >= 0x2000 && uc <= 0x200A) || uc == 0x202F ||
         uc == 0x205F || uc == 0x3000;
}

static bool isDoubleQuote(char16_t c) {
  return c == u'"' || c == u'“' || c == u'”';
}

static bool isSingleQuote(CharT c) {
  return c == u'\'' || c == u'`' || c == u'‘' || c == u'’';
}

static bool isQuote(char16_t c) { return isDoubleQuote(c) || isSingleQuote(c); }

static StringT stripLastOccurrence(const StringT &text, const StringT &toStrip,
                                   bool stripRemaining = false) {
  auto pos = text.rfind(toStrip);
  if (pos == StringT::npos)
    return text;
  if (stripRemaining) {
    return text.substr(0, pos);
  } else {
    return text.substr(0, pos) + text.substr(pos + toStrip.length());
  }
}

static StringT insertBeforeLastWhitespace(const StringT &text,
                                          const StringT &toInsert) {
  if (text.empty())
    return toInsert;

  size_t index = text.length();
  if (index == 0 || !isWhitespace(text[index - 1])) {
    return text + toInsert;
  }

  while (index > 0 && isWhitespace(text[index - 1])) {
    index--;
  }

  return text.substr(0, index) + toInsert + text.substr(index);
}

static StringT removeAtIndex(const StringT &text, size_t start, size_t count) {
  if (start >= text.length())
    return text;
  size_t end = std::min(start + count, text.length());
  return text.substr(0, start) + text.substr(end);
}

static bool endsWithCommaOrNewline(const StringT &text) {
  if (text.empty())
    return false;
  for (int i = static_cast<int>(text.length()) - 1; i >= 0; --i) {
    CharT c = text[i];
    if (c == ',' || c == '\n')
      return true;
    if (!isWhitespace(c))
      break;
  }
  return false;
}

// Escape character map
static const std::unordered_map<char16_t, char16_t> escapeCharacters = {
    {u'"', u'"'},  {u'\\', u'\\'}, {u'/', u'/'},  {u'b', u'\b'},
    {u'f', u'\f'}, {u'n', u'\n'},  {u'r', u'\r'}, {u't', u'\t'}};

static const std::unordered_map<char16_t, StringT> controlCharacters = {
    {u'\b', u"\\b"},
    {u'\f', u"\\f"},
    {u'\n', u"\\n"},
    {u'\r', u"\\r"},
    {u'\t', u"\\t"}};

// --- JSONRepairError Implementation ---
JSONRepairError::JSONRepairError(const std::string &message, size_t pos)
    : std::runtime_error(message + " at position " + std::to_string(pos)),
      position(pos) {}

// --- Main jsonrepair function ---
std::string jsonrepair(const std::string &text_utf8, int maxDepth) {
  std::u16string input;
  utf8::utf8to16(text_utf8.begin(), text_utf8.end(), std::back_inserter(input));
  auto output = jsonrepair(input, maxDepth);
  std::string rs;
  utf8::utf16to8(output.begin(), output.end(), std::back_inserter(rs));
  return rs;
}
std::u16string jsonrepair(const std::u16string &text, int maxDepth) {
  size_t i = 0;
  StringT output;
  int currentDepth = 0;

  if (maxDepth <= 0)
    maxDepth = 100;

  // Helper lambdas
  auto parseWhitespaceAndSkipComments = [&](bool skipNewline = true) -> bool {
    size_t start = i;
    auto isWhiteSpace = skipNewline ? isWhitespace : isWhitespaceExceptNewline;

    auto parseWhitespace = [&]() -> bool {
      StringT whitespace;
      while (i < text.length()) {
        CharT c = text[i];
        if (isWhiteSpace(c)) {
          whitespace += c;
          i++;
        } else if (isSpecialWhitespace(c)) {
          whitespace += ' ';
          i++;
        } else {
          break;
        }
      }
      if (!whitespace.empty()) {
        output += whitespace;
        return true;
      }
      return false;
    };

    auto parseComment = [&]() -> bool {
      if (i + 1 < text.length() && text[i] == '/' && text[i + 1] == '*') {
        i += 2;
        while (i < text.length() && !(i + 1 < text.length() && text[i] == '*' &&
                                      text[i + 1] == '/')) {
          i++;
        }
        if (i + 1 < text.length())
          i += 2;
        return true;
      }
      if (i + 1 < text.length() && text[i] == '/' && text[i + 1] == '/') {
        while (i < text.length() && text[i] != '\n') {
          i++;
        }
        return true;
      }
      return false;
    };

    bool changed = parseWhitespace();
    do {
      bool commentChanged = parseComment();
      if (commentChanged) {
        changed = true;
        parseWhitespace();
      } else {
        break;
      }
    } while (true);

    return i > start;
  };

  auto parseCharacter = [&](CharT c) -> bool {
    if (i < text.length() && text[i] == c) {
      output += c;
      i++;
      return true;
    }
    return false;
  };

  auto skipCharacter = [&](CharT c) -> bool {
    if (i < text.length() && text[i] == c) {
      i++;
      return true;
    }
    return false;
  };

  auto skipEscapeCharacter = [&]() -> bool { return skipCharacter('\\'); };

  auto skipEllipsis = [&]() -> bool {
    parseWhitespaceAndSkipComments();
    if (i + 2 < text.length() && text[i] == '.' && text[i + 1] == '.' &&
        text[i + 2] == '.') {
      i += 3;
      parseWhitespaceAndSkipComments();
      skipCharacter(',');
      return true;
    }
    return false;
  };

  auto parseMarkdownCodeBlock =
      [&](const std::vector<StringT> &blocks) -> bool {
    parseWhitespaceAndSkipComments();
    for (const auto &block : blocks) {
      if (i + block.length() <= text.length() &&
          text.substr(i, block.length()) == block) {
        i += block.length();
        if (i < text.length() && isFunctionNameCharStart(text[i])) {
          while (i < text.length() && isFunctionNameChar(text[i])) {
            i++;
          }
        }
        parseWhitespaceAndSkipComments();
        return true;
      }
    }
    return false;
  };

  auto prevNonWhitespaceIndex = [&](size_t start) -> size_t {
    size_t prev = start;
    while (prev > 0 && isWhitespace(text[prev - 1])) {
      prev--;
    }
    return prev;
  };

  // Forward declarations
  std::function<bool()> parseValue;
  std::function<bool()> parseObject;
  std::function<bool()> parseArray;
  std::function<void()> parseNewlineDelimitedJSON;
  std::function<bool(bool, size_t)> parseString;
  std::function<bool()> parseConcatenatedString;
  std::function<bool()> parseNumber;
  std::function<bool()> parseKeywords;
  std::function<bool(const StringT &, const StringT &)> parseKeyword;
  std::function<bool(bool)> parseUnquotedString;
  std::function<bool()> parseRegex;

  // --- Implementations ---

  parseValue = [&]() -> bool {
    if (currentDepth > maxDepth) {
      throw JSONRepairError("Maximum depth exceeded", i);
    }
    parseWhitespaceAndSkipComments();
    bool processed = parseObject() || parseArray() ||
                     parseString(false, static_cast<size_t>(-1)) ||
                     parseNumber() || parseKeywords() ||
                     parseUnquotedString(false) || parseRegex();
    parseWhitespaceAndSkipComments();
    return processed;
  };

  parseObject = [&]() -> bool {
    if (i >= text.length() || text[i] != '{')
      return false;
    currentDepth++;
    output += '{';
    i++;
    parseWhitespaceAndSkipComments();

    if (skipCharacter(',')) {
      parseWhitespaceAndSkipComments();
    }

    bool initial = true;
    while (i < text.length() && text[i] != '}') {
      bool processedComma = false;
      if (!initial) {
        processedComma = parseCharacter(',');
        if (!processedComma) {
          output = insertBeforeLastWhitespace(output, u",");
        }
        parseWhitespaceAndSkipComments();
      } else {
        initial = false;
      }

      skipEllipsis();

      bool processedKey = parseString(false, static_cast<size_t>(-1)) ||
                          parseUnquotedString(true);
      if (!processedKey) {
        if (i >= text.length() || text[i] == '}' || text[i] == '{' ||
            text[i] == ']' || text[i] == '[') {
          output = stripLastOccurrence(output, u",");
        } else {
          throw JSONRepairError("Object key expected", i);
        }
        break;
      }

      parseWhitespaceAndSkipComments();
      bool processedColon = parseCharacter(':');
      bool truncated = i >= text.length();
      if (!processedColon) {
        if (isStartOfValue(i < text.length() ? text[i] : '\0') || truncated) {
          output = insertBeforeLastWhitespace(output, u":");
        } else {
          throw JSONRepairError("Colon expected", i);
        }
      }

      bool processedValue = parseValue();
      if (!processedValue) {
        if (processedColon || truncated) {
          output += u"null";
        } else {
          throw JSONRepairError("Colon expected", i);
        }
      }
    }

    if (i < text.length() && text[i] == '}') {
      output += '}';
      i++;
    } else {
      output = insertBeforeLastWhitespace(output, u"}");
    }
    currentDepth--;
    return true;
  };

  parseArray = [&]() -> bool {
    if (i >= text.length() || text[i] != '[')
      return false;
    currentDepth++;
    output += '[';
    i++;
    parseWhitespaceAndSkipComments();

    if (skipCharacter(',')) {
      parseWhitespaceAndSkipComments();
    }

    bool initial = true;
    while (i < text.length() && text[i] != ']') {
      if (!initial) {
        bool processedComma = parseCharacter(',');
        if (!processedComma) {
          output = insertBeforeLastWhitespace(output, u",");
        }
      } else {
        initial = false;
      }

      skipEllipsis();

      bool processedValue = parseValue();
      if (!processedValue) {
        output = stripLastOccurrence(output, u",");
        break;
      }
    }

    if (i < text.length() && text[i] == ']') {
      output += ']';
      i++;
    } else {
      output = insertBeforeLastWhitespace(output, u"]");
    }
    currentDepth--;
    return true;
  };

  parseNewlineDelimitedJSON = [&]() {
    output = u"[\n" + output;
    bool first = true;
    while (i < text.length()) {
      parseWhitespaceAndSkipComments();
      if (i >= text.length() || !isStartOfValue(text[i]))
        break;
      if (!first) {
        output += u",\n";
      } else {
        first = false;
      }
      if (!parseValue())
        break;
    }
    output += u"\n]";
  };

  parseString = [&](bool stopAtDelimiter, size_t stopAtIndex) -> bool {
    bool skipEscapeChars = (i < text.length() && text[i] == '\\');
    if (skipEscapeChars) {
      i++;
    }

    if (i >= text.length() || !isQuote(text[i])) {
      return false;
    }

    auto isEndQuote = [&](CharT c) -> bool {
      if (isDoubleQuote(text[i]))
        return isDoubleQuote(c);
      return isSingleQuote(c); // 简化处理
    };

    size_t iBefore = i;
    size_t oBefore = output.length();
    StringT str = u"\"";
    i++;

    while (true) {
      if (i >= text.length()) {
        size_t iPrev = prevNonWhitespaceIndex(i - 1);
        if (!stopAtDelimiter && iPrev < text.length() &&
            isDelimiter(text[iPrev])) {
          i = iBefore;
          output = output.substr(0, oBefore);
          return parseString(true, static_cast<size_t>(-1));
        }
        str = insertBeforeLastWhitespace(str, u"\"");
        output += str;
        return true;
      }

      if (i == stopAtIndex) {
        str = insertBeforeLastWhitespace(str, u"\"");
        output += str;
        return true;
      }

      if (isEndQuote(text[i])) {
        size_t iQuote = i;
        size_t oQuote = str.length();
        str += '"';
        i++;
        output += str;

        parseWhitespaceAndSkipComments(false);

        if (stopAtDelimiter || i >= text.length() ||
            (i < text.length() &&
             (isDelimiter(text[i]) || isQuote(text[i]) || isDigit(text[i])))) {
          parseConcatenatedString();
          return true;
        }

        size_t iPrevchar = prevNonWhitespaceIndex(iQuote - 1);
        CharT prevchar = (iPrevchar < text.length()) ? text[iPrevchar] : '\0';

        if (prevchar == ',') {
          i = iBefore;
          output = output.substr(0, oBefore);
          return parseString(false, iPrevchar);
        }

        if (isDelimiter(prevchar)) {
          i = iBefore;
          output = output.substr(0, oBefore);
          return parseString(true, static_cast<size_t>(-1));
        }

        output = output.substr(0, oBefore);
        i = iQuote + 1;
        str = str.substr(0, oQuote) + u"\\" + str.substr(oQuote);
        continue;
      }

      if (stopAtDelimiter && isUnquotedStringDelimiter(text[i])) {
        StringT urlTest =
            (iBefore + 1 < i + 2 && iBefore + 1 < text.length())
                ? text.substr(iBefore + 1,
                              std::min(i + 2 - (iBefore + 1),
                                       text.length() - (iBefore + 1)))
                : u"";
        if (i > 0 && text[i - 1] == ':' && isUrlStart(urlTest)) {
          while (i < text.length() && isUrlChar(text[i])) {
            str += text[i];
            i++;
          }
        }
        str = insertBeforeLastWhitespace(str, u"\"");
        output += str;
        parseConcatenatedString();
        return true;
      }

      if (i < text.length() && text[i] == '\\') {
        if (i + 1 >= text.length()) {
          i++;
          continue;
        }
        CharT next = text[i + 1];
        auto it = escapeCharacters.find(next);
        if (it != escapeCharacters.end()) {
          str += text.substr(i, 2);
          i += 2;
        } else if (next == 'u') {
          int j = 2;
          while (j < 6 && i + j < text.length() && isHex(text[i + j])) {
            j++;
          }
          if (j == 6) {
            str += text.substr(i, 6);
            i += 6;
          } else if (i + j >= text.length()) {
            i = text.length();
          } else {
            throw JSONRepairError("Invalid unicode character", i);
          }
        } else {
          str += next;
          i += 2;
        }
        continue;
      }

      if (i < text.length()) {
        CharT c = text[i];
        if (c == '"' && (i == 0 || text[i - 1] != '\\')) {
          str += u"\\\"";
          i++;
        } else if (isControlCharacter(c)) {
          auto ctrlIt = controlCharacters.find(c);
          if (ctrlIt != controlCharacters.end()) {
            str += ctrlIt->second;
          } else {
            str += c;
          }
          i++;
        } else {
          if (!isValidStringCharacter(c)) {
            auto cs = StringT(1, c);
            std::string rs;
            utf8::utf16to8(cs.begin(), cs.end(), std::back_inserter(rs));
            throw JSONRepairError("Invalid character " + rs, i);
          }
          str += c;
          i++;
        }
      }

      // 防止死循环：确保 i 至少推进 1
      if (i == iBefore) {
        i++; // 强制推进，避免死循环
      }

      if (skipEscapeChars) {
        skipEscapeCharacter();
      }
    }
  };

  parseConcatenatedString = [&]() -> bool {
    bool processed = false;
    parseWhitespaceAndSkipComments();
    while (i < text.length() && text[i] == '+') {
      processed = true;
      i++;
      parseWhitespaceAndSkipComments();
      // 只移除最后一个引号，不移除后续内容
      output = stripLastOccurrence(output, u"\"", false);
      size_t start = output.length();
      bool parsed = parseString(false, static_cast<size_t>(-1));
      if (parsed) {
        output = removeAtIndex(output, start,
                               1); // 移除开头的 "，因为 parseString 会加
      } else {
        output = insertBeforeLastWhitespace(output, u"\"");
      }
    }
    return processed;
  };

  parseNumber = [&]() -> bool {
    size_t start = i;
    if (i < text.length() && text[i] == '-') {
      i++;
      if (i >= text.length() || (!isDigit(text[i]) && text[i] != '.')) {
        output += text.substr(start, i - start) + u"0";
        return true;
      }
    }

    while (i < text.length() && isDigit(text[i])) {
      i++;
    }

    if (i < text.length() && text[i] == '.') {
      i++;
      if (i >= text.length() || !isDigit(text[i])) {
        output += text.substr(start, i - start) + u"0";
        return true;
      }
      while (i < text.length() && isDigit(text[i])) {
        i++;
      }
    }

    if (i < text.length() && (text[i] == 'e' || text[i] == 'E')) {
      i++;
      if (i < text.length() && (text[i] == '+' || text[i] == '-')) {
        i++;
      }
      if (i >= text.length() || !isDigit(text[i])) {
        output += text.substr(start, i - start) + u"0";
        return true;
      }
      while (i < text.length() && isDigit(text[i])) {
        i++;
      }
    }

    if (i >= text.length() || isDelimiter(text[i]) || isWhitespace(text[i])) {
      if (i > start) {
        StringT num = text.substr(start, i - start);
        bool hasInvalidLeadingZero =
            num.length() > 1 && num[0] == '0' && num[1] >= '0' && num[1] <= '9';
        output += hasInvalidLeadingZero ? (u"\"" + num + u"\"") : num;
        return true;
      }
    } else {
      i = start;
      return false;
    }

    return false;
  };

  parseKeywords = [&]() -> bool {
    return parseKeyword(u"true", u"true") || parseKeyword(u"false", u"false") ||
           parseKeyword(u"null", u"null") || parseKeyword(u"True", u"true") ||
           parseKeyword(u"False", u"false") || parseKeyword(u"None", u"null");
  };

  parseKeyword = [&](const StringT &name, const StringT &value) -> bool {
    if (i + name.length() <= text.length() &&
        text.substr(i, name.length()) == name) {
      output += value;
      i += name.length();
      return true;
    }
    return false;
  };

  parseUnquotedString = [&](bool isKey) -> bool {
    size_t start = i;
    if (i < text.length() && isFunctionNameCharStart(text[i])) {
      while (i < text.length() && isFunctionNameChar(text[i])) {
        i++;
      }
      size_t j = i;
      while (j < text.length() && isWhitespace(text[j])) {
        j++;
      }
      if (j < text.length() && text[j] == '(') {
        i = j + 1;
        parseValue();
        if (i < text.length() && text[i] == ')') {
          i++;
          if (i < text.length() && text[i] == ';') {
            i++;
          }
        }
        return true;
      }
    }

    while (i < text.length() && !isUnquotedStringDelimiter(text[i]) &&
           !isQuote(text[i]) && (!isKey || text[i] != ':')) {
      i++;
    }

    if (i > start && i > 0 && text[i - 1] == ':' && i + 2 <= text.length()) {
      StringT test =
          text.substr(start, std::min(i + 2 - start, text.length() - start));
      if (isUrlStart(test)) {
        while (i < text.length() && isUrlChar(text[i])) {
          i++;
        }
      }
    }

    if (i > start) {
      while (i > start && isWhitespace(text[i - 1])) {
        i--;
      }
      StringT symbol = text.substr(start, i - start);
      if (symbol == u"undefined") {
        output += u"null";
      } else {
        StringT escaped = u"\"";
        for (CharT c : symbol) {
          if (c == '"' || c == '\\') {
            escaped += '\\';
          }
          escaped += c;
        }
        escaped += '"';
        output += escaped;
      }
      if (i < text.length() && text[i] == '"') {
        i++;
      }
      return true;
    }
    return false;
  };

  parseRegex = [&]() -> bool {
    if (i < text.length() && text[i] == '/') {
      size_t start = i;
      i++;
      while (i < text.length() &&
             (text[i] != '/' || (i > 0 && text[i - 1] == '\\'))) {
        i++;
      }
      if (i < text.length()) {
        i++; // skip closing '/'
      }
      output += u"\"" + text.substr(start, i - start) + u"\"";
      return true;
    }
    return false;
  };

  // --- Main logic ---

  parseMarkdownCodeBlock({u"```", u"[```", u"{```"});

  bool processed = parseValue();
  if (!processed) {
    throw JSONRepairError("Unexpected end of json string", text.length());
  }

  parseMarkdownCodeBlock({u"```", u"```]", u"```}"});

  bool processedComma = parseCharacter(',');
  if (processedComma) {
    parseWhitespaceAndSkipComments();
  }

  if (i < text.length() && isStartOfValue(text[i]) &&
      endsWithCommaOrNewline(output)) {
    if (!processedComma) {
      output = insertBeforeLastWhitespace(output, u",");
    }
    parseNewlineDelimitedJSON();
  } else if (processedComma) {
    output = stripLastOccurrence(output, u",");
  }

  while (i < text.length() && (text[i] == '}' || text[i] == ']')) {
    i++;
    parseWhitespaceAndSkipComments();
  }

  if (i >= text.length()) {
    return output;
  }

  auto cs = StringT(1, text[i]);
  std::string rs;
  utf8::utf16to8(cs.begin(), cs.end(), std::back_inserter(rs));
  throw JSONRepairError("Unexpected character " + rs, i);
}
