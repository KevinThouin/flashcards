#ifndef JSON_IO_H
#define JSON_IO_H

#include "card.h"

Cards readCards(const char* cardsPath);
CardsDueDates readCardsDueDates(const char* cardsDueDatesPath, const Cards& cards);
void writeCardsDueDate(const char* cardsDueDatesPath, const CardsDueDates& cardsDueDates);

#endif
