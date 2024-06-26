#include "json_io.h"

#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <format>
#include <iostream>

#include "rapidjson/error/en.h"
#include "rapidjson/error/error.h"
#include "rapidjson/reader.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/prettywriter.h"

std::string ymdToString(const std::chrono::year_month_day& ymd) {
  return std::format("{:%F}", ymd);
}

std::optional<std::chrono::year_month_day> stringToYmd(const char* str, std::size_t length) {
  using namespace std::chrono;

  int year_i, month_i, day_i;

  const char* const last = str+length;
  std::from_chars_result res{str, std::errc{}};
  res = std::from_chars(res.ptr, last, year_i);
  if (res.ec != std::errc{} || *res.ptr != '-') return std::nullopt;
  ++res.ptr;
  res = std::from_chars(res.ptr, last, month_i);
  if (res.ec != std::errc{} || *res.ptr != '-') return std::nullopt;
  ++res.ptr;
  res = std::from_chars(res.ptr, last, day_i);
  if (res.ec != std::errc{} || *res.ptr != '\0') return std::nullopt;

  if (year_i < -32767 || year_i > 32767 || month_i < 1 || month_i > 12 || day_i < 1 || day_i > 31) return std::nullopt;
  
  year_month_day ymd{year(year_i), month(month_i), day(day_i)};
  if (!ymd.ok()) return std::nullopt;
  return ymd;
}

struct FileNotFoundException : std::runtime_error {
  using std::runtime_error::runtime_error;
};

class File {
  FILE* mFile;
  const char* mFilename;
  bool mIsForReading;

  void close(bool shouldThrow) {
    if (mFile == nullptr) return;
    int err = fclose(mFile);
    mFile = nullptr;
    if (err && shouldThrow) {
      const char* openMode = (mIsForReading) ? "read" : "write";
      throw std::runtime_error(std::format("Failed to {} to file {}!", openMode, mFilename));
    }
  }

public:
  File(const char* filename, const char* mode)
    : mFile(fopen(filename, mode)), mFilename(filename), mIsForReading(mode[0]=='r') {
    if (mFile == nullptr) {
      int err = errno;
      errno = 0;
      const char* openMode = (mIsForReading) ? "reading" : "writing";
      std::string msg = std::format("Failed to open file {} for {} ({})!", filename, openMode, std::strerror(err));
      (err == ENOENT) ? throw FileNotFoundException(msg) : throw std::runtime_error(msg);
    }
  }

  File(const File&) = delete;
  File& operator=(const File&) = delete;

  ~File() noexcept {
    close(false);
  }

  FILE* getHandle() const {return mFile;}
  void close() {close(true);}
};

class ReaderBase {
  std::string mError;

protected:
  void setError(std::string&& error) {mError = std::move(error);}

public:
  void checkResult(const rapidjson::ParseResult& result, const File& file) {
    if (ferror(file.getHandle())) {
      throw std::runtime_error("Error while reading json file!");
    }
    if (result.Code() != rapidjson::kParseErrorNone) {
      const char* errMsg;
      if (result.Code() != rapidjson::kParseErrorTermination || mError.empty())
      {
        errMsg = rapidjson::GetParseError_En(result.Code());
      } else {
        errMsg = mError.c_str();
      }
      throw std::runtime_error(std::format("Error parsing json at offset {} ({})!", result.Offset(), errMsg));
    }
  }
};

class CardsDueDatesReader : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, CardsDueDatesReader>, public ReaderBase {
  CardsDueDates& mCardsDueDates;
  const Cards& mCards;
  std::string mTitle;
  bool mIsDocumentObject = false;

public:
  CardsDueDatesReader(CardsDueDates& cardsDueDates, const Cards& cards)
    : mCardsDueDates(cardsDueDates), mCards(cards) {}

  bool Default() {
    setError("Unexpected element type");
    return false;
  }

  bool String(const char* str, rapidjson::SizeType lenght, [[maybe_unused]] bool copy) {
    if (!mIsDocumentObject) {
      setError("Unexpected element type");
      return false;
    }
    auto ymd = stringToYmd(str, lenght);
    if (!ymd.has_value()) {
      setError("Invalid date");
      return false;
    }
    const Card* card = mCards.getCard(mTitle);
    mTitle.clear();
    if (card == nullptr) {
      std::cout << "Card `" << str << "` is not present!" << std::endl;
    } else {
      mCardsDueDates.addCard(*card, *ymd);
    }
    return true;
  }

  bool StartObject() {
    if (!mIsDocumentObject) {
      mIsDocumentObject = true;
      return true;
    } else {
      setError("Unexpected element type");
      return false;
    }
  }

  bool Key(const char* str, rapidjson::SizeType lenght, [[maybe_unused]] bool copy) {
    mTitle = std::string{str, lenght};
    if (!lenght) {
      setError("Empty key");
    }
    return lenght > rapidjson::SizeType(0);
  }

  bool EndObject([[maybe_unused]] rapidjson::SizeType memCount) {return true;}
};

struct CardsReader : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, CardsDueDatesReader>, public ReaderBase {
  Cards& mCards;
  std::string mTitle;
  std::string mFirstSide;
  bool mIsDocumentObject = false;
  bool mIsParsingCard = false;

  CardsReader(Cards& cards) : mCards(cards) {}

  bool Default() {
    setError("Unexpected element type");
    return false;
  }

  bool String(const char* str, rapidjson::SizeType lenght, [[maybe_unused]] bool copy) {
    if (!mIsDocumentObject || !mIsParsingCard) {
      setError("Unexpected element type");
      return false;
    }
    if (lenght == 0) {
      setError("Empty card side");
      return false;
    }
    if (mFirstSide.empty()) {
      mFirstSide = std::string{str, lenght};
      return true;
    } else {
      std::string secondSide{str, lenght};
      mIsParsingCard = false;
      bool success = mCards.registerCard(Card{std::move(mTitle), std::move(mFirstSide), std::move(secondSide)});
      if (!success) {
        setError(std::format("Card `{}` already present", mTitle, std::string_view{str, lenght}));
      }
      return success;
    }
  }

  bool StartObject() {
    if (!mIsDocumentObject) {
      mIsDocumentObject = true;
      return true;
    } else {
      setError("Unexpected element type");
      return false;
    }
  }

  bool Key(const char* str, rapidjson::SizeType lenght, [[maybe_unused]] bool copy) {
    mTitle = std::string{str, lenght};
    if (!lenght) {
      setError("Empty key");
    }
    return lenght > rapidjson::SizeType(0);
  }

  bool EndObject([[maybe_unused]] rapidjson::SizeType memCount) {return true;}

  bool StartArray() {
    if (!mIsDocumentObject || mIsParsingCard) {
      setError("Unexpected element type");
      return false;
    }
    mIsParsingCard = true;
    return true;
  }

  bool EndArray(rapidjson::SizeType numElement) {
    if (numElement != rapidjson::SizeType(2)) {
      setError("Card can only have 2 sides");
      return false;
    }
    return true;
  }
};

template<typename Writer>
void writeCardsDueDates(const CardsDueDates& cardsDueDates, Writer& writer) {
  std::string today = ymdToString(cardsDueDates.getToday());

  writer.StartObject();
  for (CardReference card : cardsDueDates.getDueCards()) {
    writer.Key(card.get().title().c_str());
    writer.String(today.c_str());
  }
  for (const auto& [dueDate, card] : cardsDueDates.getOtherCards()) {
    writer.Key(card.get().title().c_str());
    writer.String(ymdToString(dueDate).c_str());
  }
  writer.EndObject();
}

Cards readCards(const char* cardsPath) {
  std::cout << "Reading cards..." << std::endl;
  Cards cards;
  File fp{cardsPath, "r"};
  char readBuffer[65536];
  rapidjson::FileReadStream is{fp.getHandle(), readBuffer, sizeof(readBuffer)};

  CardsReader handler{cards};
  rapidjson::Reader reader;
  handler.checkResult(reader.Parse(is, handler), fp);
  fp.close();
  return cards;
}

CardsDueDates readCardsDueDates(const char* cardsDueDatesPath, const Cards& cards) {
  CardsDueDates cardsDueDates;
  try {
    File fp{cardsDueDatesPath, "r"};
    std::cout << "Reading cards due dates..." << std::endl;
    char readBuffer[65536];
    rapidjson::FileReadStream is{fp.getHandle(), readBuffer, sizeof(readBuffer)};

    CardsDueDatesReader handler{cardsDueDates, cards};
    rapidjson::Reader reader;
    handler.checkResult(reader.Parse(is, handler), fp);
    fp.close();
  } catch (const FileNotFoundException&) {
    std::cout << "Cards due dates file not found!" << std::endl;
  }

  return cardsDueDates;
}

void writeCardsDueDate(const char* cardsDueDatesPath, const CardsDueDates& cardsDueDates) {
  File fp{cardsDueDatesPath, "w"};
  char writeBuffer[65536];
  rapidjson::FileWriteStream os{fp.getHandle(), writeBuffer, sizeof(writeBuffer)};
  rapidjson::PrettyWriter writer{os};
  writer.SetIndent(' ', 0);
  writeCardsDueDates(cardsDueDates, writer);
  fp.close();
}


