#pragma once
#include<string>
namespace EVUtility {
	//std::string WStringToString(const std::wstring wstr);
	//std::wstring StringToWString(const std::string& str);
    std::string UrlEncode(const std::string& str);
    std::string UrlDecode(const std::string& str);
    std::string UnicodeToUtf8(const std::string& strSrc);
    std::string Utf8ToMultibyte(const std::string& strSrc);
    std::string MultibyteToUtf8(const std::string& strSrc);
    std::string resolveForEVTile(
        const std::string& base,
        const std::string& relative);
    std::string formatEVLayerUrl(
        std::string& base,
        std::string& headInfoUrl,
        const std::string& suffix);
    } // namespace Utility
