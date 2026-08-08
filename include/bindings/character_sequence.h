// character_sequence.h
#ifndef CHARACTER_SEQUENCE_H
#define CHARACTER_SEQUENCE_H

#include <stdbool.h>

void SequenceAddKey(char key, bool was_typed);
void SequenceReset(void);
int SequenceLen(void);
int SequenceTypedCount(void);
bool SequenceMatches(const char* pattern);
bool SequenceStartsWith(const char* pattern);
void SequenceStartTimer(void);
bool SequenceIsWaiting(void);
bool SequenceTimedOut(void);
void DeleteLastTypedChar(void);

#endif
