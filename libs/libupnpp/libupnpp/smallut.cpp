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
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <math.h>

// Older compilers don't support stdc++ regex, but Windows does not
// have the Linux one. Have a simple class to solve the simple cases.
#if defined(_WIN32)
#define USE_STD_REGEX
#include <regex>
#else
#define USE_LINUX_REGEX
#include <regex.h>
#endif

#include <string>
#include <iostream>
#include <list>
#include <unordered_map>
#include <unordered_set>

#include "smallut.h"

using namespace std;

int stringicmp(const string& s1, const string& s2)
{
    string::const_iterator it1 = s1.begin();
    string::const_iterator it2 = s2.begin();
    string::size_type size1 = s1.length(), size2 = s2.length();
    char c1, c2;

    if (size1 < size2) {
        while (it1 != s1.end()) {
            c1 = ::toupper(*it1);
            c2 = ::toupper(*it2);
            if (c1 != c2) {
                return c1 > c2 ? 1 : -1;
            }
            ++it1;
            ++it2;
        }
        return size1 == size2 ? 0 : -1;
    } else {
        while (it2 != s2.end()) {
            c1 = ::toupper(*it1);
            c2 = ::toupper(*it2);
            if (c1 != c2) {
                return c1 > c2 ? 1 : -1;
            }
            ++it1;
            ++it2;
        }
        return size1 == size2 ? 0 : 1;
    }
}
void stringtolower(string& io)
{
    string::iterator it = io.begin();
    string::iterator ite = io.end();
    while (it != ite) {
        *it = ::tolower(*it);
        it++;
    }
}
string stringtolower(const string& i)
{
    string o = i;
    stringtolower(o);
    return o;
}

void stringtoupper(string& io)
{
    string::iterator it = io.begin();
    string::iterator ite = io.end();
    while (it != ite) {
        *it = ::toupper(*it);
        it++;
    }
}
string stringtoupper(const string& i)
{
    string o = i;
    stringtoupper(o);
    return o;
}

extern int stringisuffcmp(const string& s1, const string& s2)
{
    string::const_reverse_iterator r1 = s1.rbegin(), re1 = s1.rend(),
                                   r2 = s2.rbegin(), re2 = s2.rend();
    while (r1 != re1 && r2 != re2) {
        char c1 = ::toupper(*r1);
        char c2 = ::toupper(*r2);
        if (c1 != c2) {
            return c1 > c2 ? 1 : -1;
        }
        ++r1;
        ++r2;
    }
    return 0;
}

//  s1 is already lowercase
int stringlowercmp(const string& s1, const string& s2)
{
    string::const_iterator it1 = s1.begin();
    string::const_iterator it2 = s2.begin();
    string::size_type size1 = s1.length(), size2 = s2.length();
    char c2;

    if (size1 < size2) {
        while (it1 != s1.end()) {
            c2 = ::tolower(*it2);
            if (*it1 != c2) {
                return *it1 > c2 ? 1 : -1;
            }
            ++it1;
            ++it2;
        }
        return size1 == size2 ? 0 : -1;
    } else {
        while (it2 != s2.end()) {
            c2 = ::tolower(*it2);
            if (*it1 != c2) {
                return *it1 > c2 ? 1 : -1;
            }
            ++it1;
            ++it2;
        }
        return size1 == size2 ? 0 : 1;
    }
}

//  s1 is already uppercase
int stringuppercmp(const string& s1, const string& s2)
{
    string::const_iterator it1 = s1.begin();
    string::const_iterator it2 = s2.begin();
    string::size_type size1 = s1.length(), size2 = s2.length();
    char c2;

    if (size1 < size2) {
        while (it1 != s1.end()) {
            c2 = ::toupper(*it2);
            if (*it1 != c2) {
                return *it1 > c2 ? 1 : -1;
            }
            ++it1;
            ++it2;
        }
        return size1 == size2 ? 0 : -1;
    } else {
        while (it2 != s2.end()) {
            c2 = ::toupper(*it2);
            if (*it1 != c2) {
                return *it1 > c2 ? 1 : -1;
            }
            ++it1;
            ++it2;
        }
        return size1 == size2 ? 0 : 1;
    }
}

bool beginswith(const std::string& big, const std::string& small)
{
    if (big.compare(0, small.size(), small)) {
        return false;
    }
    return true;
}

// Compare charset names, removing the more common spelling variations
bool samecharset(const string& cs1, const string& cs2)
{
    string mcs1, mcs2;
    // Remove all - and _, turn to lowecase
    for (unsigned int i = 0; i < cs1.length(); i++) {
        if (cs1[i] != '_' && cs1[i] != '-') {
            mcs1 += ::tolower(cs1[i]);
        }
    }
    for (unsigned int i = 0; i < cs2.length(); i++) {
        if (cs2[i] != '_' && cs2[i] != '-') {
            mcs2 += ::tolower(cs2[i]);
        }
    }
    return mcs1 == mcs2;
}

template <class T> bool stringToStrings(const string& s, T& tokens,
                                        const string& addseps)
{
    string current;
    tokens.clear();
    enum states {SPACE, TOKEN, INQUOTE, ESCAPE};
    states state = SPACE;
    for (unsigned int i = 0; i < s.length(); i++) {
        switch (s[i]) {
        case '"':
            switch (state) {
            case SPACE:
                state = INQUOTE;
                continue;
            case TOKEN:
                current += '"';
                continue;
            case INQUOTE:
                tokens.insert(tokens.end(), current);
                current.clear();
                state = SPACE;
                continue;
            case ESCAPE:
                current += '"';
                state = INQUOTE;
                continue;
            }
            break;
        case '\\':
            switch (state) {
            case SPACE:
            case TOKEN:
                current += '\\';
                state = TOKEN;
                continue;
            case INQUOTE:
                state = ESCAPE;
                continue;
            case ESCAPE:
                current += '\\';
                state = INQUOTE;
                continue;
            }
            break;

        case ' ':
        case '\t':
        case '\n':
        case '\r':
            switch (state) {
            case SPACE:
                continue;
            case TOKEN:
                tokens.insert(tokens.end(), current);
                current.clear();
                state = SPACE;
                continue;
            case INQUOTE:
            case ESCAPE:
                current += s[i];
                continue;
            }
            break;

        default:
            if (!addseps.empty() && addseps.find(s[i]) != string::npos) {
                switch (state) {
                case ESCAPE:
                    state = INQUOTE;
                    break;
                case INQUOTE:
                    break;
                case SPACE:
                    tokens.insert(tokens.end(), string(1, s[i]));
                    continue;
                case TOKEN:
                    tokens.insert(tokens.end(), current);
                    current.erase();
                    tokens.insert(tokens.end(), string(1, s[i]));
                    state = SPACE;
                    continue;
                }
            } else switch (state) {
                case ESCAPE:
                    state = INQUOTE;
                    break;
                case SPACE:
                    state = TOKEN;
                    break;
                case TOKEN:
                case INQUOTE:
                    break;
                }
            current += s[i];
        }
    }
    switch (state) {
    case SPACE:
        break;
    case TOKEN:
        tokens.insert(tokens.end(), current);
        break;
    case INQUOTE:
    case ESCAPE:
        return false;
    }
    return true;
}

template bool stringToStrings<list<string> >(const string&,
        list<string>&, const string&);
template bool stringToStrings<vector<string> >(const string&,
        vector<string>&, const string&);
template bool stringToStrings<set<string> >(const string&,
        set<string>&, const string&);
template bool stringToStrings<std::unordered_set<string> >
(const string&, std::unordered_set<string>&, const string&);

template <class T> void stringsToString(const T& tokens, string& s)
{
    for (typename T::const_iterator it = tokens.begin();
            it != tokens.end(); it++) {
        bool hasblanks = false;
        if (it->find_first_of(" \t\n") != string::npos) {
            hasblanks = true;
        }
        if (it != tokens.begin()) {
            s.append(1, ' ');
        }
        if (hasblanks) {
            s.append(1, '"');
        }
        for (unsigned int i = 0; i < it->length(); i++) {
            char car = it->at(i);
            if (car == '"') {
                s.append(1, '\\');
                s.append(1, car);
            } else {
                s.append(1, car);
            }
        }
        if (hasblanks) {
            s.append(1, '"');
        }
    }
}
template void stringsToString<list<string> >(const list<string>&, string&);
template void stringsToString<vector<string> >(const vector<string>&, string&);
template void stringsToString<set<string> >(const set<string>&, string&);
template void stringsToString<unordered_set<string> >(const unordered_set<string>&, string&);
template <class T> string stringsToString(const T& tokens)
{
    string out;
    stringsToString<T>(tokens, out);
    return out;
}
template string stringsToString<list<string> >(const list<string>&);
template string stringsToString<vector<string> >(const vector<string>&);
template string stringsToString<set<string> >(const set<string>&);
template string stringsToString<unordered_set<string> >(const unordered_set<string>&);

template <class T> void stringsToCSV(const T& tokens, string& s,
                                     char sep)
{
    s.erase();
    for (typename T::const_iterator it = tokens.begin();
            it != tokens.end(); it++) {
        bool needquotes = false;
        if (it->empty() ||
                it->find_first_of(string(1, sep) + "\"\n") != string::npos) {
            needquotes = true;
        }
        if (it != tokens.begin()) {
            s.append(1, sep);
        }
        if (needquotes) {
            s.append(1, '"');
        }
        for (unsigned int i = 0; i < it->length(); i++) {
            char car = it->at(i);
            if (car == '"') {
                s.append(2, '"');
            } else {
                s.append(1, car);
            }
        }
        if (needquotes) {
            s.append(1, '"');
        }
    }
}
template void stringsToCSV<list<string> >(const list<string>&, string&, char);
template void stringsToCSV<vector<string> >(const vector<string>&, string&,
        char);

void stringToTokens(const string& str, vector<string>& tokens,
                    const string& delims, bool skipinit)
{
    string::size_type startPos = 0, pos;

    // Skip initial delims, return empty if this eats all.
    if (skipinit &&
            (startPos = str.find_first_not_of(delims, 0)) == string::npos) {
        return;
    }
    while (startPos < str.size()) {
        // Find next delimiter or end of string (end of token)
        pos = str.find_first_of(delims, startPos);

        // Add token to the vector and adjust start
        if (pos == string::npos) {
            tokens.push_back(str.substr(startPos));
            break;
        } else if (pos == startPos) {
            // Dont' push empty tokens after first
            if (tokens.empty()) {
                tokens.push_back(string());
            }
            startPos = ++pos;
        } else {
            tokens.push_back(str.substr(startPos, pos - startPos));
            startPos = ++pos;
        }
    }
}

bool stringToBool(const string& s)
{
    if (s.empty()) {
        return false;
    }
    if (isdigit(s[0])) {
        int val = atoi(s.c_str());
        return val ? true : false;
    }
    if (s.find_first_of("yYtT") == 0) {
        return true;
    }
    return false;
}

void trimstring(string& s, const char *ws)
{
    rtrimstring(s, ws);
    ltrimstring(s, ws);
}

void rtrimstring(string& s, const char *ws)
{
    string::size_type pos = s.find_last_not_of(ws);
    if (pos != string::npos && pos != s.length() - 1) {
        s.replace(pos + 1, string::npos, string());
    }
}

void ltrimstring(string& s, const char *ws)
{
    string::size_type pos = s.find_first_not_of(ws);
    if (pos == string::npos) {
        s.clear();
        return;
    }
    s.replace(0, pos, string());
}

// Remove some chars and replace them with spaces
string neutchars(const string& str, const string& chars)
{
    string out;
    neutchars(str, out, chars);
    return out;
}
void neutchars(const string& str, string& out, const string& chars)
{
    string::size_type startPos, pos;

    for (pos = 0;;) {
        // Skip initial chars, break if this eats all.
        if ((startPos = str.find_first_not_of(chars, pos)) == string::npos) {
            break;
        }
        // Find next delimiter or end of string (end of token)
        pos = str.find_first_of(chars, startPos);
        // Add token to the output. Note: token cant be empty here
        if (pos == string::npos) {
            out += str.substr(startPos);
        } else {
            out += str.substr(startPos, pos - startPos) + " ";
        }
    }
}


/* Truncate a string to a given maxlength, avoiding cutting off midword
 * if reasonably possible. Note: we could also use textsplit, stopping when
 * we have enough, this would be cleanly utf8-aware but would remove
 * punctuation */
static const string cstr_SEPAR = " \t\n\r-:.;,/[]{}";
string truncate_to_word(const string& input, string::size_type maxlen)
{
    string output;
    if (input.length() <= maxlen) {
        output = input;
    } else {
        output = input.substr(0, maxlen);
        string::size_type space = output.find_last_of(cstr_SEPAR);
        // Original version only truncated at space if space was found after
        // maxlen/2. But we HAVE to truncate at space, else we'd need to do
        // utf8 stuff to avoid truncating at multibyte char. In any case,
        // not finding space means that the text probably has no value.
        // Except probably for Asian languages, so we may want to fix this
        // one day
        if (space == string::npos) {
            output.erase();
        } else {
            output.erase(space);
        }
    }
    return output;
}

// Escape things that would look like markup
string escapeHtml(const string& in)
{
    string out;
    for (string::size_type pos = 0; pos < in.length(); pos++) {
	switch(in.at(pos)) {
	case '<': out += "&lt;"; break;
	case '>': out += "&gt;"; break;
	case '&': out += "&amp;"; break;
	case '"': out += "&quot;"; break;
	default: out += in.at(pos); break;
	}
    }
    return out;
}

string escapeShell(const string& in)
{
    string out;
    out += "\"";
    for (string::size_type pos = 0; pos < in.length(); pos++) {
        switch (in.at(pos)) {
        case '$':
            out += "\\$";
            break;
        case '`':
            out += "\\`";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\\n";
            break;
        case '\\':
            out += "\\\\";
            break;
        default:
            out += in.at(pos);
        }
    }
    out += "\"";
    return out;
}

// Escape value to be suitable as C++ source double-quoted string (for
// generating a c++ program
string makeCString(const string& in)
{
    string out;
    out += "\"";
    for (string::size_type pos = 0; pos < in.length(); pos++) {
        switch (in.at(pos)) {
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\\':
            out += "\\\\";
            break;
        default:
            out += in.at(pos);
        }
    }
    out += "\"";
    return out;
}


// Substitute printf-like percent cmds inside a string
bool pcSubst(const string& in, string& out, const map<char, string>& subs)
{
    string::const_iterator it;
    for (it = in.begin(); it != in.end(); it++) {
        if (*it == '%') {
            if (++it == in.end()) {
                out += '%';
                break;
            }
            if (*it == '%') {
                out += '%';
                continue;
            }
            map<char, string>::const_iterator tr;
            if ((tr = subs.find(*it)) != subs.end()) {
                out += tr->second;
            } else {
                // We used to do "out += *it;" here but this does not make
                // sense
            }
        } else {
            out += *it;
        }
    }
    return true;
}

bool pcSubst(const string& in, string& out, const map<string, string>& subs)
{
    out.erase();
    string::size_type i;
    for (i = 0; i < in.size(); i++) {
        if (in[i] == '%') {
            if (++i == in.size()) {
                out += '%';
                break;
            }
            if (in[i] == '%') {
                out += '%';
                continue;
            }
            string key = "";
            if (in[i] == '(') {
                if (++i == in.size()) {
                    out += string("%(");
                    break;
                }
                string::size_type j = in.find_first_of(")", i);
                if (j == string::npos) {
                    // ??concatenate remaining part and stop
                    out += in.substr(i - 2);
                    break;
                }
                key = in.substr(i, j - i);
                i = j;
            } else {
                key = in[i];
            }
            map<string, string>::const_iterator tr;
            if ((tr = subs.find(key)) != subs.end()) {
                out += tr->second;
            } else {
                // Substitute to nothing, that's the reasonable thing to do
                // instead of keeping the %(key)
                // out += key.size()==1? key : string("(") + key + string(")");
            }
        } else {
            out += in[i];
        }
    }
    return true;
}
inline static int ulltorbuf(uint64_t val, char *rbuf)
{
    int idx;
    for (idx = 0; val; idx++) {
        rbuf[idx] = '0' + val % 10;
        val /= 10;
    }
    while (val);
    rbuf[idx] = 0;
    return idx;
}

inline static void ullcopyreverse(const char *rbuf, string& buf, int idx)
{
    buf.reserve(idx + 1);
    for (int i = idx - 1; i >= 0; i--) {
        buf.push_back(rbuf[i]);
    }
}

void ulltodecstr(uint64_t val, string& buf)
{
    buf.clear();
    if (val == 0) {
        buf = "0";
        return;
    }

    char rbuf[30];
    int idx = ulltorbuf(val, rbuf);

    ullcopyreverse(rbuf, buf, idx);
    return;
}

void lltodecstr(int64_t val, string& buf)
{
    buf.clear();
    if (val == 0) {
        buf = "0";
        return;
    }

    bool neg = val < 0;
    if (neg) {
        val = -val;
    }

    char rbuf[30];
    int idx = ulltorbuf(val, rbuf);

    if (neg) {
        rbuf[idx++] = '-';
    }
    rbuf[idx] = 0;

    ullcopyreverse(rbuf, buf, idx);
    return;
}

string lltodecstr(int64_t val)
{
    string buf;
    lltodecstr(val, buf);
    return buf;
}

string ulltodecstr(uint64_t val)
{
    string buf;
    ulltodecstr(val, buf);
    return buf;
}

// Convert byte count into unit (KB/MB...) appropriate for display
string displayableBytes(int64_t size)
{
    const char *unit;

    double roundable = 0;
    if (size < 1000) {
        unit = " B ";
        roundable = double(size);
    } else if (size < 1E6) {
        unit = " KB ";
        roundable = double(size) / 1E3;
    } else if (size < 1E9) {
        unit = " MB ";
        roundable = double(size) / 1E6;
    } else {
        unit = " GB ";
        roundable = double(size) / 1E9;
    }
    size = int64_t(round(roundable));
    return lltodecstr(size).append(unit);
}

string breakIntoLines(const string& in, unsigned int ll,
                      unsigned int maxlines)
{
    string query = in;
    string oq;
    unsigned int nlines = 0;
    while (query.length() > 0) {
        string ss = query.substr(0, ll);
        if (ss.length() == ll) {
            string::size_type pos = ss.find_last_of(" ");
            if (pos == string::npos) {
                pos = query.find_first_of(" ");
                if (pos != string::npos) {
                    ss = query.substr(0, pos + 1);
                } else {
                    ss = query;
                }
            } else {
                ss = ss.substr(0, pos + 1);
            }
        }
        // This cant happen, but anyway. Be very sure to avoid an infinite loop
        if (ss.length() == 0) {
            oq = query;
            break;
        }
        oq += ss + "\n";
        if (nlines++ >= maxlines) {
            oq += " ... \n";
            break;
        }
        query = query.substr(ss.length());
    }
    return oq;
}

// Date is Y[-M[-D]]
static bool parsedate(vector<string>::const_iterator& it,
                      vector<string>::const_iterator end, DateInterval *dip)
{
    dip->y1 = dip->m1 = dip->d1 = dip->y2 = dip->m2 = dip->d2 = 0;
    if (it->length() > 4 || !it->length() ||
            it->find_first_not_of("0123456789") != string::npos) {
        return false;
    }
    if (it == end || sscanf(it++->c_str(), "%d", &dip->y1) != 1) {
        return false;
    }
    if (it == end || *it == "/") {
        return true;
    }
    if (*it++ != "-") {
        return false;
    }

    if (it->length() > 2 || !it->length() ||
            it->find_first_not_of("0123456789") != string::npos) {
        return false;
    }
    if (it == end || sscanf(it++->c_str(), "%d", &dip->m1) != 1) {
        return false;
    }
    if (it == end || *it == "/") {
        return true;
    }
    if (*it++ != "-") {
        return false;
    }

    if (it->length() > 2 || !it->length() ||
            it->find_first_not_of("0123456789") != string::npos) {
        return false;
    }
    if (it == end || sscanf(it++->c_str(), "%d", &dip->d1) != 1) {
        return false;
    }

    return true;
}

// Called with the 'P' already processed. Period ends at end of string
// or at '/'. We dont' do a lot effort at validation and will happily
// accept 10Y1Y4Y (the last wins)
static bool parseperiod(vector<string>::const_iterator& it,
                        vector<string>::const_iterator end, DateInterval *dip)
{
    dip->y1 = dip->m1 = dip->d1 = dip->y2 = dip->m2 = dip->d2 = 0;
    while (it != end) {
        int value;
        if (it->find_first_not_of("0123456789") != string::npos) {
            return false;
        }
        if (sscanf(it++->c_str(), "%d", &value) != 1) {
            return false;
        }
        if (it == end || it->empty()) {
            return false;
        }
        switch (it->at(0)) {
        case 'Y':
        case 'y':
            dip->y1 = value;
            break;
        case 'M':
        case 'm':
            dip->m1 = value;
            break;
        case 'D':
        case 'd':
            dip->d1 = value;
            break;
        default:
            return false;
        }
        it++;
        if (it == end) {
            return true;
        }
        if (*it == "/") {
            return true;
        }
    }
    return true;
}

#ifdef _WIN32
int setenv(const char *name, const char *value, int overwrite)
{
    if (!overwrite) {
        const char *cp = getenv(name);
        if (cp) {
            return -1;
        }
    }
    return _putenv_s(name, value);
}
void unsetenv(const char *name)
{
    _putenv_s(name, "");
}
#endif

time_t portable_timegm(struct tm *tm)
{
    time_t ret;
    char *tz;

    tz = getenv("TZ");
    setenv("TZ", "", 1);
    tzset();
    ret = mktime(tm);
    if (tz) {
        setenv("TZ", tz, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();
    return ret;
}

#if 0
static void cerrdip(const string& s, DateInterval *dip)
{
    cerr << s << dip->y1 << "-" << dip->m1 << "-" << dip->d1 << "/"
         << dip->y2 << "-" << dip->m2 << "-" << dip->d2
         << endl;
}
#endif

// Compute date + period. Won't work out of the unix era.
// or pre-1970 dates. Just convert everything to unixtime and
// seconds (with average durations for months/years), add and convert
// back
static bool addperiod(DateInterval *dp, DateInterval *pp)
{
    struct tm tm;
    // Create a struct tm with possibly non normalized fields and let
    // timegm sort it out
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = dp->y1 - 1900 + pp->y1;
    tm.tm_mon = dp->m1 + pp->m1 - 1;
    tm.tm_mday = dp->d1 + pp->d1;
    time_t tres = mktime(&tm);
    localtime_r(&tres, &tm);
    dp->y1 = tm.tm_year + 1900;
    dp->m1 = tm.tm_mon + 1;
    dp->d1 = tm.tm_mday;
    //cerrdip("Addperiod return", dp);
    return true;
}
int monthdays(int mon, int year)
{
    switch (mon) {
    // We are returning a few too many 29 days februaries, no problem
    case 2:
        return (year % 4) == 0 ? 29 : 28;
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
        return 31;
    default:
        return 30;
    }
}
bool parsedateinterval(const string& s, DateInterval *dip)
{
    vector<string> vs;
    dip->y1 = dip->m1 = dip->d1 = dip->y2 = dip->m2 = dip->d2 = 0;
    DateInterval p1, p2, d1, d2;
    p1 = p2 = d1 = d2 = *dip;
    bool hasp1 = false, hasp2 = false, hasd1 = false, hasd2 = false,
         hasslash = false;

    if (!stringToStrings(s, vs, "PYMDpymd-/")) {
        return false;
    }
    if (vs.empty()) {
        return false;
    }

    vector<string>::const_iterator it = vs.begin();
    if (*it == "P" || *it == "p") {
        it++;
        if (!parseperiod(it, vs.end(), &p1)) {
            return false;
        }
        hasp1 = true;
        //cerrdip("p1", &p1);
        p1.y1 = -p1.y1;
        p1.m1 = -p1.m1;
        p1.d1 = -p1.d1;
    } else if (*it == "/") {
        hasslash = true;
        goto secondelt;
    } else {
        if (!parsedate(it, vs.end(), &d1)) {
            return false;
        }
        hasd1 = true;
    }

    // Got one element and/or /
secondelt:
    if (it != vs.end()) {
        if (*it != "/") {
            return false;
        }
        hasslash = true;
        it++;
        if (it == vs.end()) {
            // ok
        } else if (*it == "P" || *it == "p") {
            it++;
            if (!parseperiod(it, vs.end(), &p2)) {
                return false;
            }
            hasp2 = true;
        } else {
            if (!parsedate(it, vs.end(), &d2)) {
                return false;
            }
            hasd2 = true;
        }
    }

    // 2 periods dont' make sense
    if (hasp1 && hasp2) {
        return false;
    }
    // Nothing at all doesn't either
    if (!hasp1 && !hasd1 && !hasp2 && !hasd2) {
        return false;
    }

    // Empty part means today IF other part is period, else means
    // forever (stays at 0)
    time_t now = time(0);
    struct tm *tmnow = gmtime(&now);
    if ((!hasp1 && !hasd1) && hasp2) {
        d1.y1 = 1900 + tmnow->tm_year;
        d1.m1 = tmnow->tm_mon + 1;
        d1.d1 = tmnow->tm_mday;
        hasd1 = true;
    } else if ((!hasp2 && !hasd2) && hasp1) {
        d2.y1 = 1900 + tmnow->tm_year;
        d2.m1 = tmnow->tm_mon + 1;
        d2.d1 = tmnow->tm_mday;
        hasd2 = true;
    }

    // Incomplete dates have different meanings depending if there is
    // a period or not (actual or infinite indicated by a / + empty)
    //
    // If there is no explicit period, an incomplete date indicates a
    // period of the size of the uncompleted elements. Ex: 1999
    // actually means 1999/P12M
    //
    // If there is a period, the incomplete date should be extended
    // to the beginning or end of the unspecified portion. Ex: 1999/
    // means 1999-01-01/ and /1999 means /1999-12-31
    if (hasd1) {
        if (!(hasslash || hasp2)) {
            if (d1.m1 == 0) {
                p2.m1 = 12;
                d1.m1 = 1;
                d1.d1 = 1;
            } else if (d1.d1 == 0) {
                d1.d1 = 1;
                p2.d1 = monthdays(d1.m1, d1.y1);
            }
            hasp2 = true;
        } else {
            if (d1.m1 == 0) {
                d1.m1 = 1;
                d1.d1 = 1;
            } else if (d1.d1 == 0) {
                d1.d1 = 1;
            }
        }
    }
    // if hasd2 is true we had a /
    if (hasd2) {
        if (d2.m1 == 0) {
            d2.m1 = 12;
            d2.d1 = 31;
        } else if (d2.d1 == 0) {
            d2.d1 = monthdays(d2.m1, d2.y1);
        }
    }
    if (hasp1) {
        // Compute d1
        d1 = d2;
        if (!addperiod(&d1, &p1)) {
            return false;
        }
    } else if (hasp2) {
        // Compute d2
        d2 = d1;
        if (!addperiod(&d2, &p2)) {
            return false;
        }
    }

    dip->y1 = d1.y1;
    dip->m1 = d1.m1;
    dip->d1 = d1.d1;
    dip->y2 = d2.y1;
    dip->m2 = d2.m1;
    dip->d2 = d2.d1;
    return true;
}


void catstrerror(string *reason, const char *what, int _errno)
{
    if (!reason) {
        return;
    }
    if (what) {
        reason->append(what);
    }

    reason->append(": errno: ");

    char nbuf[20];
    sprintf(nbuf, "%d", _errno);
    reason->append(nbuf);

    reason->append(" : ");

#if defined(sun) || defined(_WIN32)
    // Note: sun strerror is noted mt-safe ??
    reason->append(strerror(_errno));
#else
#define ERRBUFSZ 200
    char errbuf[ERRBUFSZ];
    // There are 2 versions of strerror_r.
    // - The GNU one returns a pointer to the message (maybe
    //   static storage or supplied buffer).
    // - The POSIX one always stores in supplied buffer and
    //   returns 0 on success. As the possibility of error and
    //   error code are not specified, we're basically doomed
    //   cause we can't use a test on the 0 value to know if we
    //   were returned a pointer...
    // Also couldn't find an easy way to disable the gnu version without
    // changing the cxxflags globally, so forget it. Recent gnu lib versions
    // normally default to the posix version.
    // At worse we get no message at all here.
    errbuf[0] = 0;
    // We don't use ret, it's there to silence a cc warning
    auto ret = strerror_r(_errno, errbuf, ERRBUFSZ);
    (void)ret;
    reason->append(errbuf);
#endif
}


static const char *vlang_to_code[] = {
    "be", "cp1251",
    "bg", "cp1251",
    "cs", "iso-8859-2",
    "el", "iso-8859-7",
    "he", "iso-8859-8",
    "hr", "iso-8859-2",
    "hu", "iso-8859-2",
    "ja", "eucjp",
    "kk", "pt154",
    "ko", "euckr",
    "lt", "iso-8859-13",
    "lv", "iso-8859-13",
    "pl", "iso-8859-2",
    "rs", "iso-8859-2",
    "ro", "iso-8859-2",
    "ru", "koi8-r",
    "sk", "iso-8859-2",
    "sl", "iso-8859-2",
    "sr", "iso-8859-2",
    "th", "iso-8859-11",
    "tr", "iso-8859-9",
    "uk", "koi8-u",
};

static const string cstr_cp1252("CP1252");

string langtocode(const string& lang)
{
    static std::unordered_map<string, string> lang_to_code;
    if (lang_to_code.empty()) {
        for (unsigned int i = 0;
                i < sizeof(vlang_to_code) / sizeof(char *); i += 2) {
            lang_to_code[vlang_to_code[i]] = vlang_to_code[i + 1];
        }
    }
    std::unordered_map<string, string>::const_iterator it =
        lang_to_code.find(lang);

    // Use cp1252 by default...
    if (it == lang_to_code.end()) {
        return cstr_cp1252;
    }

    return it->second;
}

string localelang()
{
    const char *lang = getenv("LANG");

    if (lang == 0 || *lang == 0 || !strcmp(lang, "C") ||
            !strcmp(lang, "POSIX")) {
        return "en";
    }
    string locale(lang);
    string::size_type under = locale.find_first_of("_");
    if (under == string::npos) {
        return locale;
    }
    return locale.substr(0, under);
}

#ifdef USE_STD_REGEX

class SimpleRegexp::Internal {
public:
    Internal(const string& exp, int flags, int nm)
        : expr(exp,
               basic_regex<char>::flag_type(regex_constants::extended |
                   ((flags&SRE_ICASE) ? regex_constants::icase : 0) |
                   ((flags&SRE_NOSUB) ? regex_constants::nosubs : 0)
                   )), ok(true), nmatch(nm) {
    }
    std::regex expr;
    std::smatch res;
    bool ok;
    int nmatch;
};

bool SimpleRegexp::simpleMatch(const string& val) const
{
    if (!ok())
        return false;
    return regex_match(val, m->res, m->expr);
}

string SimpleRegexp::getMatch(const string& val, int i) const
{
    return m->res.str(i);
}

#else // -> !WIN32

class SimpleRegexp::Internal {
public:
    Internal(const string& exp, int flags, int nm) : nmatch(nm) {
        if (regcomp(&expr, exp.c_str(), REG_EXTENDED |
                    ((flags&SRE_ICASE) ? REG_ICASE : 0) |
                    ((flags&SRE_NOSUB) ? REG_NOSUB : 0)) == 0) {
            ok = true;
        } else {
            ok = false;
        }
        matches.reserve(nmatch+1);
    }
    ~Internal() {
        regfree(&expr);
    }
    bool ok;
    regex_t expr;
    int nmatch;
    vector<regmatch_t> matches;
};

bool SimpleRegexp::simpleMatch(const string& val) const
{
    if (!ok())
        return false;
    if (regexec(&m->expr, val.c_str(), m->nmatch+1, &m->matches[0], 0) == 0) {
        return true;
    } else {
        return false;
    }
}

string SimpleRegexp::getMatch(const string& val, int i) const
{
    if (i > m->nmatch) {
        return string();
    }
    return val.substr(m->matches[i].rm_so,
                      m->matches[i].rm_eo - m->matches[i].rm_so);
}

#endif // win/notwinf

SimpleRegexp::SimpleRegexp(const string& exp, int flags, int nmatch)
    : m(new Internal(exp, flags, nmatch))
{
}

SimpleRegexp::~SimpleRegexp()
{
    delete m;
}

bool SimpleRegexp::ok() const
{
    return m->ok;
}

bool SimpleRegexp::operator() (const string& val) const
{
    return simpleMatch(val);
}

string flagsToString(const vector<CharFlags>& flags, unsigned int val)
{
    const char *s;
    string out;
    for (auto& flag : flags) {
        if ((val & flag.value) == flag.value) {
            s = flag.yesname;
        } else {
            s = flag.noname;
        }
        if (s && *s) {
            /* We have something to write */
            if (out.length()) {
                // If not first, add '|' separator
                out.append("|");
            }
            out.append(s);
        }
    }
    return out;
}

string valToString(const vector<CharFlags>& flags, unsigned int val)
{
    string out;
    for (auto& flag : flags) {
        if (flag.value == val) {
            out = flag.yesname;
            return out;
        }
    }
    {
        char mybuf[100];
        sprintf(mybuf, "Unknown Value 0x%x", val);
        out = mybuf;
    }
    return out;
}

unsigned int stringToFlags(const vector<CharFlags>& flags,
                           const string& input, const char *sep)
{
    unsigned int out = 0;

    vector<string> toks;
    stringToTokens(input, toks, sep);
    for (auto& tok: toks) {
        trimstring(tok);
        for (auto& flag : flags) {
            if (!tok.compare(flag.yesname)) {
                /* Note: we don't break: the same name could conceivably
                   set several flags. */
                out |= flag.value;
            }
        }
    }
    return out;
}


// Initialization for static stuff to be called from main thread before going
// multiple
void smallut_init_mt()
{
    // Init langtocode() static table
    langtocode("");
}
