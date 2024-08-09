#include "card.h"
#include "json_io.h"
#include "due_dates_statistics.h"

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

void addNewCardsAndCheckDuplicatesInDueDates(CardsDueDates& cardsDueDates, const Cards& cards, unsigned int allowedNewCardsCount) {
  std::unordered_set<std::string_view> presentCards;
  for (const auto& [card, _] : cardsDueDates.getDueCards()) {
    if (!presentCards.insert(card.get().title()).second) {
      throw std::runtime_error(std::format("Card `{}` is already present", card.get().title()));
    }
  }
  for (const auto& [_0, card, _1] : cardsDueDates.getOtherCards()) {
    if (!presentCards.insert(card.get().title()).second) {
      throw std::runtime_error(std::format("Card `{}` is already present", card.get().title()));
    }
  }

  for (const auto& card : cards) {
    if (!presentCards.contains(card.title())) {
      cardsDueDates.addDueCard(card, -1);
      if (--allowedNewCardsCount == 0) break;
    }
  }
}

auto readCardsData(const char* cardsPath, const char* cardsDueDatesPath, unsigned int maxNewCardCount) {
  Cards cards = readCards(cardsPath);
  DueDatesStatistics dueDatesStatistics;
  CardsDueDates cardsDueDates = readCardsDueDates(cardsDueDatesPath, cards, dueDatesStatistics);
  dueDatesStatistics.print(std::cout);
  addNewCardsAndCheckDuplicatesInDueDates(cardsDueDates, cards, maxNewCardCount);
  cardsDueDates.shuffleDueCards();
  return std::make_pair(std::move(cards), std::move(cardsDueDates));
}

struct CommandLineArguments {
  std::string cardsPath;
  std::string cardsDueDatesPath;
  unsigned int maxNewCardCount = std::numeric_limits<unsigned int>::max();
  bool isAskingForHelp = false;
  bool isReversed = false;

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

int showCard(const Card& card, int numberOfDaysSinceLastTime, bool isReversed) {
  std::cout << "\033[2J\033[1;1H"; // Clear screen
  std::cout << ( isReversed ? card.secondSide() : card.firstSide() ) << std::endl;
  if (numberOfDaysSinceLastTime > 0) {
    std::cout << "(Montrée la dernière fois il y a " << numberOfDaysSinceLastTime << " jours)" << std::endl;
  } else if (numberOfDaysSinceLastTime == 0) {
    std::cout << "(Montrée la dernière fois aujourd'hui)" << std::endl;
  }
  waitForNewline();
  std::cout << ( isReversed ? card.firstSide() : card.secondSide() ) << "\n\n";
  return getNumberOfDaysToReshowCard();
}

CommandLineArguments parseCommandLineArgument(int argc, char** argv) {
  CommandLineArguments args;
  while (--argc) {
    argv = &argv[1];
    if (!strcmp(argv[0], "--help") || !strcmp(argv[0], "-h")) args.isAskingForHelp = true;
    else if (!strcmp(argv[0], "-r")) args.isReversed = true;
    else if (argv[0][0] == '-' && argv[0][1] == 'n') {
      unsigned long count = std::numeric_limits<unsigned long>::max();
      char* end;
      if (argv[0][2] >= '0' && argv[0][2] <= '9') {
        count = strtoul(&argv[0][2], &end, 10);
      } else if (argc > 0) {
        argv = &argv[1]; --argc;
        count = strtoul(&argv[0][2], &end, 10);
      }
      if (count >= std::numeric_limits<unsigned int>::max() || *end != '\0') args.isAskingForHelp = true;
      args.maxNewCardCount = static_cast<unsigned int>(count);
    }
    else if (args.cardsPath.empty()) args.cardsPath = argv[0];
    else args.cardsDueDatesPath = argv[0];
  }
  return args;
}

std::string getDueDatesPathFromCardsPath(const std::string& cardsPath, bool isReversed) {
  std::filesystem::path path{cardsPath};
  std::filesystem::path directory = path.parent_path();
  std::string filename = path.stem().string();
  filename += isReversed ? "_reversed_due_dates.json" : "_due_dates.json";
  return (directory / std::filesystem::path{filename}).string();
}

void usage(const char* executablePath) {
  std::cout << std::format(
      "Usage: {} cards_path [cards_due_dates_path] [-r]\n"
      "    -r  flip the side of the cards when showing\n"
      "    if `cards_due_dates_path` is not present it is made from `cards_path` by removing its extention and appending `_due_dates.json`.",
      executablePath) << std::endl;
}

volatile sig_atomic_t gShouldExit = 0;

void pickAndShowCard(CardsDueDates& cardsDueDates, bool isReversed) {
  auto card = cardsDueDates.pickNewCard();
  if (!card.has_value()) {
    std::cout << "Il n'y a plus de carte à afficher!" << std::endl;
    gShouldExit = true;
    return;
  }

  int nextDueTime = showCard((**card).first.get(), (**card).second, isReversed);
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
      args.cardsDueDatesPath = getDueDatesPathFromCardsPath(args.cardsPath, args.isReversed);

    auto [cards, cardsDueDates] = readCardsData(args.cardsPath.c_str(), args.cardsDueDatesPath.c_str(), args.maxNewCardCount);

    setupTriggerExitSignalHandler();

    std::cin.exceptions(std::istream::badbit | std::istream::eofbit);

    try {
      if (!gShouldExit) waitForNewline();
      while (!gShouldExit) {
        pickAndShowCard(cardsDueDates, args.isReversed);
      }
    } catch (const std::ios_base::failure& e) {
      if (std::cin.bad()) {
        std::cout << "Unexpected error while reading from standard input!" << std::endl;
        throw;
      }
    }
  
    if (cardsDueDates.isDirty()) {
      std::cout << "Updating cards due date..." << std::endl;
      writeCardsDueDate(args.cardsDueDatesPath.c_str(), cardsDueDates);
    }
  } catch (const std::exception& e) {
    std::cout << e.what() << std::endl;
  }
  return EXIT_SUCCESS;
}
