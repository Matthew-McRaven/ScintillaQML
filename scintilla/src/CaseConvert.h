#pragma once
// Scintilla source code edit control
// Encoding: UTF-8
/** @file CaseConvert.h
 ** Performs Unicode case conversions.
 ** Does not handle locale-sensitive case conversion.
 **/
// Copyright 2013 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <string>
#include "scintilla_globals.h"

namespace Scintilla::Internal {

enum class CaseConversion { fold, upper, lower };

class SCINTILLA_EXPORT ICaseConverter {
public:
  virtual size_t CaseConvertString(char *converted, size_t sizeConverted, const char *mixed, size_t lenMixed) = 0;
};

SCINTILLA_EXPORT ICaseConverter *ConverterFor(CaseConversion conversion);

// Returns a UTF-8 string. Empty when no conversion
SCINTILLA_EXPORT const char *CaseConvert(int character, CaseConversion conversion);

// When performing CaseConvertString, the converted value may be up to 3 times longer than the input.
// Ligatures are often decomposed into multiple characters and long cases include:
// ΐ "\xce\x90" folds to ΐ "\xce\xb9\xcc\x88\xcc\x81"
constexpr size_t maxExpansionCaseConversion = 3;

// Converts a mixed case string using a particular conversion.
// Result may be a different length to input and the length is the return value.
// If there is not enough space then 0 is returned.
SCINTILLA_EXPORT size_t CaseConvertString(char *converted, size_t sizeConverted, const char *mixed, size_t lenMixed,
                                          CaseConversion conversion);

// Converts a mixed case string using a particular conversion.
SCINTILLA_EXPORT std::string CaseConvertString(const std::string &s, CaseConversion conversion);
} // namespace Scintilla::Internal
