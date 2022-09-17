// Scintilla source code edit control
/** @file LexR.cxx
 ** Lexer for R, S, SPlus Statistics Program (Heavily derived from CPP Lexer).
 **
 **/
// Copyright 1998-2002 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <cassert>
#include <cctype>

#include <string>
#include <string_view>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "LexerModule.h"

using namespace Lexilla;

namespace {

inline bool IsAWordChar(int ch) noexcept {
	return (ch < 0x80) && (isalnum(ch) || ch == '.' || ch == '_');
}

inline bool IsAWordStart(int ch) noexcept {
	return (ch < 0x80) && (isalnum(ch) || ch == '_');
}

constexpr bool IsAnOperator(int ch) noexcept {
	// '.' left out as it is used to make up numbers
	if (ch == '-' || ch == '+' || ch == '!' || ch == '~' ||
	        ch == '?' || ch == ':' || ch == '*' || ch == '/' ||
	        ch == '^' || ch == '<' || ch == '>' || ch == '=' ||
	        ch == '&' || ch == '|' || ch == '$' || ch == '(' ||
	        ch == ')' || ch == '}' || ch == '{' || ch == '[' ||
		ch == ']')
		return true;
	return false;
}

constexpr bool IsHexDigit(int ch) noexcept {
	return (ch >= '0' && ch <= '9')
		|| (ch >= 'A' && ch <= 'F')
		|| (ch >= 'a' && ch <= 'f');
}

// https://search.r-project.org/R/refmans/base/html/Quotes.html
int CheckRawString(LexAccessor &styler, Sci_Position pos, int &dashCount) {
	dashCount = 0;
	while (true) {
		const char ch = styler.SafeGetCharAt(pos++);
		switch (ch) {
		case '-':
			++dashCount;
			break;
		case '(':
			return ')';
		case '[':
			return ']';
		case '{':
			return '}';
		default:
			dashCount = 0;
			return 0;
		}
	}
}

void ColouriseRDoc(Sci_PositionU startPos, Sci_Position length, int initStyle, WordList *keywordlists[],
                            Accessor &styler) {

	const WordList &keywords   = *keywordlists[0];
	const WordList &keywords2 = *keywordlists[1];
	const WordList &keywords3 = *keywordlists[2];
	// state for raw string
	int matchingDelimiter = 0;
	int dashCount = 0;

	// Do not leak onto next line
	if (initStyle == SCE_R_INFIXEOL) {
		initStyle = SCE_R_DEFAULT;
	}

	StyleContext sc(startPos, length, initStyle, styler);
	if (sc.currentLine > 0) {
		const int lineState = styler.GetLineState(sc.currentLine - 1);
		matchingDelimiter = lineState & 0xff;
		dashCount = lineState >> 8;
	}

	for (; sc.More(); sc.Forward()) {
		// Determine if the current state should terminate.
		switch (sc.state) {
		case SCE_R_OPERATOR:
			sc.SetState(SCE_R_DEFAULT);
			break;

		case SCE_R_NUMBER:
			// https://cran.r-project.org/doc/manuals/r-release/R-lang.html#Literal-constants
			if (AnyOf(sc.ch, 'e', 'E', 'p', 'P') && (IsADigit(sc.chNext) || sc.chNext == '+' || sc.chNext == '-')) {
				sc.Forward(); // exponent part
			} else if (!(IsHexDigit(sc.ch) || (sc.ch == '.' && IsADigit(sc.chNext)))) {
				if (AnyOf(sc.ch, 'L', 'i')) {
					sc.Forward(); // integer and complex qualifier
				}
				sc.SetState(SCE_R_DEFAULT);
			}
			break;

		case SCE_R_IDENTIFIER:
			if (!IsAWordChar(sc.ch)) {
				char s[100];
				sc.GetCurrent(s, sizeof(s));
				if (keywords.InList(s)) {
					sc.ChangeState(SCE_R_KWORD);
				} else if  (keywords2.InList(s)) {
					sc.ChangeState(SCE_R_BASEKWORD);
				} else if  (keywords3.InList(s)) {
					sc.ChangeState(SCE_R_OTHERKWORD);
				}
				sc.SetState(SCE_R_DEFAULT);
			}
			break;

		case SCE_R_COMMENT:
			if (sc.MatchLineEnd()) {
				sc.SetState(SCE_R_DEFAULT);
			}
			break;

		case SCE_R_STRING:
		case SCE_R_STRING2:
		case SCE_R_BACKTICKS:
			if (sc.ch == '\\') {
				if (sc.chNext == '\"' || sc.chNext == '\'' || sc.chNext == '\\' || sc.chNext == '`') {
					sc.Forward();
				}
			} else if ((sc.state == SCE_R_STRING && sc.ch == '\"')
				|| (sc.state == SCE_R_STRING2 && sc.ch == '\'')
				|| (sc.state == SCE_R_BACKTICKS && sc.ch == '`')) {
				sc.ForwardSetState(SCE_R_DEFAULT);
			}
			break;

		case SCE_R_RAWSTRING:
		case SCE_R_RAWSTRING2:
			while (sc.ch == matchingDelimiter) {
				sc.Forward();
				int count = dashCount;
				while (count != 0 && sc.ch == '-') {
					--count;
					sc.Forward();
				}
				if (count == 0 && sc.ch == ((sc.state == SCE_R_RAWSTRING) ? '\"' : '\'')) {
					matchingDelimiter = 0;
					dashCount = 0;
					sc.ForwardSetState(SCE_R_DEFAULT);
					break;
				}
			}
			break;

		case SCE_R_INFIX:
			if (sc.ch == '%') {
				sc.ForwardSetState(SCE_R_DEFAULT);
			} else if (sc.atLineEnd) {
				sc.ChangeState(SCE_R_INFIXEOL);
				sc.ForwardSetState(SCE_R_DEFAULT);
			}
			break;
		}

		// Determine if a new state should be entered.
		if (sc.state == SCE_R_DEFAULT) {
			if (IsADigit(sc.ch) || (sc.ch == '.' && IsADigit(sc.chNext))) {
				sc.SetState(SCE_R_NUMBER);
				if (sc.ch == '0' && AnyOf(sc.chNext, 'x', 'X')) {
					sc.Forward();
				}
			} else if (AnyOf(sc.ch, 'r', 'R') && AnyOf(sc.chNext, '\"', '\'')) {
				const int chNext = sc.chNext;
				matchingDelimiter = CheckRawString(styler, sc.currentPos + 2, dashCount);
				if (matchingDelimiter) {
					sc.SetState((chNext == '\"') ? SCE_R_RAWSTRING : SCE_R_RAWSTRING2);
					sc.Forward(dashCount + 2);
				} else {
					// syntax error
					sc.SetState(SCE_R_IDENTIFIER);
					sc.ForwardSetState((chNext == '\"') ? SCE_R_STRING : SCE_R_STRING2);
				}
			} else if (IsAWordStart(sc.ch) ) {
				sc.SetState(SCE_R_IDENTIFIER);
			} else if (sc.Match('#')) {
				sc.SetState(SCE_R_COMMENT);
			} else if (sc.ch == '\"') {
				sc.SetState(SCE_R_STRING);
			} else if (sc.ch == '%') {
				sc.SetState(SCE_R_INFIX);
			} else if (sc.ch == '\'') {
				sc.SetState(SCE_R_STRING2);
			} else if (sc.ch == '`') {
				sc.SetState(SCE_R_BACKTICKS);
			} else if (IsAnOperator(sc.ch)) {
				sc.SetState(SCE_R_OPERATOR);
			}
		}

		if (sc.atLineEnd) {
			const int lineState = matchingDelimiter | (dashCount << 8);
			styler.SetLineState(sc.currentLine, lineState);
		}
	}
	sc.Complete();
}

// Store both the current line's fold level and the next lines in the
// level store to make it easy to pick up with each increment
// and to make it possible to fiddle the current level for "} else {".
void FoldRDoc(Sci_PositionU startPos, Sci_Position length, int, WordList *[],
                       Accessor &styler) {
	const bool foldCompact = styler.GetPropertyInt("fold.compact", 1) != 0;
	const bool foldAtElse = styler.GetPropertyInt("fold.at.else", 0) != 0;
	const Sci_PositionU endPos = startPos + length;
	int visibleChars = 0;
	Sci_Position lineCurrent = styler.GetLine(startPos);
	int levelCurrent = SC_FOLDLEVELBASE;
	if (lineCurrent > 0)
		levelCurrent = styler.LevelAt(lineCurrent-1) >> 16;
	int levelMinCurrent = levelCurrent;
	int levelNext = levelCurrent;
	char chNext = styler[startPos];
	int styleNext = styler.StyleAt(startPos);
	for (Sci_PositionU i = startPos; i < endPos; i++) {
		const char ch = chNext;
		chNext = styler.SafeGetCharAt(i + 1);
		const int style = styleNext;
		styleNext = styler.StyleAt(i + 1);
		const bool atEOL = (ch == '\r' && chNext != '\n') || (ch == '\n');
		if (style == SCE_R_OPERATOR) {
			if (ch == '{') {
				// Measure the minimum before a '{' to allow
				// folding on "} else {"
				if (levelMinCurrent > levelNext) {
					levelMinCurrent = levelNext;
				}
				levelNext++;
			} else if (ch == '}') {
				levelNext--;
			}
		}
		if (atEOL) {
			int levelUse = levelCurrent;
			if (foldAtElse) {
				levelUse = levelMinCurrent;
			}
			int lev = levelUse | levelNext << 16;
			if (visibleChars == 0 && foldCompact)
				lev |= SC_FOLDLEVELWHITEFLAG;
			if (levelUse < levelNext)
				lev |= SC_FOLDLEVELHEADERFLAG;
			if (lev != styler.LevelAt(lineCurrent)) {
				styler.SetLevel(lineCurrent, lev);
			}
			lineCurrent++;
			levelCurrent = levelNext;
			levelMinCurrent = levelCurrent;
			visibleChars = 0;
		}
		if (!isspacechar(ch))
			visibleChars++;
	}
}


const char * const RWordLists[] = {
		"Language Keywords",
		"Base / Default package function",
		"Other Package Functions",
		"Unused",
		"Unused",
		nullptr,
};

}

LexerModule lmR(SCLEX_R, ColouriseRDoc, "r", FoldRDoc, RWordLists);
