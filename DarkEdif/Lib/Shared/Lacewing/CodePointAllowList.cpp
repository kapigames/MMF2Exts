#include "Lacewing.h"
#include <deps/utf8proc.h>

std::string lacewing::codepointsallowlist::setcodepointsallowedlist(std::string acStr)
{
	if (acStr.empty())
	{
		codePointCategories.clear();
		specificCodePoints.clear();
		codePointRanges.clear();
		allAllowed = true;
		list = acStr;
		return std::string();
	}

	codepointsallowlist acTemp = *this;

	// Reset to blank
	codePointCategories.clear();
	specificCodePoints.clear();
	codePointRanges.clear();
	allAllowed = false;
	list = acStr;

	const auto makeError = [this, &acTemp](const char * str, ...)
	{
		va_list v, v2;
		va_start(v, str);
		va_copy(v2, v);

		size_t numChars = vsnprintf(nullptr, 0, str, v);
		std::string error(numChars, ' ');
		vsprintf(error.data(), str, v2);
		MessageBoxA(NULL, error.c_str(), "Error!", MB_ICONERROR);

		va_end(v);
		va_end(v2);
		*this = acTemp; // restore old
		return error;
	};

	// String should be format:
	// 2 letters, or 1 letter + *, or an integer number that is the UTF32 number of char
	if (acStr.front() == ',')
		return makeError("The acceptable code point list \"%hs...\" starts with a comma.", acStr.c_str());

	acStr.erase(std::remove(acStr.begin(), acStr.end(), ' '), acStr.end());
	if (acStr.empty())
		return makeError("The acceptable code point list \"%hs\" is all spaces.", acStr.c_str());

	if (acStr.back() == ',')
		return makeError("The acceptable code point list \"%hs...\" ends with a comma.", acStr.c_str());

	if (acStr.find(",,"sv) != std::string::npos)
		return makeError("The acceptable code point list \"%hs...\" contains \",,\".", acStr.c_str());

	acStr += ','; // to make sure when cur is +='d and passes end of string, it'll end with remaining = 0

	const char * cur = acStr.data();
	size_t remaining = acStr.size();
	while (true)
	{
		remaining = (acStr.data() + acStr.size()) - cur;
		if (cur >= acStr.data() + acStr.size() - 1)
			break;

		// Two-letter category, or letter + * for all categories
		if (remaining >= 2 &&
			std::isalpha(cur[0]) && (std::isalpha(cur[1]) || cur[1] == '*'))
		{
			// more than two letters
			if (remaining > 2 && cur[2] != ',')
			{
				return makeError("The acceptable code point list \"%hs\" has a 3+ letter category \"%hs\". Categories are 2 letters.",
					acStr.c_str(), cur);
			}

			// See utf8proc.cpp for these defined under utf8proc_category_t
			static const char categoryList[][3] = { "Cn","Lu","Ll","Lt","Lm","Lo","Mn","Mc","Me","Nd","Nl","No","Pc","Pd","Ps","Pe","Pi","Pf","Po","Sm","Sc","Sk","So","Zs","Zl","Zp","Cc","Cf","Cs","Co" };
			static const char wildcardCategory[] = { 'C', 'L', 'M', 'N', 'P','S','Z' };

			bool found = false;
			// Wildcard
			if (cur[1] == '*')
			{
				char firstCharUpper = std::toupper(cur[0]);
				for (size_t i = 0; i < sizeof(wildcardCategory); i++)
				{
					if (firstCharUpper == wildcardCategory[i])
					{
						// Wildcard category found, yay
						for (size_t j = 0; j < std::size(categoryList); j++) {
							if (firstCharUpper == categoryList[j][0])
								codePointCategories.push_back(j);
						}

						cur += 3;
						goto nextChar;
					}
				}

				return makeError("Wildcard category \"%.2hs\" not recognised. Check the help file.", cur);
			}

			for (size_t i = 0; i < std::size(categoryList); i++)
			{
				if (std::toupper(cur[0]) == categoryList[i][0] &&
					std::tolower(cur[1]) == categoryList[i][1])
				{
					// Category found, is it already added?
					if (std::find(codePointCategories.cbegin(), codePointCategories.cend(), i) != codePointCategories.cend())
						return makeError("Category \"%.2hs\" was added twice in list \"%hs\".", cur, acStr.c_str());

					codePointCategories.push_back(i);
					cur += 3;
					goto nextChar;
				}
			}

			return makeError("Category \"%.2hs\" not recognised. Check the help file.", cur);
		}

		// Numeric, or numeric range expected
		if (std::isdigit(cur[0])) {
			char * endPtr;
			unsigned long codePointAllowed = std::strtoul(cur, &endPtr, 0);
			if (codePointAllowed == 0 || codePointAllowed > MAXINT32) // error in strtoul, or user has put in 0 and approved null char, either way bad
				return makeError("Specific codepoint %hs not a valid codepoint.", cur, acStr.c_str());

			// Single code point, after this it's a new Unicode list, or it's end of string
			cur = endPtr;
			if (cur[0] == '\0' || cur[0] == ',')
			{
				if (std::find(specificCodePoints.cbegin(), specificCodePoints.cend(), codePointAllowed) != specificCodePoints.cend())
					return makeError("Specific codepoint %lu was added twice in list \"%hs\".", codePointAllowed, acStr.c_str());

				specificCodePoints.push_back(codePointAllowed);
				if (cur[0] == ',')
					++cur;
				goto nextChar;
			}

			// Range of code points
			if (cur[0] == '-')
			{
				++cur;
				unsigned long lastCodePointNum = std::strtoul(cur, &endPtr, 0);
				if (lastCodePointNum == 0 || lastCodePointNum > MAXINT32) // error in strtoul, or user has put in 0 and approved null char, either way bad
					return makeError("Ending number in codepoint range %lu to \"%.15hs...\" could not be read.", codePointAllowed, cur, cur);
				// Range is reversed
				if (lastCodePointNum < codePointAllowed)
					return makeError("Range %lu to %lu is backwards.", codePointAllowed, lastCodePointNum);

				// Allow range overlaps - we could search by range1 max > range2 min, but we won't.
				// We will check for an exact match in range, though.

				auto range = std::make_pair((std::int32_t)codePointAllowed, (std::int32_t)lastCodePointNum);
				if (std::find(codePointRanges.cbegin(), codePointRanges.cend(), range) != codePointRanges.cend())
					return makeError("Range %lu to %lu is in the list twice.", codePointAllowed, lastCodePointNum);

				codePointRanges.push_back(range);
				cur = endPtr + 1; // skip the ','
				goto nextChar;
			}

			// fall through
		}

		return makeError("Unrecognised character list starting at \"%.15hs\".", cur);

	nextChar:
		/* go to next char */;
	}

	return std::string();
}

int lacewing::codepointsallowlist::checkcodepointsallowed(const std::string_view toTest, int * const rejectedUTF32CodePoint /* = NULL */) const
{
	if (allAllowed)
		return -1;

	const utf8proc_uint8_t * str = (const utf8proc_uint8_t *)toTest.data();
	utf8proc_int32_t thisChar;
	utf8proc_ssize_t numBytesInCodePoint, remainingBytes = toTest.size();
	int codePointIndex = 0;
	while (remainingBytes > 0)
	{
		numBytesInCodePoint = utf8proc_iterate(str, remainingBytes, &thisChar);
		if (numBytesInCodePoint <= 0 || !utf8proc_codepoint_valid(thisChar))
			goto badChar;

		if (std::find(specificCodePoints.cbegin(), specificCodePoints.cend(), thisChar) != specificCodePoints.cend())
			goto goodChar;
		if (std::find_if(codePointRanges.cbegin(), codePointRanges.cend(),
			[=](const std::pair<std::int32_t, std::int32_t> & range) {
				return thisChar >= range.first && thisChar <= range.second;
			}) != codePointRanges.cend())
		{
			goto goodChar;
		}
		utf8proc_category_t category = utf8proc_category(thisChar);
		if (std::find(codePointCategories.cbegin(), codePointCategories.cend(), category) != codePointCategories.cend())
			goto goodChar;

		// ... fall through from above
	badChar:
		if (rejectedUTF32CodePoint != NULL)
			*rejectedUTF32CodePoint = thisChar;
		return codePointIndex;

		// Loop around
	goodChar:
		++codePointIndex;
		str += numBytesInCodePoint;
		remainingBytes -= numBytesInCodePoint;
	}

	return -1; // All good
}
