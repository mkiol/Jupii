/* Copyright (C) 2006-2016 J.F.Dockes
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *   02110-1301 USA
 */
#ifndef _SMALLUT_H_INCLUDED_
#define _SMALLUT_H_INCLUDED_

#include <sys/types.h>
#include <stdint.h>

#include <string>
#include <vector>
#include <map>
#include <set>

// Miscellaneous mostly string-oriented small utilities
// Note that none of the following code knows about utf-8.

// Call this before going multithread.
void smallut_init_mt();

#ifndef SMALLUT_DISABLE_MACROS
#ifndef MIN
#define MIN(A,B) (((A)<(B)) ? (A) : (B))
#endif
#ifndef MAX
#define MAX(A,B) (((A)>(B)) ? (A) : (B))
#endif
#ifndef deleteZ
#define deleteZ(X) {delete X;X = 0;}
#endif
#endif /* SMALLUT_DISABLE_MACROS */

// Case-insensitive compare. ASCII ONLY !
extern int stringicmp(const std::string& s1, const std::string& s2);

// For find_if etc.
struct StringIcmpPred {
    StringIcmpPred(const std::string& s1)
        : m_s1(s1) {
    }
    bool operator()(const std::string& s2) {
        return stringicmp(m_s1, s2) == 0;
    }
    const std::string& m_s1;
};

extern int stringlowercmp(const std::string& alreadylower,
                          const std::string& s2);
extern int stringuppercmp(const std::string& alreadyupper,
                          const std::string& s2);

extern void stringtolower(std::string& io);
extern std::string stringtolower(const std::string& io);
extern void stringtoupper(std::string& io);
extern std::string stringtoupper(const std::string& io);
extern bool beginswith(const std::string& big, const std::string& small);

// Is one string the end part of the other ?
extern int stringisuffcmp(const std::string& s1, const std::string& s2);

// Divine language from locale
extern std::string localelang();
// Divine 8bit charset from language
extern std::string langtocode(const std::string& lang);

// Compare charset names, removing the more common spelling variations
extern bool samecharset(const std::string& cs1, const std::string& cs2);

// Parse date interval specifier into pair of y,m,d dates.  The format
// for the time interval is based on a subset of iso 8601 with
// the addition of open intervals, and removal of all time indications.
// 'P' is the Period indicator, it's followed by a length in
// years/months/days (or any subset thereof)
// Dates: YYYY-MM-DD YYYY-MM YYYY
// Periods: P[nY][nM][nD] where n is an integer value.
// At least one of YMD must be specified
// The separator for the interval is /. Interval examples
// YYYY/ (from YYYY) YYYY-MM-DD/P3Y (3 years after date) etc.
// This returns a pair of y,m,d dates.
struct DateInterval {
    int y1;
    int m1;
    int d1;
    int y2;
    int m2;
    int d2;
};
extern bool parsedateinterval(const std::string& s, DateInterval *di);
extern int monthdays(int mon, int year);

/**
 * Parse input string into list of strings.
 *
 * Token delimiter is " \t\n" except inside dquotes. dquote inside
 * dquotes can be escaped with \ etc...
 * Input is handled a byte at a time, things will work as long as
 * space tab etc. have the ascii values and can't appear as part of a
 * multibyte char. utf-8 ok but so are the iso-8859-x and surely
 * others. addseps do have to be single-bytes
 */
template <class T> bool stringToStrings(const std::string& s, T& tokens,
                                        const std::string& addseps = "");

/**
 * Inverse operation:
 */
template <class T> void stringsToString(const T& tokens, std::string& s);
template <class T> std::string stringsToString(const T& tokens);

/**
 * Strings to CSV string. tokens containing the separator are quoted (")
 * " inside tokens is escaped as "" ([word "quote"] =>["word ""quote"""]
 */
template <class T> void stringsToCSV(const T& tokens, std::string& s,
                                     char sep = ',');

/**
 * Split input string. No handling of quoting
 */
extern void stringToTokens(const std::string& s,
                           std::vector<std::string>& tokens,
                           const std::string& delims = " \t",
                           bool skipinit = true);

/** Convert string to boolean */
extern bool stringToBool(const std::string& s);

/** Remove instances of characters belonging to set (default {space,
    tab}) at beginning and end of input string */
extern void trimstring(std::string& s, const char *ws = " \t");
extern void rtrimstring(std::string& s, const char *ws = " \t");
extern void ltrimstring(std::string& s, const char *ws = " \t");

/** Escape things like < or & by turning them into entities */
extern std::string escapeHtml(const std::string& in);

/** Double-quote and escape to produce C source code string (prog generation) */
extern std::string makeCString(const std::string& in);

/** Replace some chars with spaces (ie: newline chars). */
extern std::string neutchars(const std::string& str, const std::string& chars);
extern void neutchars(const std::string& str, std::string& out,
                      const std::string& chars);

/** Turn string into something that won't be expanded by a shell. In practise
 *  quote with double-quotes and escape $`\ */
extern std::string escapeShell(const std::string& str);

/** Truncate a string to a given maxlength, avoiding cutting off midword
 *  if reasonably possible. */
extern std::string truncate_to_word(const std::string& input,
                                    std::string::size_type maxlen);

void ulltodecstr(uint64_t val, std::string& buf);
void lltodecstr(int64_t val, std::string& buf);
std::string lltodecstr(int64_t val);
std::string ulltodecstr(uint64_t val);

/** Convert byte count into unit (KB/MB...) appropriate for display */
std::string displayableBytes(int64_t size);

/** Break big string into lines */
std::string breakIntoLines(const std::string& in, unsigned int ll = 100,
                           unsigned int maxlines = 50);

/** Small utility to substitute printf-like percents cmds in a string */
bool pcSubst(const std::string& in, std::string& out,
             const std::map<char, std::string>& subs);
/** Substitute printf-like percents and also %(key) */
bool pcSubst(const std::string& in, std::string& out,
             const std::map<std::string, std::string>& subs);

/** Append system error message */
void catstrerror(std::string *reason, const char *what, int _errno);

/** Portable timegm. MS C has _mkgmtime, but there is a bug in Gminw which
 * makes it inaccessible */
struct tm;
time_t portable_timegm(struct tm *tm);

inline void leftzeropad(std::string& s, unsigned len)
{
    if (s.length() && s.length() < len) {
        s = s.insert(0, len - s.length(), '0');
    }
}

// A class to solve platorm/compiler issues for simple regex
// matches. Uses the appropriate native lib under the hood.
// This always uses extended regexp syntax.
class SimpleRegexp {
public:
    enum Flags {SRE_NONE = 0, SRE_ICASE = 1, SRE_NOSUB = 2};
    /// @param nmatch must be >= the number of parenthesed subexp in exp
    SimpleRegexp(const std::string& exp, int flags, int nmatch = 0);
    ~SimpleRegexp();
    /// Match input against exp, return true if matches
    bool simpleMatch(const std::string& val) const;
    /// After simpleMatch success, get nth submatch, 0 is the whole
    /// match, 1 first parentheses, etc.
    std::string getMatch(const std::string& val, int matchidx) const;
    /// Calls simpleMatch()
    bool operator() (const std::string& val) const;
    /// Check after construction
    bool ok() const;
    
    class Internal;
private:
    Internal *m;
};

/// Utilities for printing names for defined values (Ex: O_RDONLY->"O_RDONLY")

/// Entries for the descriptive table
struct CharFlags {
    unsigned int value; // Flag or value
    const char *yesname;// String to print if flag set or equal
    const char *noname; // String to print if flag not set (unused for values)
};

/// Helper macro for the common case where we want to print the
/// flag/value defined name
#define CHARFLAGENTRY(NM) {NM, #NM}

/// Translate a bitfield into string description
extern std::string flagsToString(const std::vector<CharFlags>&,
                                 unsigned int flags);

/// Translate a value into a name
extern std::string valToString(const std::vector<CharFlags>&, unsigned int val);

/// Reverse operation: translate string into bitfield
extern unsigned int
stringToFlags(const std::vector<CharFlags>&, const std::string& input,
              const char *sep = "|");

#endif /* _SMALLUT_H_INCLUDED_ */
