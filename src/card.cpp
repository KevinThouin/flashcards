#include <card.h>

#include <algorithm>
#include <random>
#include <vector>

bool Cards::registerCard(Card&& card) {
  return mCards.emplace(std::move(card)).second;
}

const Card* Cards::getCard(const std::string& title) const {
  auto it = mCards.find(title);
  return (it == mCards.end()) ? (const Card*)nullptr : std::addressof(*it);
}

CardsDueDates::CardsDueDates() {
  auto now = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()}.get_local_time();
  mToday = std::chrono::year_month_day{std::chrono::time_point_cast<std::chrono::days>(now)};
}

void CardsDueDates::addDueCard(CardReference card) {
  mDueCards.push_back(card);
}

void CardsDueDates::addCard(CardReference card, const std::chrono::year_month_day& dueDate) {
  if (dueDate <= mToday) {
    mDueCards.push_back(card);
  } else {
    mOtherCards.insert({dueDate, card});
  }
}

std::optional<std::list<CardReference>::const_iterator> CardsDueDates::pickNewCard() const {
  if (!mDueCards.empty()) {
    return std::make_optional(mDueCards.begin());
  } else {
    return decltype(std::make_optional(mDueCards.begin())){};
  }
}

void CardsDueDates::putbackCard(std::list<CardReference>::const_iterator it, int nextDueDays) {
  if (nextDueDays <= 0) {
    mDueCards.splice(mDueCards.cend(), mDueCards, it);
  } else {
    std::chrono::year_month_day dueDate = static_cast<std::chrono::sys_days>(mToday) + std::chrono::days(nextDueDays);
    mOtherCards.insert({dueDate, *it});
    mDueCards.erase(it);
  }
}

void CardsDueDates::shuffleDueCards() {
  std::vector<decltype(mDueCards.cbegin())> iterators{};
  iterators.reserve(mDueCards.size());
  for (auto it = mDueCards.cbegin(); it != mDueCards.cend(); ++it) {
    iterators.push_back(it);
  }

  std::mt19937 randomGenerator{std::random_device{}()};
  std::shuffle(iterators.begin(), iterators.end(), randomGenerator);

  for (auto it : iterators) {
    mDueCards.splice(mDueCards.cend(), mDueCards, it);
  }
}

