#ifndef DUE_DATES_STATISTICS
#define DUE_DATES_STATISTICS

#include <limits>
#include <map>
#include <ostream>

class DueDatesStatistics {
  std::map<int, unsigned int> mNumberOfCardsByDays;
  int mCumulativeNumberOfDays = 0;
  int mNumberOfCards = 0;
  int mLongestNumberOfDays = std::numeric_limits<int>::min();

public:
  void addCard(int mNumberOfDays);

  void print(std::ostream& stream) const;
};

#endif
