#include <iomanip>

#include "due_dates_statistics.h"

void DueDatesStatistics::addCard(int mNumberOfDays) {
  ++mNumberOfCardsByDays[mNumberOfDays];
  mCumulativeNumberOfDays += mNumberOfDays;
  ++mNumberOfCards;
  mLongestNumberOfDays = std::max(mLongestNumberOfDays, mNumberOfDays);
}

void DueDatesStatistics::print(std::ostream& stream) const {
  if (mNumberOfCards == 0) return;
  stream << "\033[2J\033[1;1H"; // Clear screen
  for (auto [numberOfDays, numberOfCards] : mNumberOfCardsByDays) {
    stream << "Cartes dues dans " << numberOfDays << " jours: " << numberOfCards << '\n';
  }
  float averageNumberOfDaysToShowCard = ((float)mCumulativeNumberOfDays)/((float)mNumberOfCards);
  stream << "\nNombre de jours moyen pour afficher chaque carte: " << std::fixed << std::setprecision(1) << averageNumberOfDaysToShowCard
    << "\nPlus long nombre de jours pour afficher une carte: " << mLongestNumberOfDays << std::endl;
}
