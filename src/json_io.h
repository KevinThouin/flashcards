#ifndef JSON_IO_H
#define JSON_IO_H

#include "card.h"
#include "due_dates_statistics.h"

Cards readCards(const char* cardsPath);
CardsDueDates readCardsDueDates(const char* cardsDueDatesPath, const Cards& cards, DueDatesStatistics& dueDatesStatistics);
void writeCardsDueDate(const char* cardsDueDatesPath, const CardsDueDates& cardsDueDates);

#endif
