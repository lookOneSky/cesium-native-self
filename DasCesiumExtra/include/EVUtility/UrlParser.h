#pragma once
#include <string>
#include <regex>
#include <utility>

namespace EVUtility {

class UrlParser
{
public:
	UrlParser(const std::string& url)
	{
		std::regex reg("\\{(\\$[^\\{\\}]*)\\}");

		std::smatch m;

		std::string url2 = url.c_str();
		auto pos = url2.cbegin();
		auto end = url2.cend();
		for (; std::regex_search(pos, end, m, reg); pos = m.suffix().first)
		{
			mvExpression.push_back(std::make_pair(m.prefix(), false));
			mvExpression.push_back(std::make_pair(m.str(1), true));
		}
		if (pos != end) {
			mvExpression.push_back(std::make_pair(std::string(pos, end), false));
		}
	}
	std::string toStr(int v)
	{
		std::string str = std::to_string(v);
		return str;
	}
	std::string parseExpression(const std::string& exp, const std::string& serverpart, int level, int row, int col)
	{
		std::string ret;
		std::regex reg("\\$(\\w*)");
		std::smatch m;

		auto pos = exp.cbegin();
		auto end = exp.cend();
		for (; std::regex_search(pos, end, m, reg); pos = m.suffix().first)
		{
			ret += m.prefix();
			std::string mm = m.str(1);
			if (mm == "x")
			{
				ret += toStr(col);
			}
			else if (mm == "y")
			{
				ret += toStr(row);
			}
			else if (mm == "z")
			{
				ret += toStr(level);
			}
			else if (mm == "serverpart")
			{
				ret += serverpart.c_str();
			}
			else
			{
				ret += mm;
			}
		}
		ret += m.suffix();

		return ret.c_str();
	}

	std::string getUrl(const std::string& serverpart, int level, int row, int col)
	{
		std::string ret;
		for (int i = 0, c = mvExpression.size(); i < c; ++i)
		{
			std::pair<std::string, bool>& ele = mvExpression[i];
			if (ele.second)
				ret += parseExpression(ele.first, serverpart, level, row, col);
			else
				ret += ele.first.c_str();
		}

		return ret;
	}

	std::vector<std::pair<std::string, bool> > mvExpression;
};

}
