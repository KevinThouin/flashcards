#ifndef CARD_H
#define CARD_H

#include <chrono>
#include <set>
#include <string>
#include <string_view>
#include <list>
#include <optional>
#include <unordered_set>
#include <tuple>

class Card {
  std::string mTitle;
  std::string mFirstSide;
  std::string mSecondSide;

public:
  Card(const std::string& title, std::string&& firstSide, std::string&& secondSide)
    : mTitle(title), mFirstSide(std::move(firstSide)), mSecondSide(std::move(secondSide)) {}

  const std::string& title() const {return mTitle;}
  const std::string& firstSide() const {return mFirstSide;}
  const std::string& secondSide() const {return mSecondSide;}
};

struct CardTitleEqual {
  using is_transparent = void;
  bool operator()(const Card& a, const Card& b) const noexcept {return a.title() == b.title();}
  bool operator()(const Card& card, const std::string_view& title) const noexcept {return card.title() == title;}
  bool operator()(const std::string_view& title, const Card& card) const noexcept {return card.title() == title;}
};

struct CardTitleHash {
  using is_transparent = void;
  std::size_t operator()(const Card& card) const noexcept {return std::hash<std::string_view>{}(card.title());}
  std::size_t operator()(const std::string_view& title) const noexcept {return std::hash<std::string_view>{}(title);}
};

using CardReference = std::reference_wrapper<const Card>;

class Cards {
  std::unordered_set<Card, CardTitleHash, CardTitleEqual> mCards;

public:
  bool registerCard(Card&& card);

  const Card* getCard(const std::string& title) const;

  auto begin() const {return mCards.begin();}
  auto end() const {return mCards.end();}
};

class CardsDueDates {
  using Type = std::tuple<std::chrono::year_month_day, CardReference, int>;

  struct SetCompare {
    bool operator()(const Type& a, const Type& b) const {
      auto firstCompare = std::get<0>(a) <=> std::get<0>(b);
      return (firstCompare == 0) ? std::get<1>(a).get().title() < std::get<1>(b).get().title() : firstCompare < 0;
    }
  };

  std::list<std::pair<CardReference, int>> mDueCards;
  std::multiset<Type, SetCompare> mOtherCards;
  std::chrono::year_month_day mToday;

public:
  CardsDueDates();

  const auto& getDueCards() const {return mDueCards;}
  const auto& getOtherCards() const {return mOtherCards;}
  const auto& getToday() const {return mToday;}

  void addDueCard(CardReference card, int numberOfDaysSinceLastTime);
  void addCard(CardReference card, const std::chrono::year_month_day& dueDate, int numberOfDaysSinceLastTime = 0);
  std::optional<std::list<std::pair<CardReference, int>>::iterator> pickNewCard();
  void putbackCard(std::list<std::pair<CardReference, int>>::iterator it, int nextDueDays);
  void shuffleDueCards();
};

#endif

