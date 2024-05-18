#ifndef CARD_H
#define CARD_H

#include <chrono>
#include <set>
#include <string>
#include <string_view>
#include <list>
#include <optional>
#include <unordered_set>

class Card {
  std::string mTitle;
  std::string mFirstSide;
  std::string mSecondSide;

public:
  Card(std::string&& title, std::string&& firstSide, std::string&& secondSide)
    : mTitle(std::move(title)), mFirstSide(std::move(firstSide)), mSecondSide(std::move(secondSide)) {}

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
  struct SetCompare {
    using Type = std::pair<std::chrono::year_month_day, CardReference>;
    bool operator()(const Type& a, const Type& b) const {
      auto firstCompare = a.first <=> b.first;
      return (firstCompare == 0) ? a.second.get().title() < b.second.get().title() : firstCompare < 0;
    }
  };

  std::list<CardReference> mDueCards;
  std::multiset<std::pair<std::chrono::year_month_day, CardReference>, SetCompare> mOtherCards;
  std::chrono::year_month_day mToday;

public:
  CardsDueDates();

  const auto& getDueCards() const {return mDueCards;}
  const auto& getOtherCards() const {return mOtherCards;}
  const auto& getToday() const {return mToday;}

  void addDueCard(CardReference card);
  void addCard(CardReference card, const std::chrono::year_month_day& dueDate);
  std::optional<std::list<CardReference>::const_iterator> pickNewCard() const;
  void putbackCard(std::list<CardReference>::const_iterator it, int nextDueDays);
  void shuffleDueCards();
};

#endif

