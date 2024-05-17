#include "card.h"
#include "json_io.h"

#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <new>
#include <iostream>
#include <limits>
#include <signal.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>

void addNewCards(CardsDueDates& cardsDueDates, const Cards& cards) {
  std::unordered_set<std::string_view> presentCards;
  for (const auto& card : cardsDueDates.getDueCards()) {
    presentCards.insert(card.get().title());
  }
  for (const auto& [_, card] : cardsDueDates.getOtherCards()) {
    presentCards.insert(card.get().title());
  }

  for (const auto& card : cards) {
    if (!presentCards.contains(card.title())) {
      cardsDueDates.addDueCard(card);
    }
  }
}

auto readCardsData(const char* cardsPath, const char* cardsDueDatesPath) {
  Cards cards = readCards(cardsPath);
  CardsDueDates cardsDueDates = readCardsDueDates(cardsDueDatesPath, cards);
  addNewCards(cardsDueDates, cards);
  cardsDueDates.shuffleDueCards();
  return std::make_pair(std::move(cards), std::move(cardsDueDates));
}

struct CommandLineArguments {
  std::string cardsPath;
  std::string cardsDueDatesPath;
  bool isAskingForHelp = false;

  bool validate() {return !cardsPath.empty();}
};

void waitForNewline() {
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

int getPositiveInteger() {
  int ret;
  std::cin >> ret;
  if (std::cin.fail()) {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    ret = -1;
  }

  return ret;
}

int getNumberOfDaysToReshowCard() {
  int ret = -1;
  while (ret < 0) {
    std::cout << "Dans combien de jour réafficher la carte?\n";
    ret = getPositiveInteger();
  }
  waitForNewline();
  std::cout << std::endl;
  return ret;
}

int showCard(const Card& card) {
  std::cout << "\033[2J\033[1;1H"; // Clear screen
  std::cout << card.firstSide() << std::endl;
  waitForNewline();
  std::cout << card.secondSide() << "\n\n";
  return getNumberOfDaysToReshowCard();
}

CommandLineArguments parseCommandLineArgument(int argc, char** argv) {
  CommandLineArguments args;
  int argNumber = 0;
  while (--argc) {
    argv = &argv[1];
    if (!strcmp(argv[0], "--help") || !strcmp(argv[0], "-h")) args.isAskingForHelp = true;
    else if (argNumber == 0) args.cardsPath = argv[0];
    else if (argNumber == 1) args.cardsDueDatesPath = argv[0];
    ++argNumber;
  }
  return args;
}

std::string getDueDatesPathFromCardsPath(const std::string& cardsPath) {
  std::filesystem::path path{cardsPath};
  std::filesystem::path directory = path.parent_path();
  std::string filename = path.stem().string();
  filename += "_due_dates.json";
  return (directory / std::filesystem::path{filename}).string();
}

void usage(const char* executablePath) {
  std::cout << std::format(
      "Usage: {} cards_path [cards_due_dates_path]\n"
      "    if `cards_due_dates_path` is not present it is made from `cards_path` by removing its extention and appending `_due_dates.json`.",
      executablePath) << std::endl;
}

volatile sig_atomic_t gShouldExit = 0;

void pickAndShowCard(CardsDueDates& cardsDueDates) {
  auto card = cardsDueDates.pickNewCard();
  if (!card.has_value()) {
    std::cout << "Il n'y a plus de carte à afficher!" << std::endl;
    gShouldExit = true;
    return;
  }

  int nextDueTime = showCard((**card).get());
  cardsDueDates.putbackCard(*card, nextDueTime);
}

void triggerExitSignalHandler([[maybe_unused]] int signalNumber) {
  gShouldExit = 1;
}

void setupTriggerExitSignalHandler() {
  struct sigaction sa{};
  sa.sa_handler = &triggerExitSignalHandler;
  for (int signal : {SIGINT, SIGTERM}) {
    if (sigaction(signal, &sa, nullptr)) {
      throw std::runtime_error("Failed to setup signal handler!");
    }
  }
}

int main(int argc, char** argv) {
  try {
    CommandLineArguments args = parseCommandLineArgument(argc, argv);
    if (!args.validate() || args.isAskingForHelp) {
      usage(argc > 0 ? argv[0] : "flashcards");
      return EXIT_SUCCESS;
    }

    if (args.cardsDueDatesPath.empty())
      args.cardsDueDatesPath = getDueDatesPathFromCardsPath(args.cardsPath);

    auto [cards, cardsDueDates] = readCardsData(args.cardsPath.c_str(), args.cardsDueDatesPath.c_str());

    setupTriggerExitSignalHandler();

    std::cin.exceptions(std::istream::badbit | std::istream::eofbit);

    try {
      if (!gShouldExit) waitForNewline();
      while (!gShouldExit) {
        pickAndShowCard(cardsDueDates);
      }
    } catch (const std::ios_base::failure& e) {
      if (std::cin.bad()) {
        std::cout << "Unexpected error while reading from standard input!" << std::endl;
        throw;
      }
    }
  
    std::cout << "Updating cards due date..." << std::endl;
    writeCardsDueDate(args.cardsDueDatesPath.c_str(), cardsDueDates);
  } catch (const std::exception& e) {
    std::cout << e.what() << std::endl;
  }
  return EXIT_SUCCESS;
}
